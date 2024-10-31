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

std::list<std::string>::iterator displayFirstIt(State &state)
{
	return std::next(state.content.begin(), state.entryLine);
}

std::list<std::string>::iterator displayLastIt(State &state)
{
	const auto itBegin = displayFirstIt(state);

	return state.entryLine + getmaxy(state.wcontent) >= state.content.size() ? state.content.end() : std::next(itBegin, getmaxy(state.wcontent));
}

std::string display_convert_line(std::string line)
{
	if (line.back() == '\n')
	{
		line.pop_back();
	}

	for (std::size_t pos = 0; pos < line.length(); pos++)
	{
		if (line.at(pos) != '\t')
		{
			continue;
		}

		line.erase(pos , 1);

		do
		{
			line.insert(pos, 1, ' ');
			pos++;
		} while (pos % TABSIZE);
	}

	return line;
}

void display_content(State &state)
{
	if(state.refreshDisplay == true)
	{
		wclear(state.wcontent);
	}

	const auto itBegin = state.refreshDisplay == true ? displayFirstIt(state) : state.contentIt;
	const auto itEnd = state.refreshDisplay == true ? displayLastIt(state) : std::next(state.contentIt, 1);

	for(auto it = itBegin; it != itEnd; it++)
	{
		int initalCursorY = getcury(state.wcontent);

		std::string itValue = display_convert_line(*it);

		bool nextLine = true;

		if(state.contentIt == it)
		{
			state.cursorY = distance(displayFirstIt(state), state.contentIt);
			wmove(state.wcontent, state.cursorY, 0);
			wprintw(state.wcontent, "%s", state.contentIt->substr(0, state.currentIndex).c_str());
			getyx(state.wcontent, state.cursorY, state.cursorX);
			wmove(state.wcontent, state.cursorY, 0);
		}

		// Last line to print to screen
		if(it != std::prev(state.content.end()) && it == std::prev(itEnd) && it->at(it->length() - 1) == '\n')
		{
			nextLine = false;
		}

		wprintw(state.wcontent, "%s", itValue.substr(0, getmaxx(state.wcontent) - 1).c_str());

		if(itValue.length() > (std::size_t)getmaxx(state.wcontent))
		{
			wattron(state.wcontent, A_REVERSE);
			wprintw(state.wcontent, "%s", ">");
			wattroff(state.wcontent, A_REVERSE);
		}

		if(nextLine == true)
		{
			wmove(state.wcontent, initalCursorY + 1, 0);
		}
	}

	wmove(state.wcontent, state.cursorY, state.cursorX);
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
		display_content(state);

		state.refreshDisplay = true;

		state.key = wgetch(state.wcontent);
	}

	endwin();

	return 0;
}
