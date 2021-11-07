CC = gcc
CCFLAGS = -g -Wall

SRC = lcc.c stack.c
INCLUDE = stack.h
OBJ = $(SRC:.c=.o)
EXE = lcc

.PHONY: all
all: $(EXE)

.PHONY: clean
clean:
	rm *.o
	rm lcc

.PHONY: test
test:
	./tests.sh

lcc.o: stack.h
stack.o: stack.h

$(EXE): $(OBJ) $(INCLUDE)
	$(CC) $(CCFLAGS) -o $@ $(OBJ) $(LIBS)

%.o: %.c
	$(CC) -c $(CCFLAGS) -o $@ $<
