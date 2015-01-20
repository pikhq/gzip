#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <locale.h>

#include <zlib.h>

#include "util.h"

#include "asprintf.h"
#include "getopt_long.h"

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
static char *opt_suffix		= ".gz";
static int   opt_level		= 6;

static int handle_path(char*);

static void report_error(int err, const char *fmt, ...)
{
	va_list ap;
	if(opt_verbosity < 1) return;
	fprintf(stderr, "%s: ", program_name);
	if(fmt) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
	if(err) {
		fprintf(stderr, "%s%s", fmt ? ": " : "", strerror(err));
	}
	fprintf(stderr, "\n");
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
			"file time stamp\n"
		"  -N, --name           save or restore the original file "
			"time stamp (does not\n"
		"                       save or restore the name)\n"
		"                       (default when compressing)\n"
		"  -q, --quiet          suppress all warnings\n"
		"  -r, --recursive      operate recursively on directories\n"
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
		if(deflateInit2(strm, opt_level, Z_DEFLATED, 31, 9,
				Z_DEFAULT_STRATEGY) != Z_OK) {
			report_error(0, "%s", strm->msg);
			return 1;
		}
	} else {
		if(inflateInit2(strm, 31) != Z_OK) {
			report_error(0, "%s", strm->msg);
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
		if(n < 0) { report_error(errno, 0); return 1; }
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

static int remove_suffix(char *str, char *suffix, bool case_insensitive)
{
	int (*cmp)(const char*,const char*) = strcmp;
	if(case_insensitive)
		cmp = strcasecmp;
	for(char *s = str; *s; s++) {
		if(cmp(s, suffix) == 0) {
			*s = 0;
			return 1;
		}
	}
	return 0;
}

static char *input_to_output_path(const char *path)
{
	char *ret = strdup(path);
	if(!ret) {
		return 0;
	}

	if(!remove_suffix(ret, opt_suffix, false)) {
		char *suffixes[] = {
			".gz",
			"-gz",
			".z",
			"-z",
			"_z",
			".tgz",
			0
		};
		for(char **suffix = suffixes; *suffix; suffix++)
			if(remove_suffix(ret, *suffix, true))
				return ret;
		free(ret);
		errno = EINVAL;
		return 0;
	}

	return ret;
}

static ssize_t inflate_read(z_stream *strm, char *buf, size_t len, int flush)
{
	int err;
	ssize_t ret = 0;

	do {
		strm->next_out = buf + ret;
		strm->avail_out = len;
		if(strm->avail_out != len) strm->avail_out = INT_MAX;
		err = inflate(strm, flush);
		ret += len - strm->avail_out;
		len -= len - strm->avail_out;
		if(err == Z_BUF_ERROR && flush == Z_FINISH) {
			strm->msg = "unexpected end of file";
			return -2;
		}
		if(err == Z_BUF_ERROR) {
			return ret;
		}
		if(err == Z_STREAM_END) {
			if(strm->avail_in) {
				inflateReset(strm);
				continue;
			}
			return ret == 0 ? -1 : ret;
		}
		if(err != Z_OK) {
			if(!strm->msg)
				strm->msg = (char*)zError(err);
			return -2;
		}
	} while(len);

	return ret;
}

static ssize_t deflate_read(z_stream *strm, char *buf, size_t len, int flush)
{
	ssize_t ret = 0;
	while(len) {
		strm->next_out = buf + ret;
		strm->avail_out = len;
		if(strm->avail_out != len) strm->avail_out = INT_MAX;
		deflate(strm, flush);
		if((len - strm->avail_out) == 0)
			if(ret == 0 && flush == Z_FINISH)
				return -1;
			else
				return ret;
		ret += len - strm->avail_out;
		len -= len - strm->avail_out;
	}
	return ret;
}

static int strm_push(z_stream *strm, char *buf, size_t len)
{
	strm->next_in = buf;
	strm->avail_in = len;
	if(strm->avail_in != len) { errno = ERANGE; return -1; }
	return 0;
}

static ssize_t strm_read(z_stream *strm, char *buf, size_t len, int flush)
{
	if(opt_compress)
		return deflate_read(strm, buf, len, flush);
	else
		return inflate_read(strm, buf, len, flush);
}

static int read_header(z_stream *strm, gz_header *head, char *in_file,
                       int in_fd)
{
	char in = 0;
	char out = 0;
	int res;
	int ret = 0;

	res = inflateGetHeader(strm, head);

	head->name = 0;
	head->name_max = 0;

	while(head->done == 0) {
		size_t read_amt;
		int flush = Z_BLOCK;

		read_amt = read(in_fd, &in, 1);
		if(read_amt < 0) {
			report_error(errno, "%s", in_file);
			return 1;
		}
		if(read_amt == 0) flush = Z_FINISH;

		strm_push(strm, &in, 1);
		if(strm_read(strm, &out, 1, flush) < 0) {
			report_error(0, "%s: %s", in_file, strm->msg);
			return 1;
		}
	}
	return 0;
}

static int out_to_fd(z_stream *strm, char *in_file, int in_fd,
                     char *out_file, int out_fd)
{
	int ret = 0;
	char in[4096];
	char out[4096];
	ssize_t read_amt, write_amt = 0;
	int flush = Z_NO_FLUSH;

	do {
		read_amt = read(in_fd, in, sizeof in);
		if(read_amt < 0) {
			report_error(errno, "%s", in_file);
			return 1;
		}
		if(read_amt == 0) flush = Z_FINISH;

		if(strm_push(strm, in, read_amt) < 0) {
			report_error(errno, "%s", in_file);
			return 1;
		}

		/* If we still have input and the last bit of input happened to
		 * be a complete gzip file we need to tell inflate to keep
		 * processing.
		 */
		if(write_amt == -1 && flush != Z_FINISH && !opt_compress) {
			inflateReset(strm);
		}
		while((write_amt = strm_read(strm, out, sizeof out, flush))
		      > 0) {
			if(do_write(out_fd, out, write_amt) != 0)
				return 1;
		}
		if(write_amt < -1) {
			report_error(0, "%s: %s", in_file, strm->msg);
			return 1;
		}
	} while(flush != Z_FINISH);
	return 0;
}

static int out_stats(z_stream *strm, char *in_file, int in_fd)
{
	static bool first_time = true;
	int err;
	int ret = 0;
	char in[4096];
	char out[4096];
	char *out_path;
	ssize_t read_amt, write_amt = 0;
	int flush = Z_NO_FLUSH;

	uintmax_t compr_total = 0;
	uintmax_t uncompr_total = 0;

	int uintmax_width;

	gz_header header;

	for(uintmax_t n = UINTMAX_MAX; n > 9; n /= 10)
		uintmax_width++;

	out_path = input_to_output_path(in_file);
	if(!out_path) goto cleanup;

	inflateGetHeader(strm, &header);

	do {
		read_amt = read(in_fd, in, sizeof in);
		if(read_amt < 0) {
			report_error(0, "%s", in_file);
			ret = 1;
			goto cleanup;
		}
		if(read_amt == 0) flush = Z_FINISH;
		compr_total += read_amt;
		if(strm_push(strm, in, read_amt) < 0) {
			report_error(errno, "%s", in_file);
			ret = 1;
			goto cleanup;
		}
		if(write_amt == -1 && flush != Z_FINISH) {
			inflateReset(strm);
		}
		while((write_amt = strm_read(strm, out, sizeof out,
						flush)) > 0) {
			uncompr_total += write_amt;
			uncompr_total += sizeof out - strm->avail_out;
		}
		if(write_amt < -1) {
			report_error(0, "%s: %s", in_file, strm->msg);
			ret = 1;
			goto cleanup;
		}
	} while(flush != Z_FINISH);

	if(first_time) {
		if(opt_verbosity == 2) {
			printf("method   crc      date     time     ");
		}
		if(opt_verbosity > 0) {
			printf("%*.*s %*.*s  ratio  uncompressed name\n",
			       uintmax_width, uintmax_width, "compressed",
			       uintmax_width, uintmax_width, "uncompressed");
		}
		first_time = false;
	}

	if(opt_verbosity == 2) {
		struct tm *tm = localtime(&header.time);
		char *buf;
		size_t buf_alloc;
		size_t strftime_res;

		printf("deflate  %08lx ", strm->adler);

		buf = malloc(30);
		buf_alloc = 30;
		if(!buf) {
			report_error(errno, "%s", in_file);
			ret = 1;
			goto cleanup;
		}
		do {
			char *new_buf = grow_buf(buf, &buf_alloc);
			if(!new_buf) break;
			buf = new_buf;
			errno = 0;
		} while(!strftime(buf, buf_alloc, "%x %X ", tm) && errno == 0);
		if(errno) {
			report_error(errno, "%s", in_file);
			free(buf);
			ret = 1;
			goto cleanup;
		}
		printf("%s", buf);
		free(buf);
	}
	
	printf("%*"PRIuMAX" %*"PRIuMAX"  %5.2f  %s\n", uintmax_width,
	       compr_total, uintmax_width, uncompr_total,
	       ((double)(compr_total) / uncompr_total) * 100.0, out_path);

cleanup:
	free(out_path);

	return ret;
}

static int out_to_stdout(z_stream *strm, char *in_file, int in_fd)
{
	if(!opt_force && opt_compress && isatty(1)) {
		report_error(0, "compressed data not written to a terminal.\n"
		             "For help, type %s -h",
		             program_name);
		return 1;
	}
	return out_to_fd(strm, in_file, in_fd, "stdout", 1);
}

static int out_to_filename(z_stream *strm, char *in_file, int in_fd,
                           char *filename, time_t time)
{
	int out_fd, ret;

	out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC
	                      | (opt_force ? 0 : O_EXCL), 0666);
	if(out_fd < 0) {
		report_error(errno, "%s", filename);
		return 1;
	}

	ret = out_to_fd(strm, in_file, in_fd, filename, out_fd);	
	if(ret)
		goto cleanup;

	if(time != 0 && !opt_compress && opt_restore_name) {
		struct timespec timespecs[2] = {
			{.tv_nsec = UTIME_OMIT},
			{.tv_sec = time}
		};
		if(futimens(out_fd, timespecs)) {
			report_error(errno, "%s", filename);
			ret = 1;
			goto cleanup;
		}
	}

cleanup:
	if(ret)
		if(remove(filename))
			report_error(errno, "%s", filename);
	close(out_fd);
	return ret;

}

static int handle_stdin()
{
	int ret = 0;
	z_stream strm = {0};
	char *out_path = NULL;
	gz_header header = {
		.text = opt_ascii_text ? 0 : 1,
		.os = 3, /* Unix */
	};

	if(!opt_force && !opt_compress && isatty(0)) {
		report_error(0, "compressed data not read from a terminal.\n"
		             "For help, type: %s -h\n",
		             program_name);
		return 1;
	}

	if(init_stream(&strm))
		return 1;

	if(opt_list) {
		ret = out_stats(&strm, "stdin", 0);
		goto cleanup;
	}

	if(opt_restore_name && !opt_compress && !opt_stdout) {
		ret = read_header(&strm, &header, "stdin", 0);
		if(ret != 0) {
			goto cleanup;
		}
		if(header.name) {
			ret = out_to_filename(&strm, "stdin", 0, header.name, header.time);
			free(header.name);
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
	DIR *dir;
	struct dirent *dirent;
	int ret = 0;

	dir = fdopendir(in_fd);
	if(!dir) {
		report_error(errno, "%s", path);
		close(in_fd);
		return 1;
	}

	errno = 0;
	while(dirent = readdir(dir)) {
		char *buf;
		int tmp;

		if(strcmp(dirent->d_name, ".") == 0
		  || strcmp(dirent->d_name, "..") == 0)
			continue;

		if(asprintf(&buf, "%s/%s", path, dirent->d_name) == -1) {
			report_error(errno, "%s", path);
			ret = 1;
			// continue processing the directory before returning
			continue;
		}
		
		tmp = handle_path(buf);
		if(tmp) {
			ret = tmp;
		}
		free(buf);

		errno = 0;
	}

	if(errno) {
		report_error(errno, "%s", path);
		ret = 1;
	}

	closedir(dir);

	return ret;
}

static int handle_path(char *path)
{
	int in_fd;
	struct stat stat_buf;
	z_stream strm = {0};
	char *out_path = 0;
	int ret = 0;
	/* This header serves two purposes: first, when compressing it is used
	 * to set up what the output header will be. Second, when decompressing
	 * it serves as the storage for the input header.
	 */
	gz_header header = {
		.text = opt_ascii_text ? 0 : 1,
		.os = 3, /* Unix */
	};

	/* Use O_NOFOLLOW to implement the behavior that gzip doesn't
	 * compress symlinks
	 */
	if((in_fd = open(path, O_RDONLY | !opt_force ? O_NOFOLLOW : 0)) < 0) {
		/* O_NOFOLLOW returns ELOOP for symlinks, but that's
		 * a confusing message to give.
		 */
		if(!opt_force && errno == ELOOP) {
			struct stat stat_buf2;
			if(lstat(path, &stat_buf2)) {
				report_error(errno, "%s", path);
				return 1;
			} else if(S_ISLNK(stat_buf2.st_mode)) {
				report_error(0, "%s: is a link; not processing",
						path);
				return 1;
			} else {
				/* This condition should only occur with a race
				 * occuring. open was not able to open the file
				 * because of either recursive symlinks or the
				 * path it pointed to was a symlink. However,
				 * when we got its info with lstat we didn't
				 * fail, but the path was not a symlink.
				 *
				 * Just report the ELOOP error anyways, even
				 * though it has ceased to be true.
				 */
				report_error(ELOOP, "%s", path);
			}
		}
		report_error(errno, "%s", path);
		return 1;
	}

	if(fstat(in_fd, &stat_buf) == -1) {
		report_error(errno, "%s", path);
		ret = 1;
		goto cleanup_fd;
	}

	if(S_ISDIR(stat_buf.st_mode)) {
		if(opt_recursive) {
			return handle_dir(path, in_fd);
		} else {
			report_error(0, "%s: is a directory", path);
			ret = 2;
			goto cleanup_fd;
		}
	}

	if(init_stream(&strm))
		goto cleanup_fd;

	if(opt_list) {
		ret = out_stats(&strm, path, in_fd);
		goto cleanup_strm;
	}

	if(opt_stdout) {
		ret = out_to_stdout(&strm, path, in_fd);
		goto cleanup_strm;
	}

	if(!opt_compress) {
		if(opt_restore_name) {
			int res = read_header(&strm, &header, path, in_fd);
			if(res != 0) {
				ret = res;
				goto cleanup_strm;
			}
			header.name = 0;
		}
		out_path = input_to_output_path(path);
		if(!out_path) {
			report_error(errno, 0);
			ret = 2;
			goto cleanup_strm;
		}
	} else {
		int len;

		if(opt_store_name) {
			header.time = stat_buf.st_mtime;
		}

		deflateSetHeader(&strm, &header);

		len = asprintf(&out_path, "%s%s", path, opt_suffix);
		if(len < 0) {
			report_error(errno, 0);
			ret = 2;
			goto cleanup_strm;
		}
	}

	ret = out_to_filename(&strm, path, in_fd, out_path, header.time);

	if(ret == 0 && !opt_keep) {
		if(remove(path)) {
			report_error(errno, "%s");
			ret = 1;
		}
	}

cleanup_paths:
	free(header.name);
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

	setlocale(LC_CTYPE, "");
	setlocale(LC_MESSAGES, "");

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
			opt_compress = false;
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
		case 'S':
			opt_suffix = optarg;
			break;
		case 't':
			/* zlib always tests integrity, and I agree with this
			 * behavior. As such, just implement the option as a
			 * no-op.
			 */
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
		int tmp;
		if(strcmp(*v, "-") == 0) {
			tmp = handle_stdin();
		} else {
			tmp = handle_path(*v);
		}
		if(tmp > ret_val)
			ret_val = tmp;
	}
	return ret_val;
}
