/* 
 * William Beasley
 * William Boatman
 * CPSC 3600 HW3
 * Dr. Sekou Remy
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

#define LISTENQ         (1024)
#define SERVERPORT	(8080)
#define MAX_REQ_LINE    (1024)

enum REQUESTED_HTTP_METHOD {
	GET, HEAD, UNSUPPORTED
};
enum REQUESTED_HTTP_TYPE {
	SIMPLE, FULL
};

struct Request_Info {
	enum REQUESTED_HTTP_METHOD method;
	enum REQUESTED_HTTP_TYPE type;
	char *referer;
	char *useragent;
	char *resource;
	int status;
};

/* Function Prototypes */
int Request_Service(int conn);
int PARSE_HTTP(char * buffer, struct Request_Info * Requested_Info);
int GET_REQUEST(int conn, struct Request_Info * Requested_Info);
void Request_Initial_Info(struct Request_Info * Requested_Info);
void Free_Requested_Information(struct Request_Info * Requested_Info);
int OUT_HTTP_HEADER(int conn, struct Request_Info * Requested_Info);
int Return_Resources(int conn, int resource, struct Request_Info * Requested_Info);
int Check_Resource(struct Request_Info * Requested_Info);
int Return_Error_Msgs(int conn, struct Request_Info * Requested_Info);
void Error_Quit(char const * msg);
int Trim(char * buffer);
int StrUpper(char * buffer);
void CleanURL(char * buffer);
ssize_t Readline(int sockd, void *vptr, size_t maxlen);
ssize_t Writeline(int sockd, const void *vptr, size_t n);
void printTime();

static char server_root[1000] = "./";
char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
char* days[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
char* method[] = {"GET", "HEAD", "UNSUPPORTED"};
struct Request_Info Requested_Info;

int main(int argc, char *argv[]) {

	int listener, conn;
	struct sockaddr_in servaddr;
	unsigned short servPort;

	if (argv[2] != NULL) {
		servPort = atoi(argv[2]);
	} else {
		servPort = SERVERPORT;
	}

	if (argv[3] != NULL) {
		strcpy(server_root, argv[3]);
	}

	/*  Create socket  */

	if ((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		Error_Quit("Couldn't create listening socket.");

	/*  Populate socket address structure  */

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servPort);

	if (bind(listener, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
		Error_Quit("Couldn't bind listening socket.");

	if (listen(listener, LISTENQ) < 0)
		Error_Quit("Call to listen failed.");
	
	while (1) {

		if ((conn = accept(listener, NULL, NULL)) < 0)
			Error_Quit("Error calling accept()");

		if (close(listener) < 0)
			Error_Quit("Error closing listening socket.");

		
		Request_Service(conn);

		printf("%s\t", method[Requested_Info.method]);
		printf("%s\t", Requested_Info.resource);
		printTime();
		printf("\t%d\n", Requested_Info.status);

		if (close(conn) < 0)
			Error_Quit("Error closing connection socket.");
		exit(EXIT_SUCCESS);

		if (close(conn) < 0)
			Error_Quit("Error closing connection socket.");
		
	}

	return EXIT_FAILURE; 
}

void printTime() {

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	printf("%d %s %d %d:%02d ", 
		tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
}

int Request_Service(int conn) {

	int resource = 0;

	Request_Initial_Info(&Requested_Info);
	
	if (GET_REQUEST(conn, &Requested_Info) < 0) {
		Requested_Info.status = 400;
		return -1;
	}

	if (Requested_Info.status == 200)
		if ((resource = Check_Resource(&Requested_Info)) < 0) {
			if (errno == EACCES)
				Requested_Info.status = 403;
			else
				Requested_Info.status = 404;
		}

	if (Requested_Info.type == FULL)
		OUT_HTTP_HEADER(conn, &Requested_Info);

	if (Requested_Info.status == 200) {
		if (Return_Resources(conn, resource, &Requested_Info))
			Error_Quit("Something wrong returning resource.");
	} else
		Return_Error_Msgs(conn, &Requested_Info);

	if (resource > 0)
		if (close(resource) < 0)
			Error_Quit("Error closing resource.");

	return 0;
}

int PARSE_HTTP(char * buffer, struct Request_Info * Requested_Info) {

	static int first_header = 1;
	char *temp;
	char *endptr;
	int len;

	if (first_header == 1) {
		if (!strncmp(buffer, "GET /../", 8)) {
			Requested_Info->method = GET;
			Requested_Info->status = 403;
			buffer +=4;
		}else if (!strncmp(buffer, "GET /", 5)) {
			Requested_Info->method = GET;
			buffer += 4;
		} else if (!strncmp(buffer, "HEAD /", 6)) {
			Requested_Info->method = HEAD;
			buffer += 5;
		} else if (strncmp(buffer, "GET ", 4) == 0 && strncmp(buffer, "GET /", 5) != 0) {
			Requested_Info->method = GET;
			Requested_Info->status = 400;
			buffer += 5;
		}
		else {
			Requested_Info->method = UNSUPPORTED;
			Requested_Info->status = 405;
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
			Requested_Info->status = 400;
			return -1;
		}

		/*  ...and store it in the request information structure.  */

		Requested_Info->resource = calloc(len + 1, sizeof(char));
		strncpy(Requested_Info->resource, buffer, len);

		/*  Test to see if we have any HTTP version information.
		 If there isn't, this is a simple HTTP request, and we
		 should not try to read any more headers. For simplicity,
		 we don't bother checking the validity of the HTTP version
		 information supplied - we just assume that if it is
		 supplied, then it's a full request.                        */

		if (strstr(buffer, "HTTP/"))
			Requested_Info->type = FULL;
		else
			Requested_Info->type = SIMPLE;

		first_header = 0;
		return 0;
	}

	/*  If we get here, we have further headers aside from the
	 request line to parse, so this is a "full" HTTP request.  */

	/*  HTTP field names are case-insensitive, so make an
	 upper-case copy of the field name to aid comparison.
	 We need to make a copy of the header up until the colon.
	 If there is no colon, we return a status code of 400
	 (bad request) and terminate the connection. Note that
	 HTTP/1.0 allows (but discourages) headers to span multiple
	 lines if the following lines start with a space or a
	 tab. For simplicity, we do not allow this here.              */

	endptr = strchr(buffer, ':');
	if (endptr == NULL) {
		Requested_Info->status = 400;
		return -1;
	}

	temp = calloc((endptr - buffer) + 1, sizeof(char));
	strncpy(temp, buffer, (endptr - buffer));
	StrUpper(temp);

	/*  Increment buffer so that it now points to the value.
	 If there is no value, just return.                    */

	buffer = endptr + 1;
	while (*buffer && isspace(*buffer))
		++buffer;
	if (*buffer == '\0')
		return 0;

	/*  Now update the request information structure with the
	 appropriate field value. This version only supports the
	 "Referer:" and "User-Agent:" headers, ignoring all others.  */

	if (!strcmp(temp, "USER-AGENT")) {
		Requested_Info->useragent = malloc(strlen(buffer) + 1);
		strcpy(Requested_Info->useragent, buffer);
	} else if (!strcmp(temp, "REFERER")) {
		Requested_Info->referer = malloc(strlen(buffer) + 1);
		strcpy(Requested_Info->referer, buffer);
	}

	free(temp);
	return 0;
}

/*  Gets request headers. A CRLF terminates a HTTP header line,
 but if one is never sent we would wait forever. Therefore,
 we use select() to set a maximum length of time we will
 wait for the next complete header. If we timeout before
 this is received, we terminate the connection.               */

int GET_REQUEST(int conn, struct Request_Info * Requested_Info) {

	char buffer[MAX_REQ_LINE] = { 0 };
	int rval;
	fd_set fds;
	struct timeval tv;

	/*  Set timeout to 5 seconds  */

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	/*  Loop through request headers. If we have a simple request,
	 then we will loop only once. Otherwise, we will loop until
	 we receive a blank line which signifies the end of the headers,
	 or until select() times out, whichever is sooner.                */

	do {

		/*  Reset file descriptor set  */
		FD_ZERO(&fds);
		FD_SET(conn, &fds);

		/*  Wait until the timeout to see if input is ready  */

		rval = select(conn + 1, &fds, NULL, NULL, &tv);

		/*  Take appropriate action based on return from select()  */

		if (rval < 0) {
			Error_Quit("Error calling select() in GET_REQUEST()");
		} else if (rval == 0) {

			/*  input not ready after timeout  */

			return -1;

		} else {

			/*  We have an input line waiting, so retrieve it  */

			Readline(conn, buffer, MAX_REQ_LINE - 1);
			Trim(buffer);

			if (buffer[0] == '\0')
				break;

			if (PARSE_HTTP(buffer, Requested_Info))
				break;
		}
	} while (Requested_Info->type != SIMPLE);

	return 0;
}

/*  Initialises a request information structure  */

void Request_Initial_Info(struct Request_Info * Requested_Info) {
	Requested_Info->useragent = NULL;
	Requested_Info->referer = NULL;
	Requested_Info->resource = NULL;
	Requested_Info->method = UNSUPPORTED;
	Requested_Info->status = 200;
}

/*  Frees memory allocated for a request information structure  */

void Free_Requested_Information(struct Request_Info * Requested_Info) {
	if (Requested_Info->useragent)
		free(Requested_Info->useragent);
	if (Requested_Info->referer)
		free(Requested_Info->referer);
	if (Requested_Info->resource)
		free(Requested_Info->resource);
}

int OUT_HTTP_HEADER(int conn, struct Request_Info * Requested_Info) {

	char buffer[100];

	struct stat fileStat;
    	stat(server_root,&fileStat);
	
	time_t curr;
	struct tm * date;
	time(&curr);
	date = localtime(&curr);

	struct tm * la;
	la = localtime(&(fileStat.st_atime));
	
	sprintf(buffer, "HTTP/1.1 %d OK\r\n", Requested_Info->status);
	Writeline(conn, buffer, strlen(buffer));

	memset(buffer, 0, 100);
	sprintf(buffer, "Date: %s, %d %s %d %d:%d:%d\r\n", 
		days[date->tm_wday], date->tm_mday, months[date->tm_mon], date->tm_year + 1900,
		date->tm_hour, date->tm_min, date->tm_sec);
	Writeline(conn, buffer, strlen(buffer));

	memset(buffer, 0, 100);
	sprintf(buffer, "Last-Modified: %s, %d %s %d %d:%d:%d\r\n", 
		days[la->tm_wday], la->tm_mday, months[la->tm_mon], la->tm_year + 1900,
		la->tm_hour, la->tm_min, la->tm_sec);
	Writeline(conn, buffer, strlen(buffer));

	Writeline(conn, "Content -Type: text/html\r\n", 26);

	memset(buffer, 0, 100);
	sprintf(buffer, "Content -Length: %ld\r\n", fileStat.st_size);
	Writeline(conn, buffer, strlen(buffer));
 
	Writeline(conn, "Server: simhttp\r\n", 17);
	Writeline(conn, "\r\n", 2);

	return 0;
}

/*  Returns a resource  */

int Return_Resources(int conn, int resource, struct Request_Info * Requested_Info) {

	char c;
	int i;

	while ((i = read(resource, &c, 1))) {
		if (i < 0)
			Error_Quit("Error reading from file.");
		if (write(conn, &c, 1) < 1)
			Error_Quit("Error sending file.");
	}

	return 0;
}

/*  Tries to open a resource. The calling function can use
 the return value to check for success, and then examine
 errno to determine the cause of failure if neceesary.    */

int Check_Resource(struct Request_Info * Requested_Info) {

	/*  Resource name can contain urlencoded
	 data, so clean it up just in case.    */

	CleanURL(Requested_Info->resource);

	/*  Concatenate resource name to server root, and try to open  */

	strcat(server_root, Requested_Info->resource);
	return open(server_root, O_RDONLY);
}

/*  Returns an error message  */

int Return_Error_Msgs(int conn, struct Request_Info * Requested_Info) {

	char buffer[100];

	if (Requested_Info->status == 400) {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Bad Request</TITLE>\n"
				"</HEAD>\n\n", Requested_Info->status);
		Writeline(conn, buffer, strlen(buffer));
	}
	else if (Requested_Info->status == 403) {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Forbidden</TITLE>\n"
				"</HEAD>\n\n", Requested_Info->status);
		Writeline(conn, buffer, strlen(buffer));
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>400 Bad</TITLE>\n"
				"</HEAD>\n\n", Requested_Info->status);
		Writeline(conn, buffer, strlen(buffer));
	}
	else {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>Server Error%d</TITLE>\n"
				"</HEAD>\n\n", Requested_Info->status);
		Writeline(conn, buffer, strlen(buffer));
	}
	sprintf(buffer, "<BODY>\n<H1>Server Error %d</H1>\n", Requested_Info->status);
	Writeline(conn, buffer, strlen(buffer));

	sprintf(buffer, "<P>The request could not be completed.</P>\n"
			"</BODY>\n</HTML>\n");
	Writeline(conn, buffer, strlen(buffer));

	return 0;

}

/*  Prints an error message and quits  */

void Error_Quit(char const * msg) {
	fprintf(stderr,"WEBSERV: %s\n", msg);
	exit(EXIT_FAILURE);
}

/*  Read a line from a socket  */

ssize_t Readline(int sockd, void *vptr, size_t maxlen) {
	ssize_t n, rc;
	char c, *buffer;

	buffer = vptr;

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
			Error_Quit("Error in Readline()");
		}
	}

	*buffer = 0;
	return n;
}

/*  Write a line to a socket  */

ssize_t Writeline(int sockd, const void *vptr, size_t n) {
	size_t nleft;
	ssize_t nwritten;
	const char *buffer;

	buffer = vptr;
	nleft = n;

	while (nleft > 0) {
		if ((nwritten = write(sockd, buffer, nleft)) <= 0) {
			if (errno == EINTR)
				nwritten = 0;
			else
				Error_Quit("Error in Writeline()");
		}
		nleft -= nwritten;
		buffer += nwritten;
	}

	return n;
}

/*  Removes trailing whitespace from a string  */

int Trim(char * buffer) {
	int n = strlen(buffer) - 1;

	while (!isalnum(buffer[n]) && n >= 0)
		buffer[n--] = '\0';

	return 0;
}

/*  Converts a string to upper-case  */

int StrUpper(char * buffer) {
	while (*buffer) {
		*buffer = toupper(*buffer);
		++buffer;
	}
	return 0;
}

/*  Cleans up url-encoded string  */

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

