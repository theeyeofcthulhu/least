CC = gcc
CCFLAGS = -g -Wall -Wextra -Wshadow

AS = nasm
ASFLAGS = -g -felf64

SRC = lcc.c stack.c error.c util.c lstring.c
INCLUDE = stack.h error.h util.h dictionary.h lstring.h
OBJ = $(SRC:.c=.o)
EXE = lcc
ASM_LIB = $(addprefix lib/, uprint.asm putchar.asm)
ASM_LIB_O = $(ASM_LIB:.asm=.o)

.PHONY: all
all: $(EXE) lib

.PHONY: clean
clean:
	rm *.o
	rm lcc

.PHONY: test
test:
	./tests.sh

.PHONY: lib
lib: $(ASM_LIB_O)

lib/%.o: lib/%.asm
	$(AS) $(ASFLAGS) -o $@ $<

lcc.o: stack.h error.h util.h dictionary.h lstring.h
stack.o: stack.h error.h
error.o: error.h
util.o: util.h
lstring.o: lstring.h error.h

$(EXE): $(OBJ) $(INCLUDE)
	$(CC) -o $@ $(OBJ) $(LIBS)

%.o: %.c
	$(CC) -c $(CCFLAGS) -o $@ $<
