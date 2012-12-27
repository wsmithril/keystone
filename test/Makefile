# makefile for keystone

CC = gcc
CFLAGS += -O2 -Wall -I../src
LDFLAGS += -pthread -L../src -lkeystone

.PHONY: all clean

all: test-memdb

clean:
	rm -f test-*

test: all
	LD_LIBRARY_PATH=../src ./test-memdb

test-memdb: test-memdb.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@


