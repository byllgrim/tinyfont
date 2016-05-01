/* See LICENSE file */
#include <arpa/inet.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Function declarations */
static void die(const char *errstr, ...);
static void *ecalloc(size_t nmemb, size_t size);
static void readheader();

/* Global variables */
static char *buf;
static char *txt;
static FILE *fontfile;
static int em;
static int px;

/* Function definitions */
void
die(const char *fmt, ...) {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);

        if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
                fputc(' ', stderr);
                perror(NULL);
        }

        exit(EXIT_FAILURE);
}

void *
ecalloc(size_t nmemb, size_t size)
{
        void *p;
        if (!(p = calloc(nmemb, size)))
                perror(NULL);
        return p;
}

void
readheader()
{
	uint16_t skip;

	fread(buf, sizeof(char), 8, fontfile);
	if (strncmp(buf, "tinyfont", 8))
		die("not a tinyfont file\n");

	fread(buf, sizeof(uint16_t), 1, fontfile);
	skip = ntohs(*(uint16_t *)buf);
	fseek(fontfile, (long)skip, SEEK_CUR); /* copyright */

	fread(buf, sizeof(uint16_t), 1, fontfile);
	em = (int)ntohs(*(uint16_t *)buf);
}

int
main(int argc, char *argv[])
{
	if (argc != 4)
		die("usage: txt2ff fontfile px string\n");
	if (!(fontfile = fopen(argv[1], "r")))
		die("failed to open file\n");

	px = atoi(argv[2]);
	txt = argv[3];
	buf = ecalloc(1, BUFSIZ); /* TODO is it opposite? */

	readheader();

	return EXIT_SUCCESS;
}
