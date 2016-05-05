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

typedef struct Spline Spline;
struct Spline {
	float x0, y0;
	float x1, y1;
	float x2, y2;
	float x3, y3;
	Spline *next;
};

typedef struct {
	Rune p;
	int width;
	Spline *splines;
} Glyph;

typedef struct {
	int len;
	Glyph **map;
} GlyphMap;

/* Function declarations */
static void swap32(void *a);
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
static Spline *parsecommands(int len);
static Glyph *newglyph(Rune p, int w, Spline *s);
static void addglyph(Glyph *g);

/* Global variables */
static char *buf;
static char *txt;
static FILE *fontfile;
static OffsetMap *om;
static GlyphMap *gm;
static int em;
static int px;
static int glyphcount;
static long glyphsoffset;

/* Function definitions */
void
swap32(void *a)
{
	char *c = (char *)a;
	char *rev = malloc(4);
	memcpy(rev, a, 4);

	c[0] = rev[3];
	c[1] = rev[2];
	c[2] = rev[1];
	c[3] = rev[0];

	free(rev);
}

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

	gm = ecalloc(1, sizeof(GlyphMap));
	gm->len = glyphcount;
	gm->map = ecalloc(glyphcount, sizeof(Glyph **));

	while (*s) {
		s += chartorune(&p, s);
		loadglyph(p);
	}
}

void
loadglyph(Rune p)
{
	Spline *splines;
	uint16_t codepoint, width, cmdlen;
	long offset;
	/* TODO what if the glyph is already loaded? */

	offset = getoffset(p);
	fseek(fontfile, offset, SEEK_SET);

	fread(&codepoint, sizeof(uint16_t), 1, fontfile);
	codepoint = ntohs(codepoint);

	fread(&width, sizeof(uint16_t), 1, fontfile);
	width = ntohs(width);

	fread(&cmdlen, sizeof(uint16_t), 1, fontfile);
	cmdlen = ntohs(cmdlen)/4;
	splines = parsecommands(cmdlen);
	addglyph(newglyph((Rune)codepoint, (int)width, splines));
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

Spline *
parsecommands(int len)
{
	int i;
	float f, x0, y0;
	float *cmd = ecalloc(len*4, sizeof(float));
	Spline *new, *first, *splines;
	first = splines = malloc(sizeof(Spline)); /* dummy */
	fread(cmd, 1, len*4, fontfile);

	for (i = 0; i < len; i++) {
		f = cmd[i];
		swap32(&f); /* TODO this assumes LE */

		switch (*(char*)&f) {
		case 'c':
			new = malloc(sizeof(Spline));
			new->y3 = cmd[i-1]; new->x3 = cmd[i-2];
			new->y2 = cmd[i-3]; new->x2 = cmd[i-4];
			new->y1 = cmd[i-5]; new->x1 = cmd[i-6];
			new->y0 = y0; new->x0 = x0;
			swap32(&(new->x1)); swap32(&(new->y1));
			swap32(&(new->x2)); swap32(&(new->y2));
			swap32(&(new->x3)); swap32(&(new->y3));
			splines->next = new;
			splines = new;
			x0 = new->x3; y0 = new->y3;
			break;
		case 'l':
			new = malloc(sizeof(Spline));
			new->y0 = y0; new->x0 = x0;
			new->x1 = new->y1 = new->x2 = new->y2 = -1;
			new->y3 = cmd[i-1]; new->x3 = cmd[i-2];
			swap32(&(new->x3)); swap32(&(new->y3));
			splines->next = new;
			splines = new;
			x0 = new->x3; y0 = new->y3;
			break;
		case 'm':
			y0 = cmd[i-1]; x0 = cmd[i-2];
			swap32(&x0); swap32(&y0);
		}
	}

	free(cmd);
	return first;
}

Glyph *
newglyph(Rune p, int w, Spline *s)
{
	Glyph *g = ecalloc(1, sizeof(Glyph));
	g->p = p;
	g->width = w;
	g->splines = s;
	return g;
}

void
addglyph(Glyph *g)
{
	Glyph **map = gm->map;
	int rune, index, len;
	rune = g->p;
	len = gm->len;
	index = rune % len;

	while (map[index] && map[index]->p != rune)
		index++;

	map[index] = g;
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
