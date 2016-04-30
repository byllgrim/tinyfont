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
	float x3, y3;
	int type;
	Command *next;
};

typedef struct Glyph Glyph;
struct Glyph {
	int codepoint; //TODO uint16_t ?
	int width;
	Command *commands;
	Glyph *next;
};

/* Function declarations */
static void parse();
static void parseglyph();
static int movetocommands();
static Command *parsecommands();
static void writeheader();
static void writeglyphs();

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
parse()
{
	char *end;
	char *key = strtok_r(strbuf, ": ", &end);

	if (!strcmp(key, "StartChar"))
		parseglyph();
	else if (!strcmp(key, "Copyright"))
		strcpy(copyright, strtok(end+1, "\n")); //TODO strncpy?
	else if (!strcmp(key, "Ascent"))
		ascent = atoi(++end);
	else if (!strcmp(key, "Descent"))
		descent = atoi(++end);
}

void
parseglyph()
{
	Glyph *glyph = malloc(sizeof(Glyph));
	char *end;

	fgets(strbuf, BUFSIZ, stdin);
	strtok(strbuf, " "); strtok(NULL, " ");
	glyph->codepoint = atoi(strtok(NULL, " "));

	fgets(strbuf, BUFSIZ, stdin);
	strtok_r(strbuf, " ", &end);
	glyph->width = atoi(end);

	if (movetocommands())
		glyph->commands = parsecommands();

	lastglyph->next = glyph;
	lastglyph = glyph;
	glyphcount++;
	//TODO glyphsize += commandsize?
}

int
movetocommands()
{
	char *prev = malloc(BUFSIZ);

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

Command *
parsecommands()
{
	char *line;
	float x0, y0;
	Command *first = malloc(sizeof(Command)); //TODO dummies use memory
	Command *last = first;

	while (strcmp(strbuf, "EndSplineSet\n")) {
		line = strbuf;
		float x = strtof(line, &line);
		float y = strtof(line, &line);
		char *t = strtok_r(line, " ", &line);

		if (!strcmp(t, "m")) {
			x0 = x; y0 = y;
		} else if (!strcmp(t, "l")) {
			Command *cmd = malloc(sizeof(Command));
			cmd->x0 = x0; cmd->y0 = y0;
			cmd->x3 = x; cmd->y3 = y;
			last->next = cmd;
			last = cmd;
			x0 = x; y0 = y;
			//TODO commandsize += 2?
		} else {
			Command *cmd = malloc(sizeof(Command));
			cmd->x0 = x0; cmd->y0 = y0;
			cmd->x1 = x; cmd->y1 = y;
			cmd->x2 = strtof(t, &t); cmd->y2 = strtof(line, &line);
			cmd->x3 = strtof(line, &line);
			cmd->y3 = strtof(line, &line);
			last->next = cmd;
			last = cmd;
			x0 = cmd->x3; y0 = cmd->y3;
			//TODO commandsize += 4?
		}

		fgets(strbuf, BUFSIZ, stdin);
	}

	return first;
}

void
writeheader()
{
	fwrite("tinyfont", 1, 8, stdout); //TODO consider printf

	uint16_t length = (uint16_t)strlen(copyright);
	uint16_t n_length = htons(length); //TODO this assumes LE
	fwrite(&n_length, sizeof(uint16_t), 1, stdout);
	fwrite(copyright, 1, length, stdout);

	uint16_t em = (uint16_t)(ascent+descent);
	em = htons(em);
	fwrite(&em, sizeof(uint16_t), 1, stdout);
}

void
writeglyphs()
{
	int i;
	uint16_t maplength, n_maplength, codepoint, offset;
	Glyph *glyph = firstglyph->next; //first is dummy

	maplength = (uint16_t)(glyphcount * 4); //4 = bytes/entry
	n_maplength = htons(maplength);
	fwrite(&n_maplength, sizeof(uint16_t), 1, stdout);

	for (offset = i = 0; i < glyphcount; i++) {
		fprintf(stderr, "codepoint: %d\n", glyph->codepoint);
		codepoint = htons((uint16_t)glyph->codepoint);
		fwrite(&codepoint, sizeof(uint16_t), 1, stdout);
		fwrite(&offset, sizeof(uint16_t), 1, stdout);
		//TODO offset += glyph->length;
		glyph = glyph->next; //TODO fix segfault
	}
}

int
main()
{
	copyright = malloc(BUFSIZ); /* TODO init()? */
	strbuf = malloc(BUFSIZ);
	firstglyph = malloc(sizeof(Glyph)); //TODO dummies use memory
	lastglyph = firstglyph;

	while(!feof(stdin)) {
		fgets(strbuf, BUFSIZ, stdin);
		parse(strbuf);
	}

	writeheader();
	writeglyphs();

	//TODO does one free buffers before return?
	return 0;
}
