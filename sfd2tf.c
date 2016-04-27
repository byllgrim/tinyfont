#include <stdio.h>
#include <stdlib.h>

int
main()
{
	char *str = calloc(BUFSIZ, sizeof(char));

	while(!feof(stdin)) {
		printf("%s", str);
		fgets(str, BUFSIZ, stdin);

		/* parse header:
		 *   'Copyright: '
		 *   'Ascent: '
		 *   'Descent: '
		 *
		 * parse glyphs:
		 *   'StartChar: '
		 *     ∟> 'Fore\nSplineSet'
		 *     ∟> 'EndSplineSet'
		 */
	}

	return 0;
}
