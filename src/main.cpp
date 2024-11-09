#include <algorithm>
#include <curses.h>
#include <err.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <list>
#include <unistd.h>

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
			wclrtoeol(state.wcontent);
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

void edt_open(State &state, const char* filename)
{
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		err(EXIT_FAILURE, "stat");
	}

	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		fprintf(stderr, "edt: %s: Not a regular file\n", filename);
		exit(EXIT_FAILURE);
	}

	FILE* fp;

	if ((fp = fopen(filename, "r")) == NULL) {
		err(EXIT_FAILURE, "fopen");
	}

	if (sb.st_size == 0) {
		state.content.emplace_back("");
	} else {
		char * line = NULL;
		size_t len = 0;
		ssize_t nread;

		while ((nread = getline(&line, &len, fp)) != -1) {
			state.content.emplace_back(std::string(line));
		}

		free(line);
		fclose(fp);
	}

	state.filename = std::string(filename);

	state.contentIt = state.content.begin();
}

void edt_write(State &state)
{
	FILE* fp;

	if ((fp = fopen(state.filename.c_str(), "w")) == NULL) {
		wprintw(state.wmenu, "%s: fopen: %s", TARGET, strerror(errno));
		wrefresh(state.wmenu);
		return;
	}

	for(auto it = state.content.begin(); it != state.content.end(); it++) {
		fwrite(it->c_str(), sizeof(char), strlen(it->c_str()), fp);
	}

	fclose(fp);
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

void event_up(State &state)
{
	state.refreshDisplay = false;

	if(state.contentIt != state.content.begin())
	{
		scroll_up(state);

		state.contentIt = std::prev(state.contentIt);

		if(state.contentIt->length()-1 < state.savedIndex)
		{
			state.currentIndex = state.contentIt->length()-1;
		}
		else
		{
			state.currentIndex = state.savedIndex;
		}
	}
}

void event_down(State &state)
{
	state.refreshDisplay = false;

	if(state.contentIt == std::prev(state.content.end()))
	{
		return;
	}

	state.contentIt = std::next(state.contentIt);

	scroll_down(state);

	if(state.contentIt->length() <= state.savedIndex && state.contentIt == std::prev(state.content.end()))
	{
		state.currentIndex = state.contentIt->length();
	}
	else if(state.contentIt->length()-1 < state.savedIndex)
	{
		state.currentIndex = state.contentIt->length()-1;
	}
	else
	{
		state.currentIndex = state.savedIndex;
	}
}

void event_page_up(State &state)
{
	if((int)state.entryLine < getmaxy(state.wcontent))
	{
		state.entryLine = 0;
		state.contentIt = state.content.begin();
	}
	else
	{
		state.entryLine -= getmaxy(state.wcontent);
		state.contentIt = std::prev(state.contentIt, getmaxy(state.wcontent));
	}

	state.currentIndex = 0;
	state.savedIndex = 0;
}

void event_page_down(State &state)
{
	if(state.entryLine > state.content.size() - getmaxy(state.wcontent))
	{
		state.contentIt = std::prev(state.content.end());
	}
	else
	{
		state.entryLine += getmaxy(state.wcontent);

		if(std::distance(state.contentIt, state.content.end()) > getmaxy(state.wcontent))
		{
			state.contentIt = std::next(state.contentIt, getmaxy(state.wcontent));
		}
		else
		{
			state.contentIt = std::prev(state.content.end());
		}
	}

	state.currentIndex = 0;
	state.savedIndex = 0;
}

void event_backspace(State &state)
{
	// If at beginning of line and current line is not the first line
	if(state.currentIndex == 0 && state.contentIt != state.content.begin())
	{
		scroll_up(state);

		// When the line has no content with exception of "\n"
		if(std::prev(state.contentIt)->length() != 1)
		{
			state.contentIt = std::prev(state.contentIt);

			state.currentIndex = state.contentIt->length()-1;
			state.savedIndex = state.contentIt->length()-1;

			if(state.contentIt->length() > 0) {
				// Append the complete line to the previous line
				state.contentIt->append(std::next(state.contentIt)->substr(0));

				// Delete the first line break in the current line
				state.contentIt->erase(state.contentIt->find("\n"), 1);
			}

			// Delete the line from where the list was copied from
			state.content.erase(std::next(state.contentIt));
		}
		else if(std::prev(state.contentIt)->length() == 1)
		{
			// Delete the previous line with the "\n". The line will be poped one row up
			state.content.erase(std::prev(state.contentIt));

			state.currentIndex = 0;
			state.savedIndex = 0;
		}
	}
	else if(!(state.currentIndex == 0 && state.contentIt == state.content.begin()))
	{
		// Deletes the character that is in front of the cursor
		state.contentIt->erase(state.currentIndex-1, 1);

		state.currentIndex--;
		state.savedIndex = state.currentIndex;
		state.refreshDisplay = false;
	}
}

void event_delete(State &state)
{
	// Last character and last row
	if(state.currentIndex == state.contentIt->length() && state.content.end() == std::next(state.contentIt))
	{
		return;
	}
	// Line end: copy next line to this line, delete next line
	else if(state.currentIndex == state.contentIt->length()-1 && state.contentIt->back() == '\n')
	{
		// If the line has more content then "\n"
		if(std::next(state.contentIt)->length() != 1)
		{
			// Remove the paragraph
			state.contentIt->erase(state.currentIndex, 1);

			// Copy the entire next line to the current line
			state.contentIt->append(std::next(state.contentIt)->substr(0));

			// Delete the line from where the list was copied from
			state.content.erase(std::next(state.contentIt));
		}
		else if(std::next(state.contentIt)->length() == 1)
		{
			// Delete the next line
			state.content.erase(std::next(state.contentIt));
		}
	}
	else
	{
		// Delete character
		state.contentIt->erase(state.currentIndex, 1);
		state.refreshDisplay = false;
	}
}

void event_pos_begin(State &state)
{
	state.currentIndex = 0;
	state.savedIndex = 0;
	state.refreshDisplay = false;
}

void event_pos_end(State &state)
{
	if(state.contentIt != std::prev(state.content.end()))
	{
		state.currentIndex = state.contentIt->length()-1;
		state.savedIndex = state.contentIt->length()-1;
	}
	else
	{
		state.currentIndex = state.contentIt->length();
		state.savedIndex = state.contentIt->length();
	}

	state.refreshDisplay = false;
}

int main (int argc, char *argv[])
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s [FILE]\n", TARGET);
		exit(EXIT_FAILURE);
	}

	State state;
	state_reset(state);

	edt_open(state, argv[1]);

	ncurses_init();

	state_init_window(state);

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
			case KEY_UP:
				event_up(state);
			break;
			case KEY_DOWN:
				event_down(state);
			break;
			case KEY_PPAGE:
				event_page_up(state);
			break;
			case KEY_NPAGE:
				event_page_down(state);
			break;
			case KEY_BACKSPACE:
				event_backspace(state);
			break;
			case KEY_DC:
				event_delete(state);
			break;
			case KEY_HOME:
				event_pos_begin(state);
			break;
			case KEY_END:
				event_pos_end(state);
			break;
			case ctrl('s'):
				edt_write(state);
			break;
			default:
				event_write(state);
		}
	}

	endwin();

	return 0;
}
