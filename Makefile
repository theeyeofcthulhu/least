CXX = g++
CCFLAGS = -g -Wall -Wextra -Wshadow -std=c++20 -pedantic

AS = nasm
ASFLAGS = -g -felf64

SRC = $(wildcard *.cpp)
INCLUDE = $(wildcard *.hpp)
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

.PHONY: format
format: $(SRC) $(INCLUDE)
	clang-format -i $^

.PHONY: lib
lib: $(ASM_LIB_O)

lib/%.o: lib/%.asm
	$(AS) $(ASFLAGS) -o $@ $<

.PHONY: depend
depend: .depend 

.depend: $(SRC)
	$(CXX) $(CCFLAGS) -MM $^ > "$@"

include .depend

$(EXE): $(OBJ)
	$(CXX) -o $@ $(OBJ) $(LIBS)

%.o: %.cpp
	$(CXX) -c $(CCFLAGS) -o $@ $<
