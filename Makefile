VERSION = 0.0.1

.PHONY: all clean test

all:
	cd src && make all

clean:
	cd src  && make clean
	cd test && make clean

test:
	cd test && make all && make test
