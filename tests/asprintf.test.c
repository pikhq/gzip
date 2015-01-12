#define _GNU_SOURCE
#define _ALL_SOURCE
#include <stdio.h>

#undef asprintf

int main()
{
	int (*p)(char **,const char*,...) = asprintf;
	char *buf;
	int (asprintf)(char**, const char*,...);
	return asprintf(&buf, "%s\n", "gibberish");
}
