#include "getch.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int getch()
{
	char buf[GETCH_BUFFER_SIZE] = { 0 };

	int orig_lease_type;

	if ((orig_lease_type = fcntl(STDIN_FILENO, F_GETFL)) == -1) {
		return -1;
	}

	if (read(STDIN_FILENO, buf, 1) == -1) {
		return -1;
	}

	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1) {
		return -1;
	}

	int nread = read(STDIN_FILENO, buf+1, GETCH_BUFFER_SIZE - 1);

	if (fcntl(STDIN_FILENO, F_SETFL, orig_lease_type)) {
		return -1;
	}

	if (nread == -1 && errno != EAGAIN) {
		return -1;
	}

	if (buf[0] == INT_KEY_BACKSPACE)  {
		return KEY_BACKSPACE;
	}

	if (buf[0] == KEY_ESC) {
		if (buf[1] == 91) {
			switch(buf[2]) {
				case INT_KEY_HOME:
					return KEY_HOME;
				case INT_KEY_IC:
					return KEY_IC;
				case INT_KEY_DC:
					return KEY_DC;
				case INT_KEY_END:
					return KEY_END;
				case INT_KEY_PPAGE:
					return KEY_PPAGE;
				case INT_KEY_NPAGE:
					return KEY_NPAGE;
				case INT_KEY_UP:
					return KEY_UP;
				case INT_KEY_DOWN:
					return KEY_DOWN;
				case INT_KEY_LEFT:
					return KEY_LEFT;
				case INT_KEY_RIGHT:
					return KEY_RIGHT;
			}
		}
	}

	return buf[0];
}
