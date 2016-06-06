/* See LICENSE file */
#include <arpa/inet.h>

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

/* Macros */
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) < (b) ? (b) : (a))
#define INRANGE(t) (0 <= t && t <= 1)

/* Types */
typedef union {
	char c[BUFSIZ];
	uint16_t i;
} Buf;

typedef union {
	float f;
	uint32_t i;
} cmdtype;

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

typedef struct {
	int w;
	int h;
	char **pxl; /* row-aligned */
} Image;

typedef struct Node Node;
struct Node {
	int i;
	Node *next;
};

/* Function declarations */
static void swap32(void *a);
static void die(const char *errstr, ...);
static void *ecalloc(size_t nmemb, size_t size);

static void readheader(void);
static void readmap(void);
static OffsetMap *newoffsetmap(int length);
static OffsetEntry *newoffsetentry(int rune, long offset);
static void addoffsetentry(OffsetMap *om, OffsetEntry *oe);
static void readglyphs(void);
static void loadglyph(Rune p);
static long getoffset(Rune p);
static Spline *parsecommands(int len);
static void maxminy(Spline *s);
static Glyph *newglyph(Rune p, int w, Spline *s);
static void addglyph(Glyph *g);

static void initimage(void);
static Glyph *getglyph(Rune p);
static void drawglyphs(void);
static void drawsplines(Spline *s, int hshift);
static void drawline(Spline *s, int hshift);
static void drawcurve(Spline *s, int hshift);
static void fillrow(Spline *s, int y, int width, int hshift);

static Node *findroots(Spline *s, int y);
static Node *linroot(Spline *s, int y);
static Node *cuberoots(Spline *s, int y);
static Node *listroots(Spline *s, float *roots);
static int isinlist(int x, Node *n);
static Node *cleanlist(Node *n);

static void writefile();

/* Global variables */
static Buf buf;
static char *txt;
static FILE *fontfile;
static FILE *outfile;
static OffsetMap *om;
static GlyphMap *gm;
static Image *img;
static int em;
static int px;
static int maxy;
static int miny;
static int glyphcount;
static long glyphsoffset;
static double scale;
static uint16_t white[4] = {-1, -1, -1, -1};
static uint16_t black[4] = {0, 0, 0, -1};

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
readheader(void)
{
	if (!fread(buf.c, sizeof(char), 8, fontfile)) return;
	if (strncmp(buf.c, "tinyfont", 8))
		die("not a tinyfont file\n");

	if (!fread(buf.c, sizeof(uint16_t), 1, fontfile)) return;
	em = (int)ntohs(buf.i);
	scale = ((double)px)/em;
	maxy = px;
	miny = 0;
}

void
readmap(void)
{
	uint16_t maplen, *map;
	int i, rune;
	long offset;

	if (!fread(buf.c, sizeof(uint16_t), 1, fontfile)) return;
	maplen = ntohs(buf.i);
	glyphcount = maplen/4;
	om = newoffsetmap(glyphcount);

	map = ecalloc(maplen/2, sizeof(uint16_t));
	if (!fread(map, sizeof(uint16_t), maplen/2, fontfile)) return;
	for (i = 0; i < maplen/2; i+=2) {
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

	while (1) {
		if (map[index] && map[index]->rune != rune) {
			if (index == (len - 1))
				index = 0;
			else
				index++;
		}
		if (!map[index] || (map[index] && map[index]->rune == rune))
			break;
	}

	map[index] = oe;
}

void
readglyphs(void)
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
	uint16_t rune, width, cmdlen;
	long offset;
	/* TODO what if the glyph is already loaded? */

	offset = getoffset(p);
	fseek(fontfile, offset, SEEK_SET);

	if (!fread(&rune, sizeof(uint16_t), 1, fontfile)) return;
	rune = ntohs(rune);

	if (!fread(&width, sizeof(uint16_t), 1, fontfile)) return;
	width = ntohs(width);

	if (!fread(&cmdlen, sizeof(uint16_t), 1, fontfile)) return;
	cmdlen = ntohs(cmdlen)/4;
	splines = parsecommands(cmdlen);
	addglyph(newglyph((Rune)rune, (int)width, splines));
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

	/* TODO should properly handle out of bounds instead of this */
	return index < len ? map[index]->offset + glyphsoffset : -1;
}

Spline *
parsecommands(int len)
{
	int i;
	cmdtype t;
	float x0, y0;
	float *cmd = ecalloc(len*4, sizeof(float));
	Spline *new, *first, *splines;
	first = splines = ecalloc(1, sizeof(Spline)); /* dummy */
	if (!fread(cmd, 1, len*4, fontfile)) return first;

	for (i = 0; i < len; i++) {
		t.f = cmd[i];
		swap32(&t.f); /* TODO this assumes LE */

		switch (t.i) {
		case 'c':
			new = ecalloc(1, sizeof(Spline));
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
			maxminy(new);
			break;
		case 'l':
			new = ecalloc(1, sizeof(Spline));
			new->y0 = y0; new->x0 = x0;
			new->x1 = new->y1 = new->x2 = new->y2 = 0; /* -1 ? */
			new->y3 = cmd[i-1]; new->x3 = cmd[i-2];
			swap32(&(new->x3)); swap32(&(new->y3));
			splines->next = new;
			splines = new;
			x0 = new->x3; y0 = new->y3;
			maxminy(new);
			break;
		case 'm':
			y0 = cmd[i-1]; x0 = cmd[i-2];
			swap32(&x0); swap32(&y0);
		}
	}

	free(cmd);
	splines->next = NULL;
	return first;
}

void
maxminy(Spline *s)
{
	maxy = MAX(maxy, scale*s->y0);
	maxy = MAX(maxy, scale*s->y1);
	maxy = MAX(maxy, scale*s->y2);
	maxy = MAX(maxy, scale*s->y3);
	miny = MIN(miny, scale*s->y0);
	miny = MIN(miny, scale*s->y1);
	miny = MIN(miny, scale*s->y2);
	miny = MIN(miny, scale*s->y3);
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

void
initimage(void)
{
	int width = 0, i; /* TODO does one need to say '= 0' explicitly? */
	Rune p;
	Glyph *g;
	char *s = txt;

	img = ecalloc(1, sizeof(Image));
	img->h = maxy - miny + 1;
	img->pxl = ecalloc(img->h, sizeof(char *));

	while (*s) {
		s += chartorune(&p, s);
		if ((g = getglyph(p)))
			width += scale*(g->width);
	}
	img->w = width;

	for (i = 0; i < img->h; i++) {
		img->pxl[i] = ecalloc(width, sizeof(char));
	}
}

Glyph *
getglyph(Rune p)
{
	Glyph **map = gm->map;
	int len, index;
	len = gm->len;
	index = p % len;

	while (map[index] && map[index]->p != p)
		index++;

	/* TODO handle outofbounds separate from nonexistant glyph */
	return index < len ? map[index] : NULL;
}

void
drawglyphs(void)
{
	int y, hshift = 0;
	Rune p;
	Glyph *g;
	char *s = txt;

	while (*s) {
		s += chartorune(&p, s);
		if ((g = getglyph(p))) {
			drawsplines(g->splines, hshift);
			for (y = miny; y <= maxy; y++)
				fillrow(g->splines, y, scale*g->width, hshift);
			hshift += scale*(g->width);
		}
	}
}

void
drawsplines(Spline *s, int hshift)
{
	while ((s = s->next)) { /* first run deliberately skips dummy */
		if (!s->x1 && !s->y1 && !s->x2 && !s->y2)
			drawline(s, hshift);
		else
			drawcurve(s, hshift);
	}
}

void
drawline(Spline *s, int hshift)
{
	int i, h;
	float t, x, y, x0, y0, x3, y3;
	x0 = s->x0; y0 = s->y0;
	x3 = s->x3; y3 = s->y3;
	h = img->h;

	for (i = 0; i < h; i++) {
		t = ((float)i)/h;
		x = x0*(1-t) + x3*t;
		y = y0*(1-t) + y3*t;
		x = (scale*x)+hshift;
		y = (px-scale*y); /* px-1 ? */
		img->pxl[(int)(y)][(int)x] = 1;
	}
}

void
drawcurve(Spline *s, int hshift)
{
	int i, h, step;
	float d1, d2, d3, t1, t2, t3, x, y, x0, y0, x1, y1, x2, y2, x3, y3;
	h = img->h;
	step = h*2;
	x0 = s->x0; y0 = s->y0;
	x1 = s->x1; y1 = s->y1;
	x2 = s->x2; y2 = s->y2;
	x3 = s->x3; y3 = s->y3;

	for (i = 0; i < step; i++) {
		t1 = ((float)i)/step; t2 = t1*t1; t3 = t1*t2;
		d1 = (1-t1); d2 = d1*d1; d3 = d1*d2;
		x = x0*d3 + x1*3*t1*d2 + x2*3*t2*d1 + x3*t3;
		y = y0*d3 + y1*3*t1*d2 + y2*3*t2*d1 + y3*t3;
		x = (scale*x)+hshift;
		y = (px - scale*y); /* px-1 ? */
		img->pxl[(int)(y)][(int)x] = 1;
	}
}

void
fillrow(Spline *s, int y, int width, int hshift)
{
	int x;
	int evenodd = 0;
	Node *roots, *tmp;
	roots = ecalloc(1, sizeof(Node));

	while ((s = s->next)) { /* first run deliberately skips dummy */
		tmp = roots; /* is this stupid? */
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = findroots(s, y);
	}
	roots = cleanlist(roots);

	for (x = 0; x < width; x++) {
		if (isinlist(x, roots))
			evenodd++;

		if (evenodd%2)
			img->pxl[(int)(px-y)][x+hshift] = 1; /* px-1 ? */
	}
}

Node *
findroots(Spline *s, int y)
{
	if ((s->x1 == 0) && (s->y1 == 0) && (s->x2 == 0) && (s->y2 == 0))
		return linroot(s, y);
	else
		return cuberoots(s, y);
}

Node *
linroot(Spline *s, int y)
{
	Node *root = ecalloc(1, sizeof(Node));
	float y0 = scale*s->y0, y3 = scale*s->y3;
	float t = -(y0 - y)/(y3 - y0);

	if (INRANGE(t))
		root->i = scale*(s->x0*(1-t) + s->x3*t);
	else
		root->i = -1;

	return root;
}

Node *
cuberoots(Spline *s, int y)
{
	float y0 = scale*s->y0, y1 = scale*s->y1,
	      y2 = scale*s->y2, y3 = scale*s->y3;
	float pa = -y0 + 3*y1 - 3*y2 + y3,
	      pb = 3*y0 - 6*y1 + 3*y2,
	      pc = -3*y0 + 3*y1,
	      pd = y0 - y;
	float a = pb/pa, b = pc/pa, c = pd/pa;
	float p = b - (a*a)/3,
	      q = c + (2*a*a*a - 9*a*b)/27,
	      p3 = p/3,
	      q2 = q/2,
	      D = p3*p3*p3 + q2*q2; /* discriminant */
	float mp3, mp33, r, t, cosphi, phi, crtr, t1, sd,
	      u1, v1, roots[3];
	roots[0] = roots[1] = roots[2] = -1;

	if (D > 0 || (p == 0 && q == 0) ) {
		sd = sqrt(D);
		u1 = cbrt(sd - q2);
		v1 = cbrt(sd + q2);
		roots[0] = u1 - v1 - a/3;
	} else if (D < 0) {
		mp3 = -p/3;
		mp33 = mp3*mp3*mp3;
		r = sqrt( mp33 );
		t = -q / (2*r);
		cosphi = t<-1 ? -1 : t>1 ? 1 : t;
		phi = acos(cosphi);
		crtr = cbrt(r);
		t1 = 2*crtr;
		roots[0] = t1 * cos(phi/3) - a/3;
		roots[1] = t1 * cos((phi+2*M_PI)/3) - a/3;
		roots[2] = t1 * cos((phi+4*M_PI)/3) - a/3;
	} else if (D == 0) {
		u1 = q2 < 0 ? cbrt(-q2) : -cbrt(q2);
		roots[0] = 2*u1 - a/3;
		roots[1] = -u1 - a/3;
	} else {
		printf("what? D = %f\n", D);
	}

	return listroots(s, (float *)&roots);
}

Node *
listroots(Spline *s, float *roots)
{
	/* the datastructures for roots could be improved */
	int i;
	float t, t2, t3, tt, tt2, tt3;
	Node *dummy, *end;
	dummy = end = ecalloc(1, sizeof(Node));

	for (i = 0; i < 3; i++) {
		if (INRANGE(roots[i])) {
			t = roots[i]; t2 = t*t; t3 = t2*t;
			tt = 1-t; tt2 = tt*tt; tt3 = tt2*tt;
			end->next = ecalloc(1, sizeof(Node));
			end = end->next;
			end->i = scale*(s->x0*tt3 + 3*s->x1*tt2*t
			                + 3*s->x2*tt*t2 + s->x3*t3);
		}
	}

	end = dummy->next;
	free(dummy);
	return end; /* did all of this go right? */
}

int
isinlist(int x, Node *n)
{
	if (!n)
		return 0;

	for (n = n->next; n; n = n->next) {
		if (n->i == x)
			return 1;
	}
	return 0;
}

Node *
cleanlist(Node *n)
{
	/* TODO remove duplicates */
	Node *start, *tmp;

	while (n->i == -1) {
		tmp = n;
		n = n->next; /* is this safe? */
		free(tmp);
	}
	start = n;

	while (n->next) {
		if (n->next->i == -1) {
			tmp = n->next;
			n->next = n->next->next;
			free(tmp);
		} else {
			n = n->next;
		}
	}

	return start;
}

void
writefile(void)
{
	int i, j;
	uint32_t width, height;
	char magic[8] = "farbfeld";

	fwrite(magic, sizeof(char), 8, outfile);
	width = htonl((uint32_t)img->w);
	height = htonl((uint32_t)img->h);
	fwrite(&width, sizeof(uint32_t), 1, outfile);
	fwrite(&height, sizeof(uint32_t), 1, outfile);

	for (i = 0; i < img->h; i++) {
		for (j = 0; j < img->w; j++) {
			if (!img->pxl[i][j])
				fwrite(&white, sizeof(uint16_t), 4, outfile);
			else
				fwrite(&black, sizeof(uint16_t), 4, outfile);
		}
	}
}

int
main(int argc, char *argv[])
{
	if (argc != 4)
		die("usage: txt2ff fontfile px string\n");
	if (!(fontfile = fopen(argv[1], "r")))
		die("failed to open fontfile\n");
	if (!(outfile = fopen("out.ff", "w")))
		die("failed to open outfile\n");

	px = atoi(argv[2]);
	txt = argv[3];

	readheader();
	readmap();
	readglyphs();
	initimage();
	drawglyphs();
	writefile();

	return EXIT_SUCCESS;
}
