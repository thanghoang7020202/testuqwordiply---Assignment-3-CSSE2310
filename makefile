cc = gcc
cflags = -Wall -pedantic -std=gnu99 -g
cflags += -I/local/courses/csse2310/include
cflags += -L/local/courses/csse2310/lib -lcsse2310a3

all: testuqwordiply
testuqwordiply: testuqwordiply.c
	$(cc) $(cflags) -o $@ $<

clean:
	rm -f testuqwordiply
