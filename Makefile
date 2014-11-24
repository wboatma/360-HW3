# Makefile for CpSc 360 Project 2

SRCS := $(wildcard *.cpp)

.cpp.o:
	g++ -g -c -Wall $<

all: simhttp

simhttp: http.o common.o
	g++ -g -o simhttp $^

clean:
	rm *.o
	rm simhttp

depend: $(SRCS)
	makedepend $(INCLUDES) $^

http.o: common.h http.h
