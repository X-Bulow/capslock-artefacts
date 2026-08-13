CC ?= cc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra -Werror -pedantic

.PHONY: all test clean

all: test_runtime oracle_programs

test_runtime: libcapslock.c libcapslock.h test_runtime.c
	$(CC) $(CFLAGS) libcapslock.c test_runtime.c -o $@

oracle_programs: libcapslock.c libcapslock.h oracle_programs.c
	$(CC) $(CFLAGS) libcapslock.c oracle_programs.c -o $@

test: all
	./test_runtime
	./oracle_programs

clean:
	$(RM) test_runtime oracle_programs
