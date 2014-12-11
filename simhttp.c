/* 
 * William Beasley
 * William Boatman
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

#define DEFINED_LISTEN_VAL      	1024
#define DEFAULT_SERVER_PORT 		8080
#define MAX_REQUESTED_LINE_SIZE     1024

enum REQUESTED_HTTP_METHOD {
	GET, HEAD, UNSUPPORTED
};
enum REQUESTED_HTTP_TYPE {
	SIMPLE, FULL
};

struct Request_Info {
	char *Referer, *Current_User, *Current_User_Resource;
	int Http_Request_Status;
	enum REQUESTED_HTTP_METHOD Http_Method;
	enum REQUESTED_HTTP_TYPE type;
};

int Return_Resources(int connecection, int Current_User_Resource, struct Request_Info * Requested_Info);
int Check_Resource(struct Request_Info * Requested_Info);
void Die_With_Error(char const * msg);
int Trim_String_Buffer(char * buffer);
int String_to_Upper(char * buffer);
void CleanURL(char * buffer);
ssize_t Read_Requested_Line(int sockd, void *voidptr, size_t maxlen);
ssize_t Write_Http_Line(int sockd, const void *voidptr, size_t n);
void printTime();
int OUT_HTTP_HEADER(int connecection, struct Request_Info * Requested_Info);
int Return_Error_Msgs(int connecection, struct Request_Info * Requested_Info);
int Request_Service(int connecection);
int PARSE_HTTP_REQUEST(char * buffer, struct Request_Info * Requested_Info);
int GET_REQUEST(int connecection, struct Request_Info * Requested_Info);
void Request_Initial_Info(struct Request_Info * Requested_Info);
void Free_Requested_Information(struct Request_Info * Requested_Info);

static char server_root[1000] = "./";
char* Months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
char* Days[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
char* Http_Method[] = {"GET", "HEAD", "UNSUPPORTED"};
struct Request_Info Requested_Info;

int main(int argc, char *argv[]) {
	int listener, connecection;
	struct sockaddr_in servaddr;
	unsigned short servPort;

	if (argv[2] != NULL) {
		servPort = atoi(argv[2]);
	} else {
		servPort = DEFAULT_SERVER_PORT ;
	}

	if (argv[3] != NULL) {
		strcpy(server_root, argv[3]);
	}

	if ((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		Die_With_Error("Error: Server couldn't create listening socket.");

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servPort);

	if (bind(listener, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
		Die_With_Error("Error: Server couldn't bind listening socket.");

	if (listen(listener, DEFINED_LISTEN_VAL) < 0)
		Die_With_Error("Error: call to listen failed.");
	
	while (1) {

		if ((connecection = accept(listener, NULL, NULL)) < 0)
			Die_With_Error("Error: failed on calling accept()");

		if (close(listener) < 0)
			Die_With_Error("Error: failed when closing listening socket.");

		Request_Service(connecection);

		printf("%s\t", Http_Method[Requested_Info.Http_Method]);
		printf("%s\t", Requested_Info.Current_User_Resource);
		printTime();
		printf("\t%d\n", Requested_Info.Http_Request_Status);

		if (close(connecection) < 0)
			Die_With_Error("Error: Failed when closing connection socket.");
		exit(EXIT_SUCCESS);

		if (close(connecection) < 0)
			Die_With_Error("Error: Failed when closing connection socket.");
		
	}

	return EXIT_FAILURE; 
}

void printTime() {

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	printf("%d %s %d %d:%02d ", 
		tm.tm_mday, Months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
}

int Request_Service(int connecection) {

	int Current_User_Resource = 0;

	Request_Initial_Info(&Requested_Info);
	
	if (GET_REQUEST(connecection, &Requested_Info) < 0) {
		Requested_Info.Http_Request_Status = 400;
		return -1;
	}

	if (Requested_Info.Http_Request_Status == 200)
		if ((Current_User_Resource = Check_Resource(&Requested_Info)) < 0) {
			if (errno == EACCES)
				Requested_Info.Http_Request_Status = 403;
			else
				Requested_Info.Http_Request_Status = 404;
		}

	if (Requested_Info.type == FULL)
		OUT_HTTP_HEADER(connecection, &Requested_Info);

	if (Requested_Info.Http_Request_Status == 200) {
		if (Return_Resources(connecection, Current_User_Resource, &Requested_Info))
			Die_With_Error("Error: Something went wrong returning Current_User_Resource.");
	} else
		Return_Error_Msgs(connecection, &Requested_Info);

	if (Current_User_Resource > 0)
		if (close(Current_User_Resource) < 0)
			Die_With_Error("Error: Failed during closing of Current_User_Resource.");

	return 0;
}

int PARSE_HTTP_REQUEST(char * buffer, struct Request_Info * Requested_Info) {

	static int Header = 1;
	char *temp;
	char *endptr;
	int len = 0;

	if (Header == 1) {
		if (!strncmp(buffer, "GET /../", 8)) {
			Requested_Info->Http_Method = GET;
			Requested_Info->Http_Request_Status = 403;
			buffer +=4;
		} else if (!strncmp(buffer, "GET /", 5)) {
			Requested_Info->Http_Method = GET;
			buffer += 4;
		} else if (!strncmp(buffer, "HEAD /", 6)) {
			Requested_Info->Http_Method = HEAD;
			buffer += 5;
		} else if (strncmp(buffer, "GET ", 4) == 0 && strncmp(buffer, "GET /", 5) != 0) {
			Requested_Info->Http_Method = GET;
			Requested_Info->Http_Request_Status = 400;
			buffer += 5;
		}
		else {
			Requested_Info->Http_Method = UNSUPPORTED;
			Requested_Info->Http_Request_Status = 405;
			return -1;
		}

		while (*buffer && isspace(*buffer))
			buffer++;

		endptr = strchr(buffer, ' ');
		if (endptr == NULL)
			len = strlen(buffer);
		else
			len = endptr - buffer;
		if (len == 0) {
			Requested_Info->Http_Request_Status = 400;
			return -1;
		}

		Requested_Info->Current_User_Resource = calloc(len + 1, sizeof(char));
		strncpy(Requested_Info->Current_User_Resource, buffer, len);

		if (strstr(buffer, "HTTP/"))
			Requested_Info->type = FULL;
		else
			Requested_Info->type = SIMPLE;

		Header = 0;
		return 0;
	}

	endptr = strchr(buffer, ':');
	if (endptr == NULL) {
		Requested_Info->Http_Request_Status = 400;
		return -1;
	}

	temp = calloc((endptr - buffer) + 1, sizeof(char));
	strncpy(temp, buffer, (endptr - buffer));
	String_to_Upper(temp);

	buffer = endptr + 1;
	while (*buffer && isspace(*buffer))
		++buffer;
	if (*buffer == '\0')
		return 0;

	if (!strcmp(temp, "USER-AGENT")) {
		Requested_Info->Current_User = malloc(strlen(buffer) + 1);
		strcpy(Requested_Info->Current_User, buffer);
	} else if (!strcmp(temp, "REFERER")) {
		Requested_Info->Referer = malloc(strlen(buffer) + 1);
		strcpy(Requested_Info->Referer, buffer);
	}

	free(temp);
	return 0;
}

int GET_REQUEST(int connecection, struct Request_Info * Requested_Info) {

	char buffer[MAX_REQUESTED_LINE_SIZE ] = { 0 };
	int return_val;
	fd_set fds;
	struct timeval tv;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	do {
		FD_ZERO(&fds);
		FD_SET(connecection, &fds);

		return_val = select(connecection + 1, &fds, NULL, NULL, &tv);
		if (return_val < 0) {
			Die_With_Error("Error: Failed when calling select() in GET_REQUEST() Http_Method.");
		} else if (return_val == 0) {

			return -1;

		} else {
			Read_Requested_Line(connecection, buffer, MAX_REQUESTED_LINE_SIZE  - 1);
			Trim_String_Buffer(buffer);

			if (buffer[0] == '\0')
				break;

			if (PARSE_HTTP_REQUEST(buffer, Requested_Info))
				break;
		}
	} while (Requested_Info->type != SIMPLE);

	return 0;
}
void Request_Initial_Info(struct Request_Info * Requested_Info) {
	Requested_Info->Current_User = NULL;
	Requested_Info->Referer = NULL;
	Requested_Info->Current_User_Resource = NULL;
	Requested_Info->Http_Method = UNSUPPORTED;
	Requested_Info->Http_Request_Status = 200;
}
void Free_Requested_Information(struct Request_Info * Requested_Info) {
	if (Requested_Info->Current_User)
		free(Requested_Info->Current_User);
	if (Requested_Info->Referer)
		free(Requested_Info->Referer);
	if (Requested_Info->Current_User_Resource)
		free(Requested_Info->Current_User_Resource);
}
int OUT_HTTP_HEADER(int connecection, struct Request_Info * Requested_Info) {

	char buffer[100];
	struct stat fileStat;
    stat(server_root,&fileStat);
	time_t curr;
	struct tm * date;
	time(&curr);
	date = localtime(&curr);

	struct tm * la;
	la = localtime(&(fileStat.st_atime));
	
	sprintf(buffer, "HTTP/1.1 %d OK\r\n", Requested_Info->Http_Request_Status);
	Write_Http_Line(connecection, buffer, strlen(buffer));

	memset(buffer, 0, 100);
	sprintf(buffer, "Date: %s, %d %s %d %d:%d:%d\r\n", 
		Days[date->tm_wday], date->tm_mday, Months[date->tm_mon], date->tm_year + 1900,
		date->tm_hour, date->tm_min, date->tm_sec);
	Write_Http_Line(connecection, buffer, strlen(buffer));

	memset(buffer, 0, 100);
	sprintf(buffer, "Last-Modified: %s, %d %s %d %d:%d:%d\r\n", 
		Days[la->tm_wday], la->tm_mday, Months[la->tm_mon], la->tm_year + 1900,
		la->tm_hour, la->tm_min, la->tm_sec);
	Write_Http_Line(connecection, buffer, strlen(buffer));
	Write_Http_Line(connecection, "Content -Type: text/html\r\n", 26);

	memset(buffer, 0, 100);
	sprintf(buffer, "Content -Length: %ld\r\n", fileStat.st_size);
	Write_Http_Line(connecection, buffer, strlen(buffer));
 
	Write_Http_Line(connecection, "Server: simhttp\r\n", 17);
	Write_Http_Line(connecection, "\r\n", 2);

	return 0;
}

int Return_Resources(int connecection, int Current_User_Resource, struct Request_Info * Requested_Info) {

	char c;
	int i;

	while ((i = read(Current_User_Resource, &c, 1))) {
		if (i < 0)
			Die_With_Error("Error: Failed when reading from file.");
		if (write(connecection, &c, 1) < 1)
			Die_With_Error("Error: Failed when sending file.");
	}

	return 0;
}

int Check_Resource(struct Request_Info * Requested_Info) {
	CleanURL(Requested_Info->Current_User_Resource);
	strcat(server_root, Requested_Info->Current_User_Resource);
	return open(server_root, O_RDONLY);
}
int Return_Error_Msgs(int connecection, struct Request_Info * Requested_Info) {

	char buffer[100];

	if (Requested_Info->Http_Request_Status == 400) {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Bad Request</TITLE>\n</HEAD>\n\n", Requested_Info->Http_Request_Status);
		Write_Http_Line(connecection, buffer, strlen(buffer));
	}
	else if (Requested_Info->Http_Request_Status == 403) {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Forbidden</TITLE>\n</HEAD>\n\n", Requested_Info->Http_Request_Status);
		Write_Http_Line(connecection, buffer, strlen(buffer));
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Bad</TITLE>\n</HEAD>\n\n", Requested_Info->Http_Request_Status);
		Write_Http_Line(connecection, buffer, strlen(buffer));
	}
	else {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>Server Error%d</TITLE>\n</HEAD>\n\n", Requested_Info->Http_Request_Status);
		Write_Http_Line(connecection, buffer, strlen(buffer));
	}
	sprintf(buffer, "<BODY>\n<H1>Server Error %d</H1>\n", Requested_Info->Http_Request_Status);
	Write_Http_Line(connecection, buffer, strlen(buffer));

	sprintf(buffer, "<P>The request could not be completed.</P>\n"
			"</BODY>\n</HTML>\n");
	Write_Http_Line(connecection, buffer, strlen(buffer));

	return 0;

}
// Twist on Die with Error from previous homeworks
void Die_With_Error(char const * msg) {
	fprintf(stderr,"WEBSERV: %s\n", msg);
	exit(EXIT_FAILURE);
}

ssize_t Read_Requested_Line(int sockd, void *void_pointer, size_t maxlen) {
	ssize_t n, rc;
	char c, *buffer;

	buffer = void_pointer;

	for (n = 1; n < maxlen; n++) {

		if ((rc = read(sockd, &c, 1)) == 1) {
			*buffer++ = c;
			if (c == '\n')
				break;
		} else if (rc == 0) {
			if (n == 1)
				return 0;
			else
				break;
		} else {
			if (errno == EINTR)
				continue;
			Die_With_Error("Error in Read_Requested_Line()");
		}
	}
	//printf("%s\n", buffer);
	*buffer = 0;
	return n;
}

ssize_t Write_Http_Line(int sockd, const void *void_pointer, size_t n) {
	size_t nleft;
	ssize_t nwritten;
	const char *buffer;

	buffer = void_pointer;
	nleft = n;

	while (nleft > 0) {
		if ((nwritten = write(sockd, buffer, nleft)) <= 0) {
			if (errno == EINTR)
				nwritten = 0;
			else
				Die_With_Error("Error in Write_Http_Line()");
		}
		nleft -= nwritten;
		buffer += nwritten;
	}

	return n;
}

int Trim_String_Buffer(char * buffer) {
	int n = strlen(buffer) - 1;

	while (!isalnum(buffer[n]) && n >= 0)
		buffer[n--] = '\0';

	return 0;
}

int String_to_Upper(char * buffer) {
	while (*buffer) {
		*buffer = toupper(*buffer);
		++buffer;
	}
	return 0;
}

void CleanURL(char * buffer) {
	char asciinum[3] = { 0 };
	int i = 0, c;

	while (buffer[i]) {
		if (buffer[i] == '+')
			buffer[i] = ' ';
		else if (buffer[i] == '%') {
			asciinum[0] = buffer[i + 1];
			asciinum[1] = buffer[i + 2];
			buffer[i] = strtol(asciinum, NULL, 16);
			c = i + 1;
			do {
				buffer[c] = buffer[c + 2];
			} while (buffer[2 + (c++)]);
		}
		++i;
	}
}

