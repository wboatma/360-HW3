#ifndef _COMMON_H
#define _COMMON_H

#include <iostream>
#include <cstdlib>
#include <assert.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>

#define MAXLINE 4096

//err_n_die() arguments
#define BADARGS -1
#define SOCKERR -2
#define BINDERR -3
#define LISTERR -4
#define RECVERR -5
#define READERR -6

using namespace std;

void err_n_die(int error);

#endif //_COMMON_H
