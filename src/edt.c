#include <curses.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define ctrl(x) ((x) & 0x1f)

typedef struct edt_state {
	WINDOW* weditor;
	WINDOW* wstatus;

	int key;

	struct stat sb;

	char *pathname;

	char *buf;

	size_t nbuf;

	char *p_buf;
} edt_state;

#define EDT_WSTATUS_HEIGHT 1

void edt_state_init_windows(edt_state *edt)
{
	int maxx = getmaxx(stdscr);
	int maxy = getmaxy(stdscr);

	edt->weditor = newwin(maxy - EDT_WSTATUS_HEIGHT, maxx, 0, 0);
	edt->wstatus = newwin(EDT_WSTATUS_HEIGHT, maxx, maxy - EDT_WSTATUS_HEIGHT, 0);

	refresh();

	keypad(edt->weditor, true);
	scrollok(edt->weditor, true);
}

void ncurses_init()
{
	initscr();
	noecho();
	raw();
}

void edt_display(edt_state *edt)
{
	wclear(edt->weditor);

	int x, y;

	for (char *i = edt->buf; *i != '\0'; i++) {
		if (i == edt->p_buf) {
			getyx(edt->weditor, y, x);
		}

		wprintw(edt->weditor, "%c", *i);
	}

	wmove(edt->weditor, y, x);
}

void edt_open(edt_state *edt, char* pathname)
{
	if (stat(pathname, &edt->sb) == -1) {
		err(EXIT_FAILURE, "%s", pathname);
	}

	if ((edt->sb.st_mode & S_IFMT) != S_IFREG) {
		fprintf(stderr, "%s: %s: %s\n", TARGET, pathname, "not a regular file");
		exit(EXIT_FAILURE);
	}

	int fd;

	if ((fd = open(pathname, O_RDONLY)) == -1) {
		err(EXIT_FAILURE, "%s", pathname);
	}

	edt->nbuf = ((edt->sb.st_size / sysconf(_SC_PAGESIZE)) + 1) * 4 * sysconf(_SC_PAGESIZE);
	edt->buf = (char *)mmap(NULL, edt->nbuf, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	if (edt->buf == MAP_FAILED) {
		err(EXIT_FAILURE, "%s", pathname);
	}

	if (read(fd, edt->buf, edt->sb.st_size) == -1) {
		err(EXIT_FAILURE, "%s", pathname);
	}

	edt->p_buf = edt->buf;

	if (close(fd) == -1) {
		err(EXIT_FAILURE, "%s", pathname);
	}

	edt->pathname = pathname;
}

void edt_event_left(edt_state *edt)
{
	if (edt->p_buf == edt->buf) {
	        return;
	}

	edt->p_buf--;
}

void edt_event_right(edt_state *edt)
{
	if (*edt->p_buf == '\0') {
		return;
	}

	edt->p_buf++;
}

void edt_event_delete(edt_state *edt)
{
	if (*edt->p_buf == '\0') {
                return;
        }

	memmove(edt->p_buf, edt->p_buf + 1, strlen(edt->p_buf));
}

void edt_event_backspace(edt_state *edt)
{
	if (edt->p_buf == edt->buf) {
		return;
	}

	memmove(edt->p_buf - 1, edt->p_buf, strlen(edt->p_buf));

	edt->p_buf--;
}

void edt_event_home(edt_state *edt)
{
	char *last = NULL;

	while (1) {
		char *pos;

		if (last == NULL) {
			pos = strstr(edt->buf, "\n");
		} else {
			pos = strstr(last + 1, "\n");
		}

		if (pos == NULL) {
			break;
		}

		if (edt->p_buf <= pos) {
			break;
		}

		last = pos;
	}

	if (last == NULL) {
		edt->p_buf = edt->buf;

		return;
	}

	edt->p_buf = last + 1;
}

void edt_event_end(edt_state *edt)
{
	char *pos = strstr(edt->p_buf, "\n");

	if (pos == NULL) {
		// TODO set to end of buf

		return;
	}

	edt->p_buf = pos;
}

void edt_event_write(edt_state *edt)
{
	if(!(edt->key == 10 || edt->key == 9 || (edt->key >= 32 && edt->key <= 126))) {
		return;
	}

	if (edt->nbuf <= strlen(edt->buf)) {
		// TODO mremap
		return;
	}

	memmove(edt->p_buf + 1, edt->p_buf, strlen(edt->p_buf));

	*edt->p_buf = (char)edt->key;

	edt->p_buf++;
}

void print_usage()
{
	fprintf(stderr, "Usage: %s [FILE]\n", TARGET);
	fprintf(stderr, "Visual text editor\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "JD297 %s source code <https://github.com/jd297/edt>\n", TARGET);
}

int main (int argc, char *argv[])
{
	if(argc != 2)
	{
		print_usage();

		exit(EXIT_FAILURE);
	}

	edt_state edt;

	edt_open(&edt, argv[1]);

	ncurses_init();

	edt_state_init_windows(&edt);

	wprintw(edt.wstatus, "\"%s\" %ldB %ldR", edt.pathname, edt.sb.st_size, edt.nbuf);
	wrefresh(edt.wstatus);

	while (edt.key != ctrl('x')) {
		edt_display(&edt);

		edt.key = wgetch(edt.weditor);

		switch(edt.key)
		{
			case KEY_LEFT:
				edt_event_left(&edt);
			break;
			case KEY_RIGHT:
				edt_event_right(&edt);
			break;
			/*case KEY_UP:
				event_up(state);
			break;*/
			/*case KEY_DOWN:
				event_down(state);
			break;*/
			/*case KEY_PPAGE:
				event_page_up(state);
			break;*/
			/*case KEY_NPAGE:
				event_page_down(state);
			break;*/
			case KEY_BACKSPACE:
				edt_event_backspace(&edt);
			break;
			case KEY_DC:
				edt_event_delete(&edt);
			break;
			case KEY_HOME:
				edt_event_home(&edt);
			break;
			case KEY_END:
				edt_event_end(&edt);
			break;
			/*case ctrl('s'):
				edt_write(state);
			break;*/
			default:
				edt_event_write(&edt);
		}
	}

	if (munmap(edt.buf, edt.nbuf) == -1) {
		err(EXIT_FAILURE, "%s", edt.pathname);
	}

	endwin();

	return 0;
}
