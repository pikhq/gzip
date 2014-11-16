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
	{"license",	no_argument,		NULL, 'L'},
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
static const char option_str[] = "acdfhklLnNqrS:tvV123456789";

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
		"  -L, --license        display software license\n"
		"  -n, --no-name        do not save or restore the original "
			"file name and time stamp\n"
		"  -N, --name           save or restore the original file "
			"name and time stamp (default)\n"
		"  -q, --quiet          suppress all warnings\n"
		"  -r, --recursive      operate recursively on directories\n"
		"  --rsyncable          generate rsync-friendly output\n"
		"  -S, --sufix=SUF      use suffix SUF on compressed files\n"
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

static void write_license()
{
	static const char license_msg[] =
		"Copyright (c) 2014, Josiah Worcester\n"
		"All rights reserved.\n"
		"\n"
		"Redistribution and use in source and binary forms, with or "
			"without\n"
		"modification, are permitted provided that the following "
			"conditions are met:\n"
		"\n"
		"1. Redistributions of source code must retain the above "
			"copyright notice, this\n"
		"   list of conditions and the following disclaimer.\n"
		"\n"
		"2. Redistributions in binary form must reproduce the above "
			"copyright notice,\n"
		"   this list of conditions and the following disclaimer in "
			"the documentation\n"
		"   and/or other materials provided with the distribution.\n"
		"\n"
		"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND "
			"CONTRIBUTORS \"AS IS\" AND\n"
		"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT "
			"LIMITED TO, THE IMPLIED\n"
		"WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR "
			"PURPOSE ARE\n"
		"DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR "
			"CONTRIBUTORS BE LIABLE FOR\n"
		"ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR "
			"CONSEQUENTIAL DAMAGES\n"
		"(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE "
			"GOODS OR SERVICES;\n"
		"LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) "
			"HOWEVER CAUSED AND ON\n"
		"ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT "
			"LIABILITY, OR TORT\n"
		"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT "
			"OF THE USE OF THIS\n"
		"SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH "
			"DAMAGE.\n";
	printf("%s", license_msg);
}

static int init_stream(struct z_stream_s *strm)
{
	if(opt_compress) {
		if(deflateInit2(strm, opt_level, Z_DEFLATED, 31, 8,
				Z_DEFAULT_STRATEGY) != Z_OK) {
			fprintf(stderr, "%s: %s\n", program_name, strm->msg);
			return 1;
		}
	} else {
		if(inflateInit2(strm, 31) != Z_OK) {
			fprintf(stderr, "%s: %s\n", program_name, strm->msg);
			return 1;
		}
	}
}

static int do_write(int fd, const void *buf, size_t count)
{
	while(count) {
		ssize_t n = write(fd, buf, count);
		if(n < 0 && errno == EINTR) continue;
		if(n < 0) { perror("write error"); return 1; }
		buf += n; count -= n;
	}
	return 0;
}

static int close_stream(struct z_stream_s *strm)
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

static int read_header(struct z_stream_s *strm, int in_fd)
{
	return 1;
}

static int out_to_fd(struct z_stream_s *strm, char *in_file, int in_fd,
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
				perror(in_file);
				ret = 1;
				goto cleanup;
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
					goto cleanup;
			} while(strm->avail_out == 0);
		} while(flush != Z_FINISH);
	} else {
		ssize_t read_amt, write_amt;
		int err;
		do {
			read_amt = read(in_fd, in, sizeof in);
			if(read_amt < 0) {
				perror(in_file);
				ret = 1;
				goto cleanup;
			}
			if(read_amt == 0) {
				if(opt_verbosity >= 1)
					fprintf(stderr, "%s: %s: bad input.\n",
						program_name, in_file);
				ret = 1;
				goto cleanup;
			}
			strm->next_in = in;
			strm->avail_in = read_amt;
			do {
				strm->avail_out = sizeof out;
				strm->next_out = out;
				err = inflate(strm, Z_NO_FLUSH);
				if(err != Z_OK && err != Z_STREAM_END) {
					if(opt_verbosity >= 1)
						fprintf(stderr, "%s: %s: bad "
								"input.\n",
							program_name, in_file);
					ret = 1;
					goto cleanup;
				}
				if(do_write(out_fd, out,
				            sizeof out - strm->avail_out) != 0)
					goto cleanup;
			} while(strm->avail_out == 0);
		} while(err != Z_STREAM_END);
	}

cleanup:
	close(in_fd);
	close(out_fd);
	close_stream(strm);
	return ret;
}

static int out_to_stdout(struct z_stream_s *strm, char *in_file, int in_fd)
{
	return out_to_fd(strm, in_file, in_fd, "stdout", 1);
}

static int out_to_filename(struct z_stream_s *strm, char *in_file, int in_fd,
                           char *filename)
{
	int out_fd;

	out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(out_fd < 0) {
		perror(filename);
		close_stream(strm);
		close(in_fd);
		return 1;
	}

	return out_to_fd(strm, in_file, in_fd, filename, out_fd);	
}

static int handle_stdin()
{
	struct z_stream_s strm = {0};
	char *out_path = NULL;

	init_stream(&strm);
	if(opt_restore_name && !opt_compress && !opt_stdout) {
		int res = read_header(&strm, 0);
		if(res != 0) {
			close_stream(&strm);
			return res;
		}
		struct gz_header_s header;
		inflateGetHeader(&strm, &header);
		if(header.name)
			return out_to_filename(&strm, "stdin", 0, header.name);
	}
	return out_to_stdout(&strm, "stdin", 0);
}

static int handle_dir(char *path, int in_fd)
{
	return 1;
}

static int handle_path(char *path)
{
	int in_fd;
	struct stat stat_buf;
	struct z_stream_s strm = {0};
	char out_path_buf[PATH_MAX] = {0};
	char *out_path = out_path_buf;
	int ret = 0;

	if((in_fd = open(path, O_RDONLY)) < 0) {
		perror(path);
		return 1;
	}

	if(fstat(in_fd, &stat_buf) == -1) {
		perror(path);
		ret = 1;
		goto cleanup_fd;
	}

	if((stat_buf.st_mode & S_IFMT) == S_IFDIR) {
		if(opt_recursive) {
			int ret = handle_dir(path, in_fd);
			close(in_fd);
			return ret;
		} else {
			if(opt_verbosity >= 1)
				fprintf(stderr, "%s: %s is a directory -- "
						"ignored.\n",
					program_name, path);
			ret = 2;
			goto cleanup_fd;
		}
	}

	init_stream(&strm);

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
			struct gz_header_s header;
			inflateGetHeader(&strm, &header);
			if(header.name)
				out_path = header.name;
		} else {
			if(snprintf(out_path_buf, PATH_MAX, "%s", path)
			    >= PATH_MAX) {
				fprintf(stderr, "%s: input path %s too long "
						"-- ignoring.\n",
					program_name, path);
				ret = 2;
				goto cleanup_strm;
			}
			remove_suffix(out_path, opt_suffix);
		}
	} else {
		char dir_path_buf[PATH_MAX];
		char *dir_path;
		int len;
		if(snprintf(dir_path_buf, PATH_MAX, "%s", path) >= PATH_MAX) {
			fprintf(stderr, "%s: input path %s too long -- "
					"ignoring.\n",
				program_name, path);
			ret = 2;
			goto cleanup_strm;
		}
		dir_path = dirname(dir_path_buf);
		if(strcmp(dir_path, ".") == 0) {
			len = snprintf(out_path_buf, PATH_MAX, "%s%s", path,
			               opt_suffix);
		} else {
			len = snprintf(out_path_buf, PATH_MAX, "%s/%s%s",
			               dir_path, path, opt_suffix);
		}
		if(len >= PATH_MAX) {
			if(opt_verbosity >= 1)
				fprintf(stderr, "%s: output path %s too long "
						"-- ignoring %s.\n",
					program_name, out_path, path);
			ret = 2;
			goto cleanup_strm;
		}
	}

	return out_to_filename(&strm, path, in_fd, out_path);

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
		case 'L':
			write_license();
			return 0;
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
