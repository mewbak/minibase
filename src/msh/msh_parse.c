#include <string.h>
#include <format.h>

#include "msh.h"

#define SEP 0
#define ARG 1
#define DQUOT 2
#define SQUOT 3
#define VSIGN 4
#define VCONT 5
#define COMM 6

void dispatch(struct sh* ctx, char c);

void set_state(struct sh* ctx, int st)
{
	ctx->state = (ctx->state & ~0xFF) | (st & 0xFF);
}

void push_state(struct sh* ctx, int st)
{
	ctx->state = (ctx->state << 8) | (st & 0xFF);
}

void pop_state(struct sh* ctx)
{
	ctx->state = (ctx->state >> 8);
}

void start_var(struct sh* ctx)
{
	ctx->var = ctx->hptr;
	push_state(ctx, VSIGN);
}

void end_var(struct sh* ctx)
{
	*(ctx->hptr) = '\0';	

	char* val = valueof(ctx, ctx->var);
	long vlen = strlen(val);

	ctx->hptr = ctx->var;

	char* spc = halloc(ctx, vlen);
	memcpy(spc, val, vlen);

	ctx->var = NULL;

	pop_state(ctx);
}

void add_char(struct sh* ctx, char c)
{
	char* spc = halloc(ctx, 1);
	*spc = c;
}

void end_arg(struct sh* ctx)
{
	add_char(ctx, 0);
	ctx->count++;
	set_state(ctx, SEP);
}

void end_cmd(struct sh* ctx)
{
	int argn = ctx->count;
	char* base = ctx->heap;
	char* bend = ctx->hptr;

	if(!argn) return;

	char** argv = halloc(ctx, (argn+1)*sizeof(char*));
	int argc = 0;

	argv[0] = ctx->heap;
	char* p;

	int sep = 1;
	for(p = base; p < bend; p++) {
		if(sep && argc < argn)
			argv[argc++] = p;
		sep = !*p;
	}

	argv[argc] = NULL;

	exec(ctx, argc, argv);

	ctx->hptr = ctx->heap;
	ctx->count = 0;
}

void parse_sep(struct sh* ctx, char c)
{
	switch(c) {
		case '\0':
		case ' ':
		case '\t':
			break;
		case '#':
			end_cmd(ctx);
			set_state(ctx, COMM);
			break;
		case '\n':
			end_cmd(ctx);
			set_state(ctx, SEP);
			break;
		default:
			set_state(ctx, ARG);
			dispatch(ctx, c);
	};
}

void parse_arg(struct sh* ctx, char c)
{
	switch(c) {
		case '\0':
		case ' ':
		case '\t': end_arg(ctx); break;
		case '\n': end_arg(ctx); end_cmd(ctx); break;
		case '$':  start_var(ctx); break;
		case '"':  push_state(ctx, DQUOT); break;
		case '\'': push_state(ctx, SQUOT); break;
		default: add_char(ctx, c);
	}
}

void parse_dquote(struct sh* ctx, char c)
{
	switch(c) {
		case '"': pop_state(ctx); break;
		case '$': start_var(ctx); break;
		default: add_char(ctx, c);
	}
}

void parse_squote(struct sh* ctx, char c)
{
	switch(c) {
		case '\'': pop_state(ctx); break;
		default: add_char(ctx, c);
	}
}

void parse_vsign(struct sh* ctx, char c)
{
	switch(c) {
		case 'a'...'z':
		case 'A'...'Z':
			add_char(ctx, c);
			set_state(ctx, VCONT);
			break;
		case '"':
		case '\'':
		case '$':
		case '\0':
		case '\n':
			fatal(ctx, "invalid syntax", NULL);
		default:
			add_char(ctx, c);
			end_var(ctx);
	}
}

void parse_comm(struct sh* ctx, char c)
{
	if(c == '\n')
		set_state(ctx, SEP);
}

void parse_vcont(struct sh* ctx, char c)
{
	switch(c) {
		case 'a'...'z':
		case 'A'...'Z':
			add_char(ctx, c);
			break;
		default:
			end_var(ctx);
			dispatch(ctx, c);
	}
}

void dispatch(struct sh* ctx, char c)
{
	switch(ctx->state & 0xFF) {
		case SEP: parse_sep(ctx, c); break;
		case ARG: parse_arg(ctx, c); break;
		case DQUOT: parse_dquote(ctx, c); break;
		case SQUOT: parse_squote(ctx, c); break;
		case VSIGN: parse_vsign(ctx, c); break;
		case VCONT: parse_vcont(ctx, c); break;
		case COMM: parse_comm(ctx, c); break;
		default: fatal(ctx, "bad internal state", NULL);
	}
}

void parse(struct sh* ctx, char* buf, int len)
{
	char* end = buf + len;
	char* p;

	for(p = buf; p < end; p++) {
		dispatch(ctx, *p);
		if(*p == '\n')
			ctx->line++;
	}
}

void pfini(struct sh* ctx)
{
	dispatch(ctx, '\0');

	if(ctx->state)
		fatal(ctx, "unexpected EOF", NULL);
}
