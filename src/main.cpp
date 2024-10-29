#include <algorithm>
#include <curses.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <list>

#define ctrl(x) ((x) & 0x1f)

struct State
{
	WINDOW* wcontent;
	WINDOW* wmenu;

	std::list<std::string> content;
	std::list<std::string>::iterator contentIt;

	std::size_t currentIndex;
	std::size_t savedIndex;
	std::size_t entryLine;

	std::size_t width;
	std::size_t height;

	int key;

	bool refreshDisplay;
	int cursorX, cursorY, cursorXReset;

	std::string filename;
};

void state_reset(State &state)
{
	state.key = 0;
	state.content.clear();
	state.content.emplace_back("");
	state.contentIt = state.content.begin();
	state.currentIndex = 0;
	state.savedIndex = 0;
	state.entryLine = 0;
	state.filename = "";
	state.cursorX = 0;
	state.cursorY = 0;
	state.cursorXReset = 0;
	state.refreshDisplay = true;
}

void state_init_window(State &state)
{
	state.width = getmaxx(stdscr);
	state.height = getmaxy(stdscr);

	state.wcontent = newwin(state.height - 1, state.width, 0, 0);
	state.wmenu = newwin(1, state.width, state.height - 1, 0);

	refresh();

	keypad(state.wcontent, true);
	scrollok(state.wcontent, true);

	wbkgd(state.wmenu, A_REVERSE);
	wrefresh(state.wmenu);
}

void ncurses_init()
{
	initscr();
	noecho();
	raw();
}

int main (int argc, char *argv[])
{
	State state;

	state_reset(state);

	if(argc >= 3)
	{
		fprintf(stderr, "%s: invalid argument -- \'%s\'\n", TARGET, argv[2]);
		fprintf(stderr, "usage: %s [path/to/file]\n", TARGET);

		exit(1);
	}
	else if(argc == 2) {
		// TODO load_file
	}

	ncurses_init();

	state_init_window(state);

	while (state.key != ctrl('x')) {
		state.key = wgetch(state.wcontent);
	}

	endwin();

	return 0;
}
