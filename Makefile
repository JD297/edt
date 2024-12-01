PREFIX		= /usr/local
BINDIR		= $(PREFIX)/bin
MANDIR		= $(PREFIX)/share/man

TARGET		= edt
TARGETDIR	= bin
BUILDDIR	= build
SRCDIR		= src
SRC_FILES	= $(wildcard $(SRCDIR)/*.c)
OBJ_FILES	= $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC_FILES))

CC		= gcc
CCLIBS		= -lncurses
CCFLAGS		= -Wall -Wextra -Wpedantic
CCFLAGSPROG	= -DTARGET=\"$(TARGET)\"
CCFLAGSDEBUG	= -g
#CCLIBSSTATIC	= -static

$(TARGET): $(OBJ_FILES)
	$(CC) $(CCFLAGS) $(CCFLAGSDEBUG) $(OBJ_FILES) $(CCLIBSSTATIC) -o $(TARGETDIR)/$(TARGET) $(CCLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CCFLAGS) $(CCFLAGSPROG) -c -o $@ $<

clean:
	rm -f $(BUILDDIR)/*.o $(TARGETDIR)/$(TARGET)

install: $(TARGET)
	cp $(TARGETDIR)/$(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
