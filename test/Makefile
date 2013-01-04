# makefile for keystone

CC = gcc
CFLAGS += -g -Wall -I../src
LDFLAGS += -pthread -L../src -lkeystone

.PHONY: all clean

all: test-memdb test-keystone

clean:
	rm -f test-*

test: all
	LD_LIBRARY_PATH=../src ./test-memdb
	LD_LIBRARY_PATH=../src ./test-keystone

test-memdb: test-memdb.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

test-keystone: test-keystone.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@


