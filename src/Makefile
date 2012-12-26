# makefile foe keystone

CC = gcc
CFLAGS += -O2 -Wall
LDFLAGS += -pthread

.PHONY: all clean

clean:
	rm -f *.o keystone

all: keystone

keystone: log.o hash.o storage_mem.o

log.c: log.h

storage_mem.c: log.h storage_mem.h hash.h

hash.c: hash.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

