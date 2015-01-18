#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <iconv.h>

void *grow_buf(void*, size_t*);
size_t asiconv(iconv_t, char*, size_t, char**, size_t*);

#endif
