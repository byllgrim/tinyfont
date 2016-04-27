#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Types */
/* TODO structure for splinesets */

/* Function declarations */
static void parse();
static void parsechar();
static void parsesplines();

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

	/* parse glyphs:
	 *   'StartChar: '
	 *     ∟> 'Fore\nSplineSet'
	 *     ∟> 'EndSplineSet'
	 */
}

void /* TODO return splineset? */
parsesplines()
{
	//
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

	return 0;
}
