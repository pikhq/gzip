#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#include "util.h"

void *grow_buf(void *buf, size_t *buf_size)
{
	void *ret;
	size_t new_alloc;

	if(!buf_size) return 0;
	if(!*buf_size) {
		ret = malloc(1024);
		if(ret) *buf_size = 1024;
		return ret;
	}

	new_alloc = *buf_size + *buf_size/2;
	if(*buf_size == SIZE_MAX) {
		errno = ENOMEM;
		return 0;
	}
	if(new_alloc < *buf_size)
		new_alloc = SIZE_MAX;
	ret = realloc(buf, new_alloc);
	if(ret) *buf_size = new_alloc;
	return ret;
}
