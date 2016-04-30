#include <arpa/inet.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	int codepoint; /* TODO uint16_t ? */
	int width;
	int length; /* length in bytes */
	Command *commands;
	Glyph *next;
};

/* Function declarations */
static void *ecalloc(size_t nmemb, size_t size);
static void parse();
static void parseglyph();
static int movetocommands();
static int parsecommands(Glyph *glyph);
static void writeheader();
static void writemap();

/* Global variables */
static char *copyright;
static int ascent;
static int descent;
static char *strbuf;
static Glyph *firstglyph;
static Glyph *lastglyph;
static int glyphcount = 0;

/* Function definitions */
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
	char *end;
	char *key = strtok_r(strbuf, ": ", &end);

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
	Glyph *glyph = ecalloc(1, sizeof(Glyph));
	glyph->length = 6; /* sizeof glyph header */
	char *end;

	fgets(strbuf, BUFSIZ, stdin);
	strtok(strbuf, " "); strtok(NULL, " ");
	glyph->codepoint = atoi(strtok(NULL, " "));

	fgets(strbuf, BUFSIZ, stdin);
	strtok_r(strbuf, " ", &end);
	glyph->width = atoi(end);

	if (movetocommands())
		glyph->length += parsecommands(glyph);

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
	char *line;
	Command *first = ecalloc(1, sizeof(Command)); /* TODO dummies/memory */
	Command *last = first;
	int length = 0;

	while (strcmp(strbuf, "EndSplineSet\n")) {
		Command *cmd = ecalloc(1, sizeof(Command));
		line = strbuf;
		float x = strtof(line, &line);
		float y = strtof(line, &line);
		char *t = strtok_r(line, " ", &line);

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
	fwrite("tinyfont", 1, 8, stdout);

	uint16_t length = (uint16_t)strlen(copyright);
	uint16_t n_length = htons(length); /* TODO this assumes LE */
	fwrite(&n_length, sizeof(uint16_t), 1, stdout);
	fwrite(copyright, 1, length, stdout);

	uint16_t em = (uint16_t)(ascent+descent);
	em = htons(em);
	fwrite(&em, sizeof(uint16_t), 1, stdout);
}

void
writemap()
{
	int i;
	uint16_t maplength, n_maplength, codepoint, offset, n_offset;
	Glyph *glyph = firstglyph->next; /* first is dummy */

	maplength = (uint16_t)(glyphcount * 4); /* 4 = bytes/entry */
	n_maplength = htons(maplength);
	fwrite(&n_maplength, sizeof(uint16_t), 1, stdout);

	for (offset = i = 0; i < glyphcount; i++) {
		codepoint = htons((uint16_t)glyph->codepoint);
		fwrite(&codepoint, sizeof(uint16_t), 1, stdout);

		n_offset = htons(offset);
		fwrite(&n_offset, sizeof(uint16_t), 1, stdout);
		offset += glyph->length;

		glyph = glyph->next;
	}
}

int
main()
{
	copyright = ecalloc(1, BUFSIZ);
	strbuf = ecalloc(1, BUFSIZ);
	firstglyph = ecalloc(1, sizeof(Glyph)); /* TODO dummies use memory */
	lastglyph = firstglyph;

	while(!feof(stdin)) {
		fgets(strbuf, BUFSIZ, stdin);
		parse(strbuf);
	}

	writeheader();
	writemap();
	//writeglyphs();

	/* TODO does one free buffers before return? */
	return 0;
}
