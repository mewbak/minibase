#include <bits/secure.h>
#include <sys/prctl.h>
#include <sys/seccomp.h>

#include <string.h>

#include "msh.h"
#include "msh_cmd.h"

static const struct secbit {
	char name[12];
	int bit;
} secbits[] = {
	{ "keepcaps", SECURE_KEEP_CAPS            },
	{ "nosetuid", SECURE_NO_SETUID_FIXUP      },
	{ "noroot",   SECURE_NOROOT               },
	{ "noraise",  SECURE_NO_CAP_AMBIENT_RAISE }
};

static int striplock(char* str)
{
	int len = strlen(str);

	if(len <= 5)
		return 0;
	if(strncmp(str + len - 5, "-lock", 5))
		return 0;

	str[len-5] = '\0';
	return 1;
}

static int prctl_secbits(CTX)
{
	int lock, bits = 0;
	int cmd = PR_SET_SECUREBITS;
	const struct secbit* sb;
	char* arg;

	if(noneleft(ctx))
		return -1;

	while((arg = shift(ctx))) {
		lock = striplock(arg);

		for(sb = secbits; sb < ARRAY_END(secbits); sb++)
			if(!strncmp(sb->name, arg, sizeof(sb->name)))
				break;
		if(sb >= ARRAY_END(secbits))
			return error(ctx, "unknown bit", arg, 0);

		bits |= (1 << sb->bit);

		if(!lock) continue;

		bits |= (1 << (sb->bit + 1));
	}

	return fchk(sys_prctl(cmd, bits, 0, 0, 0), ctx, NULL);
}

static int prctl_nonewprivs(CTX)
{
	int cmd = PR_SET_NO_NEW_PRIVS;

	if(moreleft(ctx))
		return -1;

	return fchk(sys_prctl(cmd, 1, 0, 0, 0), ctx, NULL);
}

static int prctl_seccomp(CTX)
{
	struct mbuf mb;
	int ret;
	char* file;

	if(shift_str(ctx, &file))
		return -1;
	if(moreleft(ctx))
		return -1;
	if(fchk(mmapfile(&mb, file), ctx, file))
		return -1;
	if(!mb.len || mb.len % 8) {
		ret = error(ctx, "odd size:", file, 0);
		goto out;
	}

	struct seccomp sc = {
		.len = mb.len / 8,
		.buf = mb.buf
	};

	int mode = SECCOMP_SET_MODE_FILTER;
	ret = fchk(sys_seccomp(mode, 0, &sc), ctx, file);
out:
	munmapfile(&mb);
	return ret;
}

int cmd_prctl(CTX)
{
	char* arg;

	if(!(arg = shift(ctx)))
		return noneleft(ctx);

	if(!strcmp(arg, "no-new-privs"))
		return prctl_nonewprivs(ctx);
	if(!strcmp(arg, "secbits"))
		return prctl_secbits(ctx);
	if(!strcmp(arg, "seccomp"))
		return prctl_seccomp(ctx);

	return error(ctx, "unknown prctl", arg, 0);
}
