# Using µnit is very simple; just include the header and add the C
# file to your sources.  That said, here is a simple Makefile to build
# the example.

CFLAGS:=-std=c99 -g
EXTENSION:=
TEST_ENV:=

example$(EXTENSION): munit.h munit.c example.c
	$(CC) $(CFLAGS) -o $@ munit.c example.c

test:
	$(TEST_ENV) ./example$(EXTENSION)

clean:
	rm -f example$(EXTENSION)

all: example$(EXTENSION)
