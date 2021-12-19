CXX = g++
CCFLAGS = -g -Wall -Wextra -Wshadow -std=c++20

AS = nasm
ASFLAGS = -g -felf64

SRC = lcc.cpp error.cpp util.cpp lstring.cpp dictionary.cpp lexer.cpp ast.cpp x86_64.cpp
# INCLUDE = error.h util.h dictionary.h lstring.h lexer.h ast.h x86_64.h
OBJ = $(SRC:.cpp=.o)
EXE = lcc
ASM_LIB = $(addprefix lib/, uprint.asm putchar.asm)
ASM_LIB_O = $(ASM_LIB:.asm=.o)

.PHONY: all
all: $(EXE) lib

.PHONY: clean
clean:
	rm $(OBJ) $(EXE) $(ASM_LIB_O)

.PHONY: test
test:
	./tests.sh

.PHONY: lib
lib: $(ASM_LIB_O)

lib/%.o: lib/%.asm
	$(AS) $(ASFLAGS) -o $@ $<

lcc.o: util.h dictionary.h error.h lexer.h ast.h x86_64.h
error.o: error.h
util.o: util.h error.h lexer.h
lstring.o: lstring.h error.h
lexer.o: lexer.h dictionary.h lstring.h util.h error.h lstring.h
ast.o: util.h lexer.h error.h ast.h x86_64.h
x86_64.o: error.h x86_64.h ast.h util.h
dictionary.o: dictionary.h ast.h

$(EXE): $(OBJ)
	$(CXX) -o $@ $(OBJ) $(LIBS)

%.o: %.cpp
	$(CXX) -c $(CCFLAGS) -o $@ $<
