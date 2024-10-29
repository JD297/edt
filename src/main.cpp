#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
	if(argc >= 3)
	{
		fprintf(stderr, "%s: invalid argument -- \'%s\'\n", TARGET, argv[2]);
		fprintf(stderr, "usage: %s [path/to/file]\n", TARGET);

		exit(1);
	}
	else if(argc == 2) {
		// TODO load_file
	}

	return 0;
}
