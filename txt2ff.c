/* See LICENSE file */
#include <arpa/inet.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

/* Types */
typedef struct {
	int rune;
	long offset;
} OffsetEntry;

typedef struct {
	int len;
	OffsetEntry **map;
} OffsetMap;

/* Function declarations */
static void die(const char *errstr, ...);
static void *ecalloc(size_t nmemb, size_t size);
static void readheader();
static void readmap();
static OffsetMap *newoffsetmap(int length);
static OffsetEntry *newoffsetentry(int rune, long offset);
static void addoffsetentry(OffsetMap *om, OffsetEntry *oe);
static void readglyphs();
static void loadglyph(Rune p);
static long getoffset(Rune p);

/* Global variables */
static char *buf;
static char *txt;
static FILE *fontfile;
static int em;
static int px;
static int glyphcount;
static OffsetMap *om;
static long glyphsoffset;

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

void
readmap()
{
	uint16_t length, *map;
	int i, rune;
	long offset;

	fread(buf, sizeof(uint16_t), 1, fontfile);
	length = ntohs(*(uint16_t *)buf);
	glyphcount = length/4;
	om = newoffsetmap(glyphcount);

	map = ecalloc(length/2, sizeof(uint16_t));
	fread(map, sizeof(uint16_t), length/2, fontfile);
	for (i = 0; i < glyphcount; i+=2) {
		rune = (int)ntohs(map[i]);
		offset = (long)ntohs(map[i+1]);
		addoffsetentry(om, newoffsetentry(rune, offset));
	}

	glyphsoffset = ftell(fontfile);
}

OffsetMap *
newoffsetmap(int length) {
	OffsetMap *om = ecalloc(1, sizeof(OffsetMap));
	om->len = length;
	om->map = ecalloc(length, sizeof(OffsetEntry *));
	return om;
}

OffsetEntry *
newoffsetentry(int rune, long offset)
{
	OffsetEntry *oe = ecalloc(1, sizeof(OffsetEntry));
	oe->rune = rune;
	oe->offset = offset;
	return oe;
}

void
addoffsetentry(OffsetMap *om, OffsetEntry *oe)
{
	OffsetEntry **map = om->map;
	int rune, len, index;
	rune = oe->rune;
	len = om->len;
	index = rune % len;

	while (map[index] && map[index]->rune != rune)
		index++;

	map[index] = oe;
}

void
readglyphs()
{
	char *s = txt;
	Rune p;
	while (*s) {
		s += chartorune(&p, s);
		loadglyph(p);
	}
}

void
loadglyph(Rune p)
{
	uint16_t codepoint, width, cmdlen;
	long offset;

	offset = getoffset(p);
	fseek(fontfile, offset, SEEK_SET);

	fread(&codepoint, sizeof(uint16_t), 1, fontfile);
	codepoint = ntohs(codepoint);

	fread(&width, sizeof(uint16_t), 1, fontfile);
	width = ntohs(width);

	fread(&cmdlen, sizeof(uint16_t), 1, fontfile);
	cmdlen = ntohs(cmdlen);
	/* TODO readcommands */
}

long
getoffset(Rune p)
{
	OffsetEntry **map = om->map;
	int len, index;
	len = om->len;
	index = p % len;

	while (map[index] && map[index]->rune != p)
		index++;

	return index < len ? map[index]->offset + glyphsoffset : -1;
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
	readmap();
	readglyphs();
	/*render();*/
	/*writefile();*/

	return EXIT_SUCCESS;
}
