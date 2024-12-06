#define _GNU_SOURCE

#include <curses.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
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

	int weditor_lines;
	int weditor_cols;
} edt_state;

void edt_state_init(edt_state *edt)
{
	edt->entryline = 0;
	edt->weditor_lines = 0;
	edt->weditor_cols = 0;
}

#define EDT_WSTATUS_HEIGHT 1
#define EDT_WEDITOR_HEIGHT getmaxy(stdscr) - EDT_WSTATUS_HEIGHT

void edt_state_init_windows(edt_state *edt)
{
	int maxx = getmaxx(stdscr);
	int maxy = getmaxy(stdscr);

	edt->weditor = newpad(edt->weditor_lines + EDT_WEDITOR_HEIGHT, edt->weditor_cols + maxx);
	edt->wstatus = newwin(EDT_WSTATUS_HEIGHT, maxx, maxy - EDT_WSTATUS_HEIGHT, 0);

	refresh();

	keypad(stdscr, true);
}

void edt_wstatus_print(edt_state *edt, const char *fmt, ...)
{
	wclear(edt->wstatus);

	va_list ap;

	va_start(ap, fmt);

	vw_printw(edt->wstatus, fmt, ap);

	wrefresh(edt->wstatus);
}

void ncurses_init()
{
	initscr();
	noecho();
	raw();
}

#define edt_pbuf_offset(EDT) edt->p_buf - edt->buf

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

	for (int i = 1; i < edt->entryline + EDT_WEDITOR_HEIGHT; i++) {
		char *line = strstr(pbuf_screen_lastline, "\n");

		if (line == NULL) {
			break;
		}

		pbuf_screen_lastline = line + 1;
	}

	return pbuf_screen_lastline;
}

#define edt_pbuf_last_entryline(EDT) EDT->weditor_lines - EDT_WEDITOR_HEIGHT

#define edt_pbuf_buf_firstline(EDT) EDT->buf

char *edt_pbuf_buf_lastline(edt_state *edt)
{
	char *pbuf_buf_lastline = edt->buf;

        while (1) {
                char *line = strstr(pbuf_buf_lastline, "\n");

                if (line == NULL) {
                        break;
                }

                pbuf_buf_lastline = line + 1;
        }

        return pbuf_buf_lastline;
}

char *edt_pbuf_currentline(edt_state *edt)
{
	char *pbuf_currentline = edt->buf;

	while (1) {
		char *line = strstr(pbuf_currentline, "\n");

		if (line == NULL || line >= edt->p_buf) {
			break;
		}

		pbuf_currentline = line +1;
	}

	return pbuf_currentline;
}

#define edt_pbuf_end(EDT) EDT->buf + strlen(EDT->buf)

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

void edt_weditor_lines_cols(edt_state *edt)
{
	char *previous = edt->buf;
	char *line;

	for (int i = 0;  ; i++) {
		line = strstr(previous, "\n");

		if (line == NULL) {
			edt->weditor_lines = i;

			break;
		}

		*line = '\0';

		size_t cols = strlen(previous);

		if ((int)cols > edt->weditor_cols) {
			edt->weditor_cols = (int)cols;
		}

		*line = '\n';

		previous = line + 1;
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

	wclrtobot(edt->weditor);

	if (saved_position == 1) {
		wmove(edt->weditor, y, x);
	}

	prefresh(edt->weditor, edt->entryline, 0, 0, 0, EDT_WEDITOR_HEIGHT - 1, getmaxx(stdscr) - 1);
}

#define EDT_RESERVED_PAGES_FACTOR 2

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

	edt->nbuf = ((edt->sb.st_size / sysconf(_SC_PAGESIZE)) + 1) * EDT_RESERVED_PAGES_FACTOR * sysconf(_SC_PAGESIZE);
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

	edt_weditor_lines_cols(edt);
}

void edt_write(edt_state *edt)
{
	FILE *fp;

	if (!(fp = fopen(edt->pathname, "w"))) {
		edt_wstatus_print(edt, "\"%s\": error: %s", edt->pathname, strerror(errno));
		return;
	}

	size_t c_buf = fwrite(edt->buf, sizeof(char), strlen(edt->buf), fp);

	if (fclose(fp) == EOF) {
		edt_wstatus_print(edt, "\"%s\": error: %s", edt->pathname, strerror(errno));
		return;
	}

	edt_wstatus_print(edt, "\"%s\" %dL, %ldB (written) %ldR", edt->pathname, edt->weditor_lines, c_buf, edt->nbuf);
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

	memset(edt->p_buf + strlen(edt->p_buf) - 1, 0, 1);

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
		edt->p_buf = edt_pbuf_end(edt);

		return;
	}

	edt->p_buf = pos;
}

void edt_event_up(edt_state *edt)
{
	if (edt_pbuf_entryline(edt) == edt_pbuf_currentline(edt) && edt_pbuf_buf_firstline(edt) != edt_pbuf_currentline(edt)) {
		edt->entryline--;
	}

	// TODO try to increment to current x pos
	edt_event_home(edt);
	edt_event_left(edt);
}

void edt_event_down(edt_state *edt)
{
	if (edt_pbuf_screen_lastline(edt) == edt_pbuf_currentline(edt) && edt_pbuf_buf_lastline(edt) != edt_pbuf_currentline(edt)) {
		edt->entryline++;
	}

	// TODO try to increment to current x pos
	edt_event_end(edt);
	edt_event_right(edt);
}

void edt_event_page_up(edt_state *edt)
{
	edt->entryline -= EDT_WEDITOR_HEIGHT;

	if (edt->entryline < 0) {
		edt->entryline = 0;
		edt->p_buf = edt->buf;

		return;
	}

	edt->p_buf = edt_pbuf_entryline(edt);
}

void edt_event_page_down(edt_state *edt)
{
	if (edt->entryline >= edt_pbuf_last_entryline(edt)) {
		edt->p_buf = edt_pbuf_screen_lastline(edt);

		return;
	}

	edt->entryline += EDT_WEDITOR_HEIGHT;

	edt->p_buf = edt_pbuf_entryline(edt);
}

void edt_event_write(edt_state *edt)
{
	if(!(edt->key == 10 || edt->key == 9 || (edt->key >= 32 && edt->key <= 126))) {
		return;
	}

	if (edt->nbuf <= strlen(edt->buf) + 1) {
		size_t pbuf_offset = edt_pbuf_offset(edt);
		size_t rbuf = edt->nbuf * 2;

		if ((edt->buf = mremap(edt->buf, edt->nbuf, rbuf, MREMAP_MAYMOVE)) == MAP_FAILED) {
			edt_wstatus_print(edt, "%s: error: %s", TARGET, strerror(errno));
			return;
		}

		edt_wstatus_print(edt, "\"%s\" %dL, %ldB (%ldR REMAPED)", edt->pathname, edt->weditor_lines, edt->sb.st_size, edt->nbuf);

		edt->nbuf = rbuf;
		edt->p_buf = edt->buf + pbuf_offset;
	}

	memmove(edt->p_buf + 1, edt->p_buf, strlen(edt->p_buf));

	*edt->p_buf = (char)edt->key;

	edt->p_buf++;

	if (edt->key == 10/* || edt_pbuf_cols(edt) >= edt->weditor_cols*/) { //TODO
		edt_weditor_lines_cols(edt);

		edt->weditor = newpad(edt->weditor_lines + EDT_WEDITOR_HEIGHT, edt->weditor_cols + getmaxx(stdscr));
	}
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
	edt_state_init(&edt);

	edt_open(&edt, argv[1]);

	ncurses_init();

	edt_state_init_windows(&edt);

	edt_wstatus_print(&edt, "\"%s\" %dL, %ldB (%ldR)", edt.pathname, edt.weditor_lines, edt.sb.st_size, edt.nbuf);

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

	endwin();

	if (munmap(edt.buf, edt.nbuf) == -1) {
		err(EXIT_FAILURE, "%s", edt.pathname);
	}

	return 0;
}
