#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>

#include <zlib.h>

static char *program_name;

static const struct option options[] = {
	{"ascii",	no_argument,		NULL, 'a'},
	{"stdout",	no_argument,		NULL, 'c'},
	{"to-stdout",	no_argument,		NULL, 'c'},
	{"decompress",	no_argument,		NULL, 'd'},
	{"uncompress",	no_argument,		NULL, 'd'},
	{"force",	no_argument,		NULL, 'f'},
	{"help",	no_argument,		NULL, 'h'},
	{"keep",	no_argument,		NULL, 'k'},
	{"list",	no_argument,		NULL, 'l'},
	{"no-name",	no_argument,		NULL, 'n'},
	{"name",	no_argument,		NULL, 'N'},
	{"quiet",	no_argument,		NULL, 'q'},
	{"recursive",	no_argument,		NULL, 'r'},
	{"rsyncable",	no_argument,		NULL, 'R'},
	{"suffix",	required_argument,	NULL, 'S'},
	{"test",	no_argument,		NULL, 't'},
	{"verbose",	no_argument,		NULL, 'v'},
	{"fast",	no_argument,		NULL, '1'},
	{"best",	no_argument,		NULL, '9'},
	{0}
};
static const char option_str[] = "acdfhklnNqrS:tvV123456789";

static bool  opt_ascii_text	= false;
static bool  opt_stdout		= false;
static bool  opt_compress	= true;
static bool  opt_force		= false;
static bool  opt_keep		= false;
static bool  opt_list		= false;
static bool  opt_store_name	= true;
static bool  opt_restore_name	= false;
static int   opt_verbosity	= 1;
static bool  opt_recursive	= false;
static bool  opt_rsyncable	= false;
static char *opt_suffix		= ".gz";
static bool  opt_test		= false;
static int   opt_level		= 6;

static int asprintf(char **s, const char *fmt, ...)
{
	int l;
	va_list ap, ap2;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	l = vsnprintf(0, 0, fmt, ap2);
	va_end(ap2);
	if(l < 0 || !(*s=malloc(l+1U))) return -1;
	return vsnprintf(*s, l+1U, fmt, ap);
}

static void report_error(const char *msg, const char *fmt, ...)
{
	va_list ap;
	if(opt_verbosity < 1) return;
	fprintf(stderr, "%s: ", program_name);
	if(fmt) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
	if(msg) {
		perror(msg);
	}
}

static void write_help()
{
	static const char help_msg[] =
		"Usage: gzip [OPTION]... [FILE]...\n"
		"Compress or uncompress FILEs (by default, compressing "
			"in-place)\n"
		"\n"
		"  -c, --stdout         write on standard out, keeping "
			"original files unchanged\n"
		"  -d, --decompress     decompress files\n"
		"  -f, --force          force overwriting output files; "
			"compress links\n"
		"  -h, --help           output this message\n"
		"  -l, --list           list compressed file contents\n"
		"  -n, --no-name        do not save or restore the original "
			"file name and time stamp\n"
		"  -N, --name           save or restore the original file "
			"name and time stamp (default)\n"
		"  -q, --quiet          suppress all warnings\n"
		"  -r, --recursive      operate recursively on directories\n"
		"  --rsyncable          generate rsync-friendly output\n"
		"  -S, --suffix=SUF     use suffix SUF on compressed files\n"
		"  -t, --test           test the integrity of compressed "
			"files\n"
		"  -v, --verbose        give verbose output\n"
		"  -V, --version        output version number\n"
		"  -1, --fast           compress fast\n"
		"  -9, --best           compress better\n"
		"\n"
		"With no FILE, or when FILE is -, operates on standard "
			"input.\n"
		"\n"
		"Report bugs to <josiahw@gmail.com>\n";
	printf("%s", help_msg);
		
}

static void write_version()
{
	static const char version_msg[] =
		"gzip 0.0\n"
		"Copyright (c) 2014, Josiah Worcester.\n";
	printf("%s", version_msg);
}

static int init_stream(z_stream *strm)
{
	if(opt_compress) {
		if(deflateInit2(strm, opt_level, Z_DEFLATED, 31, 8,
				Z_DEFAULT_STRATEGY) != Z_OK) {
			report_error(0, "%s\n", strm->msg);
			return 1;
		}
	} else {
		if(inflateInit2(strm, 31) != Z_OK) {
			report_error(0, "%s\n", strm->msg);
			return 1;
		}
	}
	return 0;
}

static int do_write(int fd, const void *buf, size_t count)
{
	while(count) {
		ssize_t n = write(fd, buf, count);
		if(n < 0 && errno == EINTR) continue;
		if(n < 0) { report_error(0, 0); return 1; }
		buf += n; count -= n;
	}
	return 0;
}

static void close_stream(z_stream *strm)
{
	if(opt_compress) {
		deflateEnd(strm);
	} else {
		inflateEnd(strm);
	}
}

static int remove_suffix(char *str, char *suffix)
{
	for(char *s = str; *s; s++) {
		if(strcmp(s, suffix) == 0) {
			*s = 0;
			return 0;
		}
	}
	return 1;
}

static int read_header(z_stream *strm, int in_fd)
{
	return 1;
}

static int out_to_fd(z_stream *strm, char *in_file, int in_fd,
                     char *out_file, int out_fd)
{
	int ret = 0;
	char in[4096];
	char out[4096];

	if(opt_compress) {
		ssize_t read_amt, write_amt;
		int flush = Z_NO_FLUSH;

		do {
			read_amt = read(in_fd, in, sizeof in);
			if(read_amt < 0) {
				report_error(in_file, 0);
				return 1;
			}
			if(read_amt == 0)
				flush = Z_FINISH;
			strm->next_in = in;
			strm->avail_in = read_amt;
			do {
				strm->avail_out = sizeof out;
				strm->next_out = out;
				deflate(strm, flush);
				if(do_write(out_fd, out,
				            sizeof out - strm->avail_out) != 0)
					return 1;
			} while(strm->avail_out == 0);
		} while(flush != Z_FINISH);
	} else {
		ssize_t read_amt, write_amt;
		int err;
		do {
			read_amt = read(in_fd, in, sizeof in);
			if(read_amt < 0) {
				report_error(in_file, 0);
				return 1;
			}
			if(read_amt == 0) {
				report_error(0, "%s: bad input.\n", in_file);
				return 1;
			}
			strm->next_in = in;
			strm->avail_in = read_amt;
			do {
				strm->avail_out = sizeof out;
				strm->next_out = out;
				err = inflate(strm, Z_NO_FLUSH);
				if(err != Z_OK && err != Z_STREAM_END) {
					report_error(0, "%s: bad input.\n",
					             in_file);
					return 1;
				}
				if(do_write(out_fd, out,
				            sizeof out - strm->avail_out) != 0)
					return 1;
			} while(strm->avail_out == 0);
		} while(err != Z_STREAM_END);
	}
}

static int out_to_stdout(z_stream *strm, char *in_file, int in_fd)
{
	return out_to_fd(strm, in_file, in_fd, "stdout", 1);
}

static int out_to_filename(z_stream *strm, char *in_file, int in_fd,
                           char *filename)
{
	int out_fd;

	out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(out_fd < 0) {
		perror(filename);
		return 1;
	}

	return out_to_fd(strm, in_file, in_fd, filename, out_fd);	
}

static int handle_stdin()
{
	int ret = 0;
	z_stream strm = {0};
	char *out_path = NULL;

	if(init_stream(&strm))
		return 1;
	if(opt_restore_name && !opt_compress && !opt_stdout) {
		ret = read_header(&strm, 0);
		if(ret != 0) {
			goto cleanup;
		}
		gz_header header;
		inflateGetHeader(&strm, &header);
		if(header.name) {
			ret = out_to_filename(&strm, "stdin", 0, header.name);
			goto cleanup;
		}
	}
	ret = out_to_stdout(&strm, "stdin", 0);
cleanup:
	close_stream(&strm);
	return ret;
}

static int handle_dir(char *path, int in_fd)
{
	return 1;
}

static int handle_path(char *path)
{
	int in_fd;
	struct stat stat_buf;
	z_stream strm = {0};
	char *out_path = 0;
	int ret = 0;

	if((in_fd = open(path, O_RDONLY)) < 0) {
		report_error(path, 0);
		return 1;
	}

	if(fstat(in_fd, &stat_buf) == -1) {
		report_error(path, 0);
		ret = 1;
		goto cleanup_fd;
	}

	if(S_ISDIR(stat_buf.st_mode)) {
		if(opt_recursive) {
			ret = handle_dir(path, in_fd);
			goto cleanup_fd;
		} else {
			report_error(0, "%s: is a directory\n", path);
			ret = 2;
			goto cleanup_fd;
		}
	}

	if(init_stream(&strm))
		goto cleanup_fd;

	if(opt_stdout) {
		return out_to_stdout(&strm, path, in_fd);
	}

	if(!opt_compress) {
		if(opt_restore_name) {
			int res = read_header(&strm, in_fd);
			if(res != 0) {
				ret = res;
				goto cleanup_strm;
			}
			gz_header header;
			inflateGetHeader(&strm, &header);
			if(header.name)
				out_path = strdup(header.name);
		}
		if(!out_path) {
			out_path = strdup(path);
			if(out_path)
				remove_suffix(out_path, opt_suffix);
		}
		if(!out_path) {
			report_error(0, 0);
			ret = 2;
			goto cleanup_strm;
		}
	} else {
		int len;
		len = asprintf(&out_path, "%s%s", path, opt_suffix);
		if(len < 0) {
			report_error(0, 0);
			ret = 2;
			goto cleanup_strm;
		}
	}

	ret = out_to_filename(&strm, path, in_fd, out_path);

	free(out_path);
cleanup_strm:
	close_stream(&strm);
cleanup_fd:
	close(in_fd);
	return ret;
}

int main(int argc, char **argv)
{
	int c;
	int n;
	int ret_val = 0;
	char **v;

	program_name = argv[0];

	while((c = getopt_long(argc, argv, option_str, options, NULL)) != -1) {
		switch(c) {
		case 'a':
			opt_ascii_text = true;
			break;
		case 'c':
			opt_stdout = true;
			break;
		case 'd':
			opt_compress = false;
			break;
		case 'f':
			opt_force = true;
			break;
		case 'h':
			write_help();
			return 0;
		case 'k':
			opt_keep = true;
			break;
		case 'l':
			opt_list = true;
			break;
		case 'n':
			opt_restore_name = opt_store_name = false;
			break;
		case 'N':
			opt_restore_name = opt_store_name = true;
			break;
		case 'q':
			opt_verbosity = 0;
			break;
		case 'r':
			opt_recursive = true;
			break;
		case 'R':
			opt_rsyncable = true;
			break;
		case 'S':
			opt_suffix = optarg;
			break;
		case 't':
			opt_test = true;
			break;
		case 'v':
			opt_verbosity = 2;
			break;
		case 'V':
			write_version();
			return 0;
		case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9':
			opt_level = c - '0';
			break;
		default:
			write_help();
			return 1;
		}
	}

	n = argc-optind;
	v = argv+optind;

	if(n == 0) {
		n = 1;
		v = (char*[]){"-", 0};
	}

	for(; n; v++, n--) {
		if(strcmp(*v, "-") == 0) {
			handle_stdin();
		} else {
			handle_path(*v);
		}
	}
}
