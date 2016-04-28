#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Types */
enum {curveto, lineto, moveto}; /* command type */

/* TODO structure for splinesets */

typedef struct {
	float x;
	float y;
} Point;

typedef struct Command Command;
struct Command {
	Point *p0;
	Point *p1;
	Point *p2;
	int type;
	Command *next;
};

typedef struct Glyph Glyph;
struct Glyph {
	int codepoint;
	int width;
	Command *commands;
	Glyph *next;
};

/* Function declarations */
static void parse();
static void parseglyph();
static void movetocommands();
static Command *parsecommands();

/* Global variables */
static char *copyright;
static int ascent;
static int descent;
static char *strbuf;
static Glyph *firstglyph;
static Glyph *lastglyph;

/* Function definitions */
void
parse()
{
	char *end;
	char *key = strtok_r(strbuf, ": ", &end);

	if (!strcmp(key, "StartChar"))
		parseglyph();
	else if (!strcmp(key, "Copyright"))
		strcpy(copyright, strtok(end, "\n")); //TODO strncpy?
	else if (!strcmp(key, "Ascent"))
		ascent = atoi(++end);
	else if (!strcmp(key, "Descent"))
		descent = atoi(++end);
}

void
parseglyph()
{
	Glyph *glyph = malloc(sizeof(Glyph));
	int codepoint, width;
	char *end;

	fgets(strbuf, BUFSIZ, stdin);
	strtok(strbuf, " "); strtok(NULL, " ");
	glyph->codepoint = atoi(strtok(NULL, " "));

	fgets(strbuf, BUFSIZ, stdin);
	strtok_r(strbuf, " ", &end);
	glyph->width = atoi(end);

	movetocommands();
	glyph->commands = parsecommands();
	/* TODO addglyph(glyph) */
}

void
movetocommands()
{
	char *prev = malloc(BUFSIZ);

	while (strcmp(prev, "Fore\n") && strcmp(strbuf, "SplineSet\n")) {
		strncpy(prev, strbuf, BUFSIZ);
		fgets(strbuf, BUFSIZ, stdin);
	}

	fgets(strbuf, BUFSIZ, stdin);
	free(prev);
}

Command *
parsecommands()
{
	while (strcmp(strbuf, "EndSplineSet\n")) {
		printf("parse: %s", strbuf);
		fgets(strbuf, BUFSIZ, stdin);
	}

	return NULL; /* TODO actually return commands */
}

int
main()
{
	copyright = malloc(BUFSIZ); /* TODO init()? */
	strbuf = malloc(BUFSIZ);

	while(!feof(stdin)) {
		fgets(strbuf, BUFSIZ, stdin);
		parse(strbuf);
	}

	//TODO does one free buffers before return?
	return 0;
}
