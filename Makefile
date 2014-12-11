
CC=gcc
CFLAGS=-Wall -g

BINS=simhttp

all: $(BINS)

simhttp: simhttp.c
	$(CC) $(CFLAGS) -o simhttp simhttp.c

clean:
	rm $(BINS)
