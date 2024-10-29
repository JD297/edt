PREFIX		= /usr/local
BINDIR		= $(PREFIX)/bin
MANDIR		= $(PREFIX)/share/man

TARGET		= edt
SRC		= src
SRC_FILES	= $(wildcard $(SRC)/*.cpp)
OBJ_FILES	= $(patsubst $(SRC)/%.cpp,$(SRC)/%.o,$(SRC_FILES))

CXX		= g++
CXXLIBS		= -lncursesw
CXXFLAGS	= -Wall -Wextra -Wpedantic
CXXFLAGSPROG    = -DTARGET=\"$(TARGET)\"
#CXXFLAGSDEBUG	= -g
#CXXLIBSSTATIC	= -static

#FEATURES	=

$(TARGET): $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEBUG) $(OBJ_FILES) $(CXXLIBSSTATIC) $(CXXLIBS) -o $(TARGET)

$(SRC)/%.o: $(SRC)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGSPROG) $(WARN) -c -o $@ $<

clean:
	rm -f $(SRC)/*.o $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
