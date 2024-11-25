PREFIX		= /usr/local
BINDIR		= $(PREFIX)/bin
MANDIR		= $(PREFIX)/share/man

TARGET		= edt
TARGETDIR	= bin
BUILDDIR	= build
SRCDIR		= src
SRC_FILES	= $(wildcard $(SRCDIR)/*.cpp)
OBJ_FILES	= $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRC_FILES))

CXX		= g++
CXXLIBS		= -lncurses
CXXFLAGS	= -Wall -Wextra -Wpedantic
CXXFLAGSPROG	= -DTARGET=\"$(TARGET)\"
CXXFLAGSDEBUG	= -g
#CXXLIBSSTATIC	= -static

$(TARGET): $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEBUG) $(OBJ_FILES) $(CXXLIBSSTATIC) -o $(TARGETDIR)/$(TARGET) $(CXXLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGSPROG) -c -o $@ $<

clean:
	rm -f $(BUILDDIR)/*.o $(TARGETDIR)/$(TARGET)

install: $(TARGET)
	cp $(TARGETDIR)/$(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
