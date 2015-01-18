#include <iconv.h>
#include <errno.h>
#include <stdlib.h>

#include "util.h"

size_t asiconv(iconv_t cd, const char * restrict inbuf, size_t inlen,
	char *restrict*restrict outbuf, size_t *outlen)
{
	const char *inptr = inbuf;
	size_t inrem = inlen;
	char *outptr;
	size_t out_i = 0;
	size_t outrem;
	size_t nconv;
	size_t ret = 0;

	if(!outbuf) return errno = EINVAL, -1;
	if(!outlen) if(*outbuf) return errno = EINVAL, -1; else outlen = &(size_t){0};

	if(!*outbuf) {
		*outbuf = malloc(inlen);
		if(!outbuf) return (size_t)-1;
		*outlen = inlen;
	}

	outptr = *outbuf;
	outrem = *outlen;

	do {
		size_t oldrem = outrem;
		nconv = iconv(cd, (char**)&inptr, &inrem, &outptr, &outrem);
		out_i += outrem - oldrem;

		if(nconv == (size_t)-1 && errno == E2BIG) {
			size_t tmplen = *outlen;
			char *tmp = grow_buf(*outbuf, &tmplen);
			if(!tmp) return -1;
			outrem += tmplen - *outlen;
			outptr = tmp + out_i;
			*outbuf = tmp;
		}
		if(nconv == (size_t)-1)
			return (size_t)-1;
		ret += nconv;
	} while(nconv == (size_t)-1);

	outptr = realloc(*outbuf, out_i);
	if(outptr) *outbuf = outptr;
	return ret;
}
