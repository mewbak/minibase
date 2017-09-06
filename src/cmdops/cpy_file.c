#include <sys/dents.h>
#include <sys/file.h>
#include <sys/fpath.h>
#include <sys/fprop.h>
#include <sys/mman.h>
#include <sys/splice.h>

#include <errtag.h>
#include <format.h>
#include <string.h>
#include <util.h>

#include "cpy.h"

/* Contents trasfer for regular files. Surprisingly non-trivial task
   in Linux if we want to keep things fast and reliable. */

#define RWBUFSIZE 1024*1024

static int sendfile(CCT, DST, SRC, uint64_t* size)
{
	uint64_t done = 0;
	long ret = 0;
	long run = 0x7ffff000;

	int outfd = dst->fd;
	int infd = src->fd;

	if(*size < run)
		run = *size;

	while(1) {
		if(done >= *size)
			break;
		if((ret = sys_sendfile(outfd, infd, NULL, run)) <= 0)
			break;
		done += ret;
	};

	if(ret >= 0)
		return 0;
	if(!done && ret == -EINVAL)
		return -1;

	failat("sendfile", dst->dir, dst->name, ret);
}

static void alloc_rw_buf(CTX)
{
	if(ctx->buf)
		return;

	long len = RWBUFSIZE;
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_ANONYMOUS;
	char* buf = sys_mmap(NULL, len, prot, flags, -1, 0);

	if(mmap_error(buf))
		fail("mmap", NULL, (long)buf);

	ctx->buf = buf;
	ctx->len = len;
}

static void readwrite(CCT, DST, SRC, uint64_t* size)
{
	struct top* ctx = cct->top;

	alloc_rw_buf(ctx);

	uint64_t done = 0;

	char* buf = ctx->buf;
	long len = ctx->len;

	if(len > *size)
		len = *size;

	int rd = 0, wr;
	int rfd = src->fd;
	int wfd = dst->fd;

	while(1) {
		if(done >= *size)
			break;
		if((rd = sys_read(rfd, buf, len)) <= 0)
			break;
		if((wr = writeall(wfd, buf, rd)) < 0)
			failat("write", dst->dir, dst->name, wr);
		done += rd;
	} if(rd < 0) {
		failat("read", src->dir, src->name, rd);
	}
}

/* Sendfile may not work on a given pair of descriptors for various reasons.
   If this happens, we fall back to read/write calls.

   Generally the reasons depend on directory (and the underlying fs) so if
   sendfile fails for one file we stop using it for the whole directory. */

static void moveblock(CCT, DST, SRC, uint64_t* size)
{
	if(cct->nosf)
		;
	else if(sendfile(cct, dst, src, size) >= 0)
		return;
	else
		cct->nosf = 1;

	readwrite(cct, dst, src, size);
}

/* Except for the last line, the code below is only there to deal with sparse
   files. See lseek(2) for explanation. The idea is to seek over the holes and
   only write data-filled blocks.

   Non-sparse files contain one block spanning the whole file and no holes,
   so a single call to moveblock is enough. */

void transfer(CCT, DST, SRC, uint64_t* size)
{
	int rfd = src->fd;
	int wfd = dst->fd;

	int64_t ds;
	int64_t de;
	uint64_t blk;

	ds = sys_lseek(rfd, 0, SEEK_DATA);

	if(ds == -EINVAL || ds >= *size)
		goto plain;

	de = sys_lseek(rfd, ds, SEEK_HOLE);

	if(de < 0 || de >= *size)
		goto plain;

	sys_ftruncate(wfd, *size);

	while(1) {
		sys_lseek(wfd, ds, SEEK_SET);
		sys_lseek(rfd, ds, SEEK_SET);

		if((blk = de - ds) > 0)
			moveblock(cct, dst, src, &blk);

		if(de >= *size)
			break;

		ds = sys_lseek(rfd, de, SEEK_DATA);
		de = sys_lseek(rfd, ds, SEEK_HOLE);

		if(ds < 0 || de < 0)
			break;
	}

	return;

plain:
	moveblock(cct, dst, src, size);
}
