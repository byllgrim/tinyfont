/* See LICENSE file */
#include <arpa/inet.h> /* TODO remove and diy? */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

/* Types */
enum {curveto, lineto, moveto}; /* command type */

typedef struct Command Command;
struct Command {
	float x0, y0;
	float x1, y1;
	float x2, y2;
	int type;
	Command *next;
};

typedef struct Glyph Glyph;
struct Glyph {
	Rune rune;
	int width;
	int cmdlen; /* length in bytes */
	Command *commands;
	Glyph *next;
};

/* Function declarations */
static void swap32(void *a);
static void *ecalloc(size_t nmemb, size_t size);
static void parse();
static void parseglyph();
static int movetocommands();
static int parsecommands(Glyph *glyph);
static void writeheader();
static void writemap();
static void writeglyphs();
static void writecommands(Glyph *glyph);

/* Global variables */
static char *copyright;
static int ascent;
static int descent;
static char *strbuf;
static Glyph *firstglyph;
static Glyph *lastglyph;
static int glyphcount = 0;

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

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		perror(NULL);
	return p;
}

void
parse()
{
	char *end, *key;
	key = strtok_r(strbuf, ": ", &end);

	if (!strcmp(key, "StartChar"))
		parseglyph();
	else if (!strcmp(key, "Copyright"))
		strncpy(copyright, strtok(end+1, "\n"), BUFSIZ);
	else if (!strcmp(key, "Ascent"))
		ascent = atoi(++end);
	else if (!strcmp(key, "Descent"))
		descent = atoi(++end);
}

void
parseglyph()
{
	char *end;
	Glyph *glyph = ecalloc(1, sizeof(Glyph));
	glyph->cmdlen = 0; /* exclude sizeof glyph header */

	fgets(strbuf, BUFSIZ, stdin);
	strtok(strbuf, " "); strtok(NULL, " ");
	glyph->rune = atoi(strtok(NULL, " "));

	fgets(strbuf, BUFSIZ, stdin);
	strtok_r(strbuf, " ", &end);
	glyph->width = atoi(end);

	if (movetocommands())
		glyph->cmdlen += parsecommands(glyph);

	lastglyph->next = glyph;
	lastglyph = glyph;
	glyphcount++;
}

int
movetocommands()
{
	char *prev = ecalloc(1, BUFSIZ);

	while (strcmp(prev, "Fore\n") || strcmp(strbuf, "SplineSet\n")) {
		if (!strcmp(strbuf, "EndChar\n")) {
			free(prev);
			return 0;
		}

		strncpy(prev, strbuf, BUFSIZ);
		fgets(strbuf, BUFSIZ, stdin);
	}

	fgets(strbuf, BUFSIZ, stdin);
	free(prev);
	return 1;
}

int
parsecommands(Glyph *glyph)
{
	char *line, *t;
	Command *first, *last, *cmd;
	int length = 0;
	float x, y;

	first = ecalloc(1, sizeof(Command)); /* TODO dummies/memory */
	last = first;

	while (strcmp(strbuf, "EndSplineSet\n")) {
		cmd = ecalloc(1, sizeof(Command));
		line = strbuf;
		x = strtof(line, &line);
		y = strtof(line, &line);
		t = strtok_r(line, " ", &line);

		if (!strcmp(t, "m")) {
			cmd->x0 = x; cmd->y0 = y;
			cmd->type = moveto;
			length += 12; /* sizeof moveto */
		} else if (!strcmp(t, "l")) {
			cmd->x0= x; cmd->y0 = y;
			cmd->type = lineto;
			length += 12; /* sizeof lineto */
		} else {
			cmd->x0 = x; cmd->y0 = y;
			cmd->x1 = strtof(t, &t); cmd->y1 = strtof(line, &line);
			cmd->x2 = strtof(line, &line);
			cmd->y2 = strtof(line, &line);
			cmd->type = curveto;
			length += 28; /* sizeof curveto */
		}

		last->next = cmd;
		last = cmd;
		fgets(strbuf, BUFSIZ, stdin);
	}

	glyph->commands = first;
	return length;
}

void
writeheader()
{
	uint16_t length, n_length, em;

	fwrite("tinyfont", 1, 8, stdout);

	length = (uint16_t)strlen(copyright);
	n_length = htons(length); /* TODO this assumes LE */
	fwrite(&n_length, sizeof(uint16_t), 1, stdout);
	fwrite(copyright, 1, length, stdout);

	em = (uint16_t)(ascent+descent);
	em = htons(em);
	fwrite(&em, sizeof(uint16_t), 1, stdout);
}

void
writemap()
{
	int i;
	uint16_t maplength, n_maplength, rune, offset, n_offset;
	Glyph *glyph = firstglyph->next; /* first is dummy */

	maplength = (uint16_t)(glyphcount * 4); /* 4 = bytes/entry */
	n_maplength = htons(maplength);
	fwrite(&n_maplength, sizeof(uint16_t), 1, stdout);

	for (offset = i = 0; i < glyphcount; i++) {
		rune = htons((uint16_t)glyph->rune);
		fwrite(&rune, sizeof(uint16_t), 1, stdout);

		n_offset = htons(offset);
		fwrite(&n_offset, sizeof(uint16_t), 1, stdout);
		offset += glyph->cmdlen + 6; /* sizeof(header) = 6 */

		glyph = glyph->next;
	}
}

void
writeglyphs()
{
	uint16_t rune, width, length;
	Glyph *glyph = firstglyph->next; /* skip the dummy */

	while (glyph) {
		rune = htons((uint16_t)glyph->rune);
		fwrite(&rune, sizeof(uint16_t), 1, stdout);

		width = htons((uint16_t)glyph->width);
		fwrite(&width, sizeof(uint16_t), 1, stdout);

		length = htons((uint16_t)glyph->cmdlen);
		fwrite(&length, sizeof(uint16_t), 1, stdout);

		writecommands(glyph);
		glyph = glyph->next;
	}
}

void
writecommands(Glyph *glyph)
{
	float points[6]; /* TODO malloc? */
	Command *cmd = glyph->commands;
	uint32_t c='c', l='l', m='m';
	swap32(&c); swap32(&l); swap32(&m);

	if (!cmd)
		return;
	else
		cmd = cmd->next; /* '->next' skips dummy */

	while (cmd) {
		points[0] = cmd->x0; points[1] = cmd->y0;
		swap32(&points[0]); swap32(&points[1]);

		switch (cmd->type) {
		case curveto:
			points[2] = cmd->x1; points[3] = cmd->y1;
			points[4] = cmd->x2; points[5] = cmd->y2;
			swap32(&points[2]); swap32(&points[3]);
			swap32(&points[4]); swap32(&points[5]);
			fwrite(&points, sizeof(float), 6, stdout);
			fwrite(&c, sizeof(uint32_t), 1, stdout);
			break;
		case lineto:
			fwrite(&points, sizeof(float), 2, stdout);
			fwrite(&l, sizeof(uint32_t), 1, stdout);
			break;
		case moveto:
			fwrite(&points, sizeof(float), 2, stdout);
			fwrite(&m, sizeof(uint32_t), 1, stdout);
			break;
		}

		cmd = cmd->next;
	}
}

int
main()
{
	copyright = ecalloc(1, BUFSIZ);
	strbuf = ecalloc(1, BUFSIZ);
	firstglyph = ecalloc(1, sizeof(Glyph)); /* TODO dummies use memory */
	lastglyph = firstglyph;

	while (!feof(stdin)) {
		fgets(strbuf, BUFSIZ, stdin);
		parse(strbuf);
	}

	writeheader();
	writemap();
	writeglyphs();

	/* TODO does one free buffers before return? */
	return 0;
}
