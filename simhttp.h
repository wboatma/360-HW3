/* 
 * William Beasley - C88846536
 * William Boatman - C16779898
 * CPSC 3600 HW3
 * Dr. Sekou Remy
 * TCP HTTP Server
 * Parses HTTP Requests such as GET
 * Displays the pages and images on websites if accessed
 */
 
#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h> 
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

//Supported types to support for our webserver, per project requirements
enum REQUESTED_HTTP_METHOD {
	GET, HEAD, UNSUPPORTED
};

enum REQUESTED_HTTP_TYPE {
	SIMPLE, FULL
};

struct request_info {
	char *referer, *current_user, *current_user_resource;
	int http_request_status;
	enum REQUESTED_HTTP_METHOD http_method;
	enum REQUESTED_HTTP_TYPE type;
};

int return_resources(int connecection, int current_user_resource, 
							                  struct request_info *requested_info);
							
int check_resource(struct request_info *requested_info);

void die_with_error(char const *message);

int trim_string_buffer(char *buffer);

int string_to_upper(char *buffer);

void cleanURL(char *buffer);

ssize_t read_requested_line(int sockd, void *voidptr, size_t maxlen);

ssize_t write_http_line(int sockd, const void *voidptr, size_t n);

void print_results();

int out_http_header(int connecection, struct request_info *requested_info);

int return_error_message(int connecection, struct request_info *requested_info);

int request_service(int connecection);

int parse_http_request(char *buffer, struct request_info *requested_info);

int get_request(int connecection, struct request_info *requested_info);

void request_initial_info(struct request_info *requested_info);

void free_requested_information(struct request_info *requested_info);