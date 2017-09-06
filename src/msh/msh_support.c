#include <sys/mman.h>
#include <sys/file.h>

#include <string.h>
#include <format.h>
#include <util.h>

#include "msh.h"

/* Heap routines. See msh.h for heap layout. */

#define PAGE 4096

void hinit(struct sh* ctx)
{
	void* heap = sys_brk(0);
	ctx->heap = heap;
	ctx->esep = NULL;
	ctx->csep = heap;
	ctx->hptr = heap;
	ctx->hend = sys_brk(heap + 4096);
}

void* halloc(struct sh* ctx, int len)
{
	void* ret = ctx->hptr;

	if(ctx->hptr + len < ctx->hend)
		goto ptr;

	int spc = ctx->hend - ctx->hptr - len;
	spc += (PAGE - spc % PAGE) % PAGE;
	ctx->hend = sys_brk(ctx->hend + spc);

	if(ctx->hptr + len < ctx->hend)
		quit(ctx, "cannot allocate memory", NULL, 0);
ptr:
	ctx->hptr += len;

	return ret;
}

void hrev(struct sh* ctx, int what)
{
	switch(what) {
		case CSEP:
			ctx->hptr = ctx->csep;
			break;
		case ESEP:
			ctx->csep = NULL;
			ctx->hptr = ctx->esep;
			break;
		case HEAP:
			ctx->esep = NULL;
			ctx->csep = NULL;
			ctx->hptr = ctx->heap;
	}
}

void hset(struct sh* ctx, int what)
{
	switch(what) {
		case CSEP: ctx->csep = ctx->hptr; break;
		case ESEP: ctx->esep = ctx->hptr; break;
	}
}

int mmapfile(struct mbuf* mb, char* name)
{
	int fd;
	long ret;
	struct stat st;

	if((fd = sys_open(name, O_RDONLY | O_CLOEXEC)) < 0)
		return fd;
	if((ret = sys_fstat(fd, &st)) < 0)
		goto out;
	/* get larger-than-int files out of the picture */
	if(st.size > 0x7FFFFFFF) {
		ret = -E2BIG;
		goto out;
	}

	void* ptr = sys_mmap(NULL, st.size, PROT_READ, MAP_SHARED, fd, 0);

	if(mmap_error(ptr)) {
		ret = (long)ptr;
		goto out;
	}

	mb->len = st.size;
	mb->buf = ptr;
	ret = 0;
out:
	sys_close(fd);
	return ret;
}

int munmapfile(struct mbuf* mb)
{
	return sys_munmap(mb->buf, mb->len);
}
