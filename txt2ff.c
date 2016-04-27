#include <stdio.h>

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("usage: txt2ff STRING\n");
		return 1;
	}

	printf("TODO: write '%s' to a ff image.\n", argv[1]);

	return 0;
}
