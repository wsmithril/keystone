# makefile for keystone

CC = gcc
CFLAGS += -O2 -Wall -fPIC
LDFLAGS += -pthread

objects = log.o hash.o storage_mem.o keystone.o

.PHONY: all clean

all: libkeystone.a libkeystone.so

clean:
	rm -f $(objects) keystone
	rm -f libkeystone.a
	rm -f libkeystone.so.$(VERSION)

libkeystone.a: $(objects)
	$(AR) $(ARFLAGS) $@ $(objects)

libkeystone.so: $(objects)
	$(CC) -shared -Wl,-soname,$@ -o $@ $(objects)

log.c: log.h

storage_mem.c: log.h storage_mem.h hash.h

hash.c: hash.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

