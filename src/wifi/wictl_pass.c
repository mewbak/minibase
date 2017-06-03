#include <sys/write.h>
#include <sys/read.h>

#include <crypto/pbkdf2.h>
#include <nlusctl.h>
#include <string.h>
#include <heap.h>
#include <util.h>
#include <fail.h>

#include "common.h"
#include "wictl.h"

static void put_psk(struct top* ctx, char* ssid, char* pass)
{
	uint8_t psk[32];

	memset(psk, 0, sizeof(psk));

	int ssidlen = strlen(ssid);
	int passlen = strlen(pass);

	pbkdf2_sha1(psk, sizeof(psk), pass, passlen, ssid, ssidlen, 4096);

	uc_put_bin(UC, ATTR_PSK, psk, sizeof(psk));
}

/* As written this only works with sane ssids and passphrases.
   Should be extended at some point to handle ssid escapes and
   multiline phrases. */

void put_psk_arg(struct top* ctx, char* ssid, char* pass)
{
	put_psk(ctx, ssid, pass);
}

void put_psk_input(struct top* ctx, char* ssid)
{
	char buf[256];
	int rd;
	char* prompt = "Passphrase: ";

	syswrite(STDOUT, prompt, strlen(prompt));
	rd = sysread(STDIN, buf, sizeof(buf));

	if(rd >= sizeof(buf))
		fail("passphrase too long", NULL, 0);

	if(rd > 0 && buf[rd-1] == '\n')
		rd--;
	if(!rd)
		fail("empty passphrase rejected", NULL, 0);
	buf[rd] = '\0';

	put_psk(ctx, ssid, buf);
}
