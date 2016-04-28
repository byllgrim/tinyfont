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

typedef struct {
	Point *p0;
	Point *p1;
	Point *p2;
	int type;
} Command;

/* Function declarations */
static void parse();
static void parsechar();
static void parsesplines();
static void parsecommands();

/* Global variables */
static char *copyright;
static int ascent;
static int descent;
static char *strbuf;

/* Function definitions */
void
parse()
{
	char *end;
	char *key = strtok_r(strbuf, ": ", &end);

	if (!strcmp(key, "StartChar"))
		parsechar();
	else if (!strcmp(key, "Copyright"))
		strcpy(copyright, strtok(end, "\n")); //TODO strncpy?
	else if (!strcmp(key, "Ascent"))
		ascent = atoi(++end);
	else if (!strcmp(key, "Descent"))
		descent = atoi(++end);
}

void
parsechar()
{
	int codepoint, width;
	char *end;

	fgets(strbuf, BUFSIZ, stdin);
	strtok(strbuf, " "); strtok(NULL, " ");
	codepoint = atoi(strtok(NULL, " "));

	fgets(strbuf, BUFSIZ, stdin);
	strtok_r(strbuf, " ", &end);
	width = atoi(end);

	parsesplines(); /* TODO save structure? */
}

void /* TODO return splineset? */
parsesplines()
{
	char *prev = malloc(BUFSIZ);

	while (strcmp(prev, "Fore\n") && strcmp(strbuf, "SplineSet\n")) {
		strncpy(prev, strbuf, BUFSIZ);
		fgets(strbuf, BUFSIZ, stdin);
	}

	parsecommands();

	free(prev);
}

void
parsecommands()
{
	fgets(strbuf, BUFSIZ, stdin);
	while (strcmp(strbuf, "EndSplineSet\n")) {
		printf("parse: %s", strbuf);
		fgets(strbuf, BUFSIZ, stdin);
	}
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
