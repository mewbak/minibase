#include <bits/socket/unix.h>
#include <bits/ioctl/socket.h>
#include <bits/errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <nlusctl.h>
#include <string.h>
#include <format.h>
#include <util.h>
#include <main.h>
#include <netlink.h>
#include <netlink/rtnl/link.h>
#include <netlink/rtnl/addr.h>
#include <netlink/rtnl/route.h>
#include <netlink/rtnl/mgrp.h>

#include "common.h"
#include "ifctl.h"

ERRTAG("ifctl");
ERRLIST(NENOENT NEINVAL NENOSYS NENOENT NEACCES NEPERM NEBUSY NEALREADY
	NENETDOWN NENOKEY NENOTCONN NENODEV NETIMEDOUT);

static void init_socket(CTX)
{
	int fd;

	uc_buf_set(&ctx->uc, ctx->txbuf, sizeof(ctx->txbuf));
	ur_buf_set(&ctx->ur, ctx->rxbuf, sizeof(ctx->rxbuf));

	if((fd = sys_socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		fail("socket", "AF_UNIX", fd);

	ctx->fd = fd;
}

static void connect_socket(CTX)
{
	int ret;

	struct sockaddr_un addr = {
		.family = AF_UNIX,
		.path = IFCTL
	};

	if((ret = sys_connect(ctx->fd, &addr, sizeof(addr))) < 0)
		fail(NULL, addr.path, ret);

	ctx->connected = 1;
}

static void resolve_device(CTX)
{
	struct ifreq ifreq;
	char* name = ctx->name;
	int nlen = strlen(name);
	int ret;

	if(nlen > IFNAMESIZ)
		fail("name too long:", name, 0);

	memzero(&ifreq, sizeof(ifreq));
	memcpy(ifreq.name, name, nlen);

	if((ret = sys_ioctl(ctx->fd, SIOCGIFINDEX, &ifreq)) < 0)
		fail("ioctl", "SIOCGIFINDEX", ret);

	ctx->ifi = ifreq.ival;
}


/* Wire utils */

void send_command(CTX)
{
	int wr, fd = ctx->fd;

	if(!ctx->connected)
		connect_socket(ctx);
	if((wr = uc_send_whole(fd, &ctx->uc)) < 0)
		fail("write", NULL, wr);
}

struct ucmsg* recv_reply(CTX)
{
	struct urbuf* ur = &ctx->ur;
	int ret, fd = ctx->fd;

	if((ret = uc_recv_shift(fd, ur)) < 0)
		return NULL;

	return ur->msg;
}

static struct ucmsg* send_recv_msg(CTX)
{
	struct ucmsg* msg;

	send_command(ctx);

	while((msg = recv_reply(ctx)))
		if(msg->cmd <= 0)
			return msg;

	fail("connection lost", NULL, 0);
}

static int send_recv_cmd(CTX)
{
	struct ucmsg* msg = send_recv_msg(ctx);

	return msg->cmd;
}

static void send_check(CTX)
{
	int ret;

	if((ret = send_recv_cmd(ctx)) < 0)
		fail(NULL, NULL, ret);
	else if(ret > 0)
		fail("unexpected reply", NULL, ret);
}

static void fail_exit_code(char* script, int code)
{
	FMTBUF(p, e, buf, 100);
	p = fmtstr(p, e, script);
	p = fmtstr(p, e, " failed with code ");
	p = fmthex(p, e, code);
	FMTENL(p, e);

	fail(buf, NULL, 0);
}

static void drop_link(CTX)
{
	uc_put_hdr(UC, CMD_IF_DROP);
	uc_put_int(UC, ATTR_IFI, ctx->ifi);
	uc_put_end(UC);

	send_check(ctx);
}

static void wait_for_mode(CTX)
{
	struct ucmsg* msg;

	while((msg = recv_reply(ctx))) {
		if(msg->cmd != REP_IF_MODE)
			continue;

		int* errno = uc_get_int(msg, ATTR_ERRNO);
		int* xcode = uc_get_int(msg, ATTR_XCODE);

		if(errno || xcode)
			drop_link(ctx);
		if(errno)
			fail(NULL, "mode", *errno);
		if(xcode)
			fail_exit_code("mode", *xcode);

		break;
	}
}

static void wait_for_stop(CTX)
{
	struct ucmsg* msg;
	int* code;

	while((msg = recv_reply(ctx))) {
		if(msg->cmd != REP_IF_STOP)
			continue;

		if((code = uc_get_int(msg, ATTR_ERRNO)))
			fail(NULL, "stop", *code);
		if((code = uc_get_int(msg, ATTR_XCODE)))
			fail_exit_code("stop", *code);

		break;
	}
}

/* Cmdline arguments */

static void no_other_options(CTX)
{
	if(ctx->argi < ctx->argc)
		fail("too many arguments", NULL, 0);
	if(ctx->opts)
		fail("bad options", NULL, 0);
}

static char* shift_arg(CTX)
{
	if(ctx->argi >= ctx->argc)
		fail("too few arguments", NULL, 0);

	return ctx->argv[ctx->argi++];
}

static int show_status(CTX)
{
	no_other_options(ctx);
	init_socket(ctx);

	uc_put_hdr(UC, CMD_IF_STATUS);
	uc_put_end(UC);

	struct ucmsg* msg;

	if(!(msg = send_recv_msg(ctx)))
		fail("connection lost", NULL, 0);
	if(msg->cmd < 0)
		fail(NULL, NULL, msg->cmd);

	dump_status(ctx, msg);

	return 0;
}

static void req_show_id(CTX)
{
	identify_device(ctx);

	if(!ctx->devid[0])
		fail("cannot figure out persistent id for", ctx->name, 0);

	FMTBUF(p, e, out, 100);
	p = fmtstr(p, e, ctx->devid);
	FMTENL(p, e);

	writeall(STDOUT, out, p - out);
}

static void set_active_mode(CTX, char* mode)
{
	uc_put_hdr(UC, CMD_IF_MODE);
	uc_put_int(UC, ATTR_IFI, ctx->ifi);
	uc_put_str(UC, ATTR_NAME, ctx->name);
	uc_put_str(UC, ATTR_MODE, mode);
	uc_put_end(UC);

	send_check(ctx);
}

static void req_identify(CTX)
{
	char mode[MODESIZE];

	identify_device(ctx);

	if(!ctx->devid[0])
		return;

	load_device_mode(ctx, mode, sizeof(mode));

	if(!mode[0])
		return;

	uc_put_hdr(UC, CMD_IF_IDMODE);
	uc_put_int(UC, ATTR_IFI, ctx->ifi);
	uc_put_str(UC, ATTR_NAME, ctx->name);
	uc_put_str(UC, ATTR_MODE, mode);
	uc_put_end(UC);

	send_check(ctx);
}

static void check_no_active(CTX, char* mode)
{
	struct ucmsg* msg;
	struct ucattr* at;
	char* lnmode;
	char* lnname;

	uc_put_hdr(UC, CMD_IF_STATUS);
	uc_put_end(UC);

	if(!(msg = send_recv_msg(ctx)))
		fail("connection lost", NULL, 0);
	if(msg->cmd < 0)
		fail(NULL, NULL, msg->cmd);

	for(at = uc_get_0(msg); at; at = uc_get_n(msg, at)) {
		if(!uc_is_nest(at, ATTR_LINK))
			continue;
		if(!(lnmode = uc_sub_str(at, ATTR_MODE)))
			continue;
		if(!(lnname = uc_sub_str(at, ATTR_NAME)))
			continue;
		if(strcmp(lnmode, mode))
			continue;

		fail("already set on", lnname, 0);
	}
}

static void req_mode(CTX)
{
	char* mode = shift_arg(ctx);

	no_other_options(ctx);

	identify_device(ctx);

	check_no_active(ctx, mode);
	set_active_mode(ctx, mode);
	wait_for_mode(ctx);

	store_device_mode(ctx, mode);
}

static void req_also(CTX)
{
	char* mode = shift_arg(ctx);

	no_other_options(ctx);

	identify_device(ctx);

	set_active_mode(ctx, mode);
	wait_for_mode(ctx);

	store_device_also(ctx, mode);
}

static void req_stop(CTX)
{
	no_other_options(ctx);

	identify_device(ctx);

	uc_put_hdr(UC, CMD_IF_STOP);
	uc_put_int(UC, ATTR_IFI, ctx->ifi);
	uc_put_end(UC);

	send_check(ctx);
	wait_for_stop(ctx);

	clear_device_entry(ctx);
}

static void simple_command(CTX, int cmd)
{
	no_other_options(ctx);

	uc_put_hdr(UC, cmd);
	uc_put_int(UC, ATTR_IFI, ctx->ifi);
	uc_put_end(UC);

	send_check(ctx);
}

static void req_dhcp_auto(CTX)
{
	simple_command(ctx, CMD_IF_DHCP_AUTO);
}

static void req_dhcp_once(CTX)
{
	simple_command(ctx, CMD_IF_DHCP_ONCE);
}

static void req_dhcp_stop(CTX)
{
	simple_command(ctx, CMD_IF_DHCP_STOP);
}

static void req_reconnect(CTX)
{
	simple_command(ctx, CMD_IF_RECONNECT);
}

static const struct cmd {
	char name[16];
	void (*call)(CTX);
} cmds[] = {
	{ "mode",      req_mode      },
	{ "also",      req_also      },
	{ "stop",      req_stop      },
	{ "auto-dhcp", req_dhcp_auto },
	{ "dhcp-once", req_dhcp_once },
	{ "stop-dhcp", req_dhcp_stop },
	{ "reconf",    req_reconnect },
	{ "reconnect", req_reconnect },
	{ "identify",  req_identify  },
	{ "id",        req_show_id   },
};

static int invoke(CTX, const struct cmd* cc)
{
	init_socket(ctx);
	resolve_device(ctx);

	cc->call(ctx);

	return 0;
}

int main(int argc, char** argv)
{
	struct top context, *ctx = &context;
	const struct cmd* cc;

	memzero(ctx, sizeof(*ctx));

	ctx->argc = argc;
	ctx->argv = argv;
	ctx->argi = 1;

	if(argc < 2)
		return show_status(ctx);
	if(argc == 2) /* ifctl device */
		fail("no command specified", NULL, 0);

	char* name = shift_arg(ctx);
	char* lead = shift_arg(ctx);

	if(*name == '-')
		fail("no options allowed", NULL, 0);

	ctx->name = name;

	for(cc = cmds; cc < cmds + ARRAY_SIZE(cmds); cc++)
		if(!strncmp(cc->name, lead, sizeof(cc->name)))
			return invoke(ctx, cc);

	fail("unknown command", lead, 0);
}
