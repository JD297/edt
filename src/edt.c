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

	int entryline;
} edt_state;

void edt_state_init(edt_state *edt)
{
	edt->entryline = 0;
}

#define EDT_WSTATUS_HEIGHT 1

void edt_state_init_windows(edt_state *edt)
{
	int maxx = getmaxx(stdscr);
	int maxy = getmaxy(stdscr);

	edt->weditor = newpad(3000, 3000); // TODO resize if needed
	edt->wstatus = newwin(EDT_WSTATUS_HEIGHT, maxx, maxy - EDT_WSTATUS_HEIGHT, 0);

	refresh();

	keypad(stdscr, true);
}

void ncurses_init()
{
	initscr();
	noecho();
	raw();
}

char *edt_pbuf_entryline(edt_state *edt)
{
	char *pbuf_entryline = edt->buf;

	for (int i = 0; i < edt->entryline; i++) {
		char *line = strstr(pbuf_entryline, "\n");

		if (line == NULL) {
			break;
		}

		pbuf_entryline = line + 1;
	}

	return pbuf_entryline;
}

char *edt_pbuf_screen_lastline(edt_state *edt)
{
	char *pbuf_screen_lastline = edt->buf;

	for (int i = 0; i < edt->entryline + getmaxy(stdscr) - EDT_WSTATUS_HEIGHT; i++) {
		char *line = strstr(pbuf_screen_lastline, "\n");

		if (line == NULL) {
			break;
		}

		pbuf_screen_lastline = line + 1;
	}

	return pbuf_screen_lastline;
}

int edt_count_lines(edt_state *edt)
{
	char *line = edt->buf;

	for (int i = 0;  ; i++) {
		line = strstr(line, "\n");

		if (line == NULL) {
			return i;
		}

		line = line + 1;
	}
}

void edt_display(edt_state *edt)
{
	wmove(edt->weditor, 0, 0);

	int x, y, saved_position = 0;

	for (char *i = edt->buf; *i != '\0'; i++) {
		if (i == edt->p_buf) {
			getyx(edt->weditor, y, x);
			saved_position = 1;
		}
		wprintw(edt->weditor, "%c", *i);
	}

	if (saved_position == 1) {
		wmove(edt->weditor, y, x);
	}

	prefresh(edt->weditor, edt->entryline, 0, 0, 0, getmaxy(stdscr) - EDT_WSTATUS_HEIGHT - 1, getmaxx(stdscr) - 1);
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

void edt_write(edt_state *edt)
{
	int fd;

	if ((fd = open(edt->pathname, O_WRONLY)) == -1) {
		err(EXIT_FAILURE, "%s", edt->pathname); // TODO err handling
	}

	size_t c_buf = strlen(edt->buf);

	if (write(fd, edt->buf, c_buf) == -1) {
		err(EXIT_FAILURE, "%s", edt->pathname); // TODO err handling
	}

	if (close(fd) == -1) {
		err(EXIT_FAILURE, "%s", edt->pathname); // TODO err handling
	}

	wmove(edt->wstatus, 0, 0);
	wclrtoeol(edt->wstatus);
	wprintw(edt->wstatus, "\"%s\" %ldB (written) %ldR", edt->pathname, c_buf, edt->nbuf);
	wrefresh(edt->wstatus);
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
		edt->p_buf = edt->buf + strlen(edt->buf); // TODO USE THIS AS A MACRO

		return;
	}

	edt->p_buf = pos;
}

void edt_event_up(edt_state *edt)
{
	// TODO try to increment to current x pos
	edt_event_home(edt);
	edt_event_left(edt);
}

void edt_event_down(edt_state *edt)
{
	// TODO try to increment to current x pos
	edt_event_end(edt);
	edt_event_right(edt);
}

void edt_event_page_up(edt_state *edt)
{
	edt->entryline -= getmaxy(stdscr) - EDT_WSTATUS_HEIGHT;

	if (edt->entryline < 0) {
		edt->entryline = 0;
		edt->p_buf = edt->buf;

		return;
	}

	edt->p_buf = edt_pbuf_entryline(edt);
}

void edt_event_page_down(edt_state *edt)
{
	if (edt->entryline >= edt_count_lines(edt) - getmaxy(stdscr) - EDT_WSTATUS_HEIGHT) { // TODO MACRO
		edt->p_buf = edt_pbuf_screen_lastline(edt);

		return;
	}

	edt->entryline += getmaxy(stdscr) - EDT_WSTATUS_HEIGHT; // TODO add MACRO FOR THIS

	edt->p_buf = edt_pbuf_entryline(edt);
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

	edt_state_init(&edt);
	edt_state_init_windows(&edt);

	wprintw(edt.wstatus, "\"%s\" %dL, %ldB (%ldR)", edt.pathname, edt_count_lines(&edt), edt.sb.st_size, edt.nbuf);
	wrefresh(edt.wstatus);

	while (edt.key != ctrl('x')) {
		edt_display(&edt);

		edt.key = getch();

		switch(edt.key)
		{
			case KEY_LEFT:
				edt_event_left(&edt);
			break;
			case KEY_RIGHT:
				edt_event_right(&edt);
			break;
			case KEY_UP:
				edt_event_up(&edt);
			break;
			case KEY_DOWN:
				edt_event_down(&edt);
			break;
			case KEY_PPAGE:
				edt_event_page_up(&edt);
			break;
			case KEY_NPAGE:
				edt_event_page_down(&edt);
			break;
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
			case ctrl('s'):
				edt_write(&edt);
			break;
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
