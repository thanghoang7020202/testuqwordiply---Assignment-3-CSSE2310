cc = gcc
cflags = -Wall -pedantic -std=gnu99 -g

all: testuqwordiply
testuqwordiply: testuqwordiply.c
	$(cc) $(cflags) -o $@ $<

clean:
	rm -f testuqwordiply
