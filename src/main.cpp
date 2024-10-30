#include <algorithm>
#include <curses.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <list>
#include <unistd.h>

#define ctrl(x) ((x) & 0x1f)

#define NEW_FILE "NEW_FILE"
#define FAIL_READ "FAIL READ"

#define IS_DIRECTORY "IS DIRECTORY"
#define IS_BLOCK_FILE "IS BLOCK DEVICE"
#define IS_CHARACTER_FILE "IS CHARACTER DEVICE"
#define IS_FIFO "IS FIFO"
#define IS_SOCKET "IS SOCKET"
#define IS_SYMLINK "IS SYMLINK"
#define IS_NOT_REGULAR_FILE "IS NOT REGULAR FILE"

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

void event_write(State &state)
{
	if( (state.key >= 32 && state.key <= 126) || state.key == 10 || state.key == 9)
	{
		state.contentIt->insert(state.currentIndex, 1, (char)state.key);
	}

	// Return key
	if(state.key == 10)
	{
		if(state.currentIndex < state.contentIt->length()-1)
		{
			// Insert new line with content after cursor in new line
			state.content.insert(std::next(state.contentIt), state.contentIt->substr(state.currentIndex+1));

			// Shorten the current line until the cursor
			*state.contentIt = state.contentIt->substr(0, state.currentIndex+1);
		}
		else
		{
			// Insert new line empty after current line
			state.content.insert(std::next(state.contentIt), "");
		}

		state.contentIt = std::next(state.contentIt);

		state.currentIndex = 0;
		state.savedIndex = 0;
	}
	else if((state.key >= 32 && state.key <= 126) || state.key == 9)
	{
		state.currentIndex++;
		state.savedIndex = state.currentIndex;
		state.refreshDisplay = false;
	}
}

void load_file(State &state, const char* filename)
{
	state_reset(state); // TODO in this the correct position for this call??

	struct stat sb;

	if (stat(filename, &sb) == -1 && errno != ENOENT) {
		state.filename = std::string(filename);

		// when color support is enabled this should be: COLOR_RED
		wprintw(state.wmenu, " \"%s\" [%s]", filename, FAIL_READ);
		wrefresh(state.wmenu);

		return;
	}

	if (access(filename, F_OK) == -1)
	{
		state.filename = std::string(filename);

		wprintw(state.wmenu, " \"%s\" [%s]", filename, NEW_FILE);
		wrefresh(state.wmenu);

		return;
	}

	switch (sb.st_mode & S_IFMT) {
		case S_IFBLK: // when color support is enabled this should be: COLOR_YELLOW
			wprintw(state.wmenu, " \"%s\" [%s]", filename, IS_BLOCK_FILE);
			wrefresh(state.wmenu);
			return;
		case S_IFCHR: // when color support is enabled this should be: COLOR_YELLOW
			wprintw(state.wmenu, " \"%s\" [%s]", filename, IS_CHARACTER_FILE);
			wrefresh(state.wmenu);
			return;
		case S_IFDIR: // when color support is enabled this should be: COLOR_RED
			wprintw(state.wmenu, " \"%s\" [%s]", filename, IS_DIRECTORY);
			wrefresh(state.wmenu);
			return;
		case S_IFIFO: // when color support is enabled this should be: COLOR_YELLOW
			wprintw(state.wmenu, " \"%s\" [%s]", filename, IS_FIFO);
			wrefresh(state.wmenu);
			return;
		case S_IFLNK: // when color support is enabled this should be: COLOR_YELLOW
			wprintw(state.wmenu, " \"%s\" [%s]", filename, IS_SYMLINK);
			wrefresh(state.wmenu);
			return;
		case S_IFSOCK: // when color support is enabled this should be: COLOR_YELLOW
			wprintw(state.wmenu, " \"%s\" [%s]", filename, IS_SOCKET);
			wrefresh(state.wmenu);
			return;
		case !S_IFREG: // when color support is enabled this should be: COLOR_YELLOW
			wprintw(state.wmenu, " \"%s\" [%s]", filename, IS_NOT_REGULAR_FILE);
			wrefresh(state.wmenu);
			return;
    }

	FILE* fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	fp = fopen(filename, "r");

	if (fp == NULL)
	{
		// when color support is enabled this should be: COLOR_RED
		wprintw(state.wmenu, " \"%s\" [%s]", filename, FAIL_READ);
		wrefresh(state.wmenu);

		return;
	}

	state.content.clear();

	while ((read = getline(&line, &len, fp)) != -1)
	{
		std::string sline;
		sline = line;

		state.content.emplace_back(sline);
	}

	fclose(fp);

	if (line)
	{
		free(line);
	}

	state.filename = std::string(filename);

	state.contentIt = state.content.begin();

	wprintw(state.wmenu, " \"%s\"", filename);
	wrefresh(state.wmenu);
}

void scroll_up(State &state)
{
	if(state.contentIt == displayFirstIt(state))
	{
		state.entryLine--;
		wscrl(state.wcontent, -1);
	}
}

void scroll_down(State &state)
{
	if(state.contentIt == displayLastIt(state))
	{
		state.entryLine++;
		wscrl(state.wcontent, 1);
	}
}

void event_left(State &state)
{
	state.refreshDisplay = false;

	if(state.currentIndex > 0)
	{
		state.currentIndex--;
		state.savedIndex = state.currentIndex;
	}
	else if(state.currentIndex == 0 && state.contentIt != state.content.begin())
	{
		scroll_up(state);

		state.contentIt = std::prev(state.contentIt);

		state.currentIndex = state.contentIt->length()-1;
		state.savedIndex = state.currentIndex;
	}
}

void event_right(State &state)
{
	state.refreshDisplay = false;

	const auto itEnd = std::prev(state.content.end());

	if(state.currentIndex < state.contentIt->length()-1 ||
	  (state.currentIndex < state.contentIt->length() && state.contentIt == itEnd)
	)
	{
		if(state.currentIndex == state.contentIt->length() && state.contentIt->back() != '\n')
		{
			return;
		}

		state.refreshDisplay = false;
		state.currentIndex++;
		state.savedIndex = state.currentIndex;
	}
	else if(state.contentIt != itEnd)
	{
		state.contentIt = std::next(state.contentIt);

		scroll_down(state);

		state.currentIndex = 0;
		state.savedIndex = 0;
	}
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

	ncurses_init();

	state_init_window(state);

	if(argc == 2) {
		load_file(state, argv[1]);
	}

	while (state.key != ctrl('x')) {
		display_content(state);

		state.refreshDisplay = true;

		state.key = wgetch(state.wcontent);

		switch(state.key)
		{
			case KEY_LEFT:
				event_left(state);
			break;
			case KEY_RIGHT:
				event_right(state);
			break;
			default:
				event_write(state);
		}
	}

	endwin();

	return 0;
}
