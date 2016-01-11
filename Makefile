# Using µnit is very simple; just include the header and add the C
# file to your sources.  That said, here is a simple Makefile to build
# the example.

CFLAGS:=-std=c99 -g

example: munit.c example.c
	$(CC) $(CFLAGS) -o $@ munit.c example.c

test:
	./example

clean:
	rm example

all: example
