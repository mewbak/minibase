#include <bits/major.h>
#include <sys/fpath.h>

#include <format.h>
#include <util.h>
#include <main.h>

ERRTAG("mknod");
ERRLIST(NEACCES NEEXIST NEFAULT NEINVAL NELOOP NENOENT NENOMEM
	NENOSPC NENOTDIR NEPERM NEROFS);

static int parse_mode(const char* mode)
{
	const char* p;
	int d, m = 0;

	if(*mode++ != '0')
		fail("mode must be octal", NULL, 0);

	for(p = mode; *p; p++)
		if(*p >= '0' && (d = *p - '0') < 8)
			m = (m<<3) | d;
		else
			fail("bad mode specification", mode, 0);

	return m;
}

static int parse_type(const char* type)
{
	if(!type[0] || type[1])
		goto bad;

	switch(*type) {
		case 'c':
		case 'u': return S_IFCHR;
		case 'b': return S_IFBLK;
		case 'p': return S_IFIFO;
		case 'r': return S_IFREG;
		case 's': return S_IFSOCK;
	}

bad:	fail("bad node type specification", NULL, 0);
}

int xparseint(char* str)
{
	int n;

	char* end = parseint(str, &n);
	if(*end || end == str)
		fail("not a number", str, 0);

	return n;
}

int main(int argc, char** argv)
{
	int i = 1;
	int mode = 0666;
	char* name = NULL;
	int type = 0;
	int major = 0;
	int minor = 0;

	if(argc < 3)
		fail("file name and type must be supplied", NULL, 0);

	if(i < argc)
		name = argv[i++];
	if(i < argc && argv[i][0] >= '0' && argv[i][0] <= '9')
		mode = parse_mode(argv[i++]);
	if(i < argc)
		type = parse_type(argv[i++]);

	int isdev = (type == S_IFCHR || type == S_IFBLK);

	if(isdev && (i != argc - 2))
		fail("major and minor numbers must be supplied", NULL, 0);
	if(!isdev && (i != argc))
		fail("too many arguments", NULL, 0);

	if(isdev) {
		major = xparseint(argv[i++]);
		minor = xparseint(argv[i++]);
	}

	long dev = makedev(major, minor);
	long ret = sys_mknod(name, mode | type, dev);

	if(ret < 0)
		fail("cannot create", name, ret);

	return 0;
}
