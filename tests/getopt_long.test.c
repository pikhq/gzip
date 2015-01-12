#define _GNU_SOURCE
#define _ALL_SOURCE
#include <getopt.h>
#include <stddef.h>

#undef getopt_long
#undef getopt_long_only

struct test_option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

#define CHECK_EXPR(x) ((void)sizeof(char[2*!!(x)-1]))

int main(int argc, char **argv)
{
	struct option option;
	struct test_option test_option;
	CHECK_EXPR(offsetof(struct option, name) == offsetof(struct test_option, name));
	CHECK_EXPR(offsetof(struct option, has_arg) == offsetof(struct test_option, has_arg));
	CHECK_EXPR(offsetof(struct option, flag) == offsetof(struct test_option, flag));
	CHECK_EXPR(offsetof(struct option, val) == offsetof(struct test_option, val));
	CHECK_EXPR(sizeof(option.name) == sizeof(test_option.name));
	CHECK_EXPR(sizeof(option.has_arg) == sizeof(test_option.has_arg));
	CHECK_EXPR(sizeof(option.flag) == sizeof(test_option.flag));
	CHECK_EXPR(sizeof(option.val) == sizeof(test_option.val));
	CHECK_EXPR(sizeof(struct option) == sizeof(struct test_option));

	int (*p)(int, char *const[], const char*, const struct option*, int*) = getopt_long;
	p = getopt_long_only;
	
	int (getopt_long)(int, char *const[], const char*, const struct option*, int*);
	int (getopt_long_only)(int, char *const[], const char*, const struct option*, int*);

	return getopt_long(argc, argv, "", (const struct option*)NULL, (int*)NULL)
		|| getopt_long_only(argc, argv, "", (const struct option*)NULL, (int*)NULL);
}
