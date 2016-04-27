#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Function declarations */
static void parseheader();
static void parseglyphs();

/* Global variables */
static char *copyright = NULL;
static int ascent = -1;
static int descent = -1;

/* Function definitions */
void
parseheader()
{
	char *key, *end, *str = calloc(BUFSIZ, sizeof(char));

	/* TODO is this a bad way to indicate uninitialized state? */
	while(descent == -1 || ascent == -1 || copyright == NULL) {
		fgets(str, BUFSIZ, stdin);
		key = strtok_r(str, ": ", &end);

		if(!strcmp(key, "Copyright")) {
			copyright = malloc(strlen(++end) - 1);
			strcpy(copyright, strtok(end, "\n")); //TODO strncpy?
		}
		if(!strcmp(key, "Ascent"))
			ascent = atoi(++end);
		if(!strcmp(key, "Descent"))
			descent = atoi(++end);
	}
}

void
parseglyphs()
{
	/* parse glyphs:
	 *   'StartChar: '
	 *     ∟> 'Fore\nSplineSet'
	 *     ∟> 'EndSplineSet'
	 */
}

int
main()
{
	parseheader();
	parseglyphs();

	char *str = malloc(BUFSIZ);
	while(!feof(stdin)) {
		fgets(str, BUFSIZ, stdin);
	}

	return 0;
}
