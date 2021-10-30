##
# Least Programming language
#
# @file
# @version 0.1

CCFLAGS=-Wall -g

lcc: lcc.c
	gcc $(CCFLAGS) -o $@ $^

test: lcc
	./tests.sh

# end
