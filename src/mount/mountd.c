#include <bits/socket/unix.h>

#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ppoll.h>
#include <sys/signal.h>
#include <sys/sched.h>
#include <sys/fpath.h>

#include <string.h>
#include <sigset.h>
#include <cmsg.h>
#include <util.h>
#include <main.h>

#include "common.h"
#include "mountd.h"

ERRTAG("mountd");

struct top {
	sigset_t defsigset;
	struct pollfd pfds[NCONNS+1];
	int nconns;
	int sigterm;
};

#define CTX struct top* ctx

void quit(const char* msg, char* arg, int err)
{
	sys_unlink(CONTROL);

	if(msg || arg || err)
		fail(msg, arg, err);
	else
		_exit(-1);
}

static void sighandler(int sig)
{
	switch(sig) {
		case SIGINT:
		case SIGTERM:
			quit(NULL, NULL, 0);
	}
}

static void setup_signals(CTX)
{
	SIGHANDLER(sa, sighandler, 0);
	int ret;

	if((ret = sys_sigprocmask(SIG_BLOCK, &sa.mask, &ctx->defsigset)) < 0)
		fail("sigprocmask", NULL, ret);

	sigaddset(&sa.mask, SIGINT);
	sigaddset(&sa.mask, SIGTERM);
	sigaddset(&sa.mask, SIGALRM);

	if((ret = sys_sigaction(SIGINT,  &sa, NULL)) < 0)
		fail("sigaction", "SIGINT", ret);
	if((ret = sys_sigaction(SIGTERM, &sa, NULL)) < 0)
		fail("sigaction", "SIGTERM", ret);
	if((ret = sys_sigaction(SIGALRM, &sa, NULL)) < 0)
		fail("sigaction", "SIGALRM", ret);
}

static void accept_connection(CTX, int sfd)
{
	struct pollfd* pfds = ctx->pfds;

	int cfd;
	struct sockaddr_un addr;
	int addrlen = sizeof(addr);
	int flags = SOCK_NONBLOCK;

	if((cfd = sys_accept4(sfd, &addr, &addrlen, flags)) >= 0)
		;
	else if(cfd == -EAGAIN)
		return;
	else fail("accept", NULL, cfd);

	if(ctx->nconns < NCONNS) {
		int i = ctx->nconns++;
		pfds[i+1].fd = cfd;
		pfds[i+1].events = POLLIN;
	} else {
		sys_close(cfd);
	}
}

static void retract_nconns(CTX)
{
	int i = ctx->nconns;

	for(; i > 0; i--)
		if(ctx->pfds[i-1].fd >= 0)
			break;

	ctx->nconns = i;
}

static void check_listening(CTX, struct pollfd* pf)
{
	if(pf->revents & POLLIN)
		accept_connection(ctx, pf->fd);
	if(pf->revents & ~POLLIN)
		quit("control socket lost", NULL, 0);
}

static void check_client(CTX, struct pollfd* pf)
{
	if(pf->revents & POLLIN)
		handle(pf->fd);

	if(pf->revents & ~POLLIN) {
		sys_close(pf->fd);
		pf->fd = -1;
		retract_nconns(ctx);
	}
}

static void check_polled_fds(CTX)
{
	int nconns = ctx->nconns;
	struct pollfd* pfds = ctx->pfds;
	int i;

	for(i = 1; i < nconns + 1; i++)
		check_client(ctx, &pfds[i]);

	check_listening(ctx, &pfds[0]);
}

static void setup_ctrl(CTX)
{
	int flags = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;
	struct sockaddr_un addr = {
		.family = AF_UNIX,
		.path = CONTROL
	};
	int fd, ret;

	if((fd = sys_socket(AF_UNIX, flags, 0)) < 0)
		fail("socket", "AF_UNIX", fd);
	if((ret = sys_bind(fd, &addr, sizeof(addr))) < 0)
		fail("bind", addr.path, ret);
	if((ret = sys_setsockopti(fd, SOL_SOCKET, SO_PASSCRED, 1)) < 0)
		fail("setsockopt", "SO_PASSCRED", ret);
	if((ret = sys_listen(fd, 1)))
		fail("listen", addr.path, ret);

	ctx->pfds[0].fd = fd;
	ctx->pfds[0].events = POLLIN;
}

int main(int argc, char** argv)
{
	(void)argv;
	struct top context, *ctx = &context;

	if(argc > 1)
		fail("too many arguments", NULL, 0);

	memzero(ctx, sizeof(*ctx));

	setup_ctrl(ctx);
	setup_signals(ctx);

	sigset_t* mask = &ctx->defsigset;

	while(1) {
		int nconns = ctx->nconns;
		struct pollfd* pfds = ctx->pfds;

		int ret = sys_ppoll(pfds, nconns+1, NULL, mask);

		if(ret < 0)
			quit("ppoll", NULL, ret);
		if(ret > 0)
			check_polled_fds(ctx);
	}
}
