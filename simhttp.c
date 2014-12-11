/* 
 * William Beasley - C88846536
 * William Boatman - C16779898
 * CPSC 3600 HW3
 * Dr. Sekou Remy
 * TCP HTTP Server
 * Parses HTTP Requests such as GET
 * Displays the pages and images on websites if accessed
 */
 
#include "simhttp.h"

/***Macro Definitions***/
#define DEFINED_LISTEN_VAL      	1024
#define DEFAULT_SERVER_PORT 		8080
#define MAX_REQUESTED_LINE_SIZE     1024

/***Global Declerations***/
static char server_root[1000] = "./";
char file_path[1000] = { 0 };
char *months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul",
												"Aug","Sep","Oct","Nov","Dec"};
char *days[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
char *http_method[] = {"GET", "HEAD", "UNSUPPORTED"};
struct request_info requested_info;


int main(int argc, char *argv[]) {
	int sock, connection;
	struct sockaddr_in servaddr;
	unsigned short servPort;

	if (argc >= 3) {
		servPort = atoi(argv[2]);
	} else {
		servPort = DEFAULT_SERVER_PORT ;
	}

	if (argc == 2) {
		strcpy(server_root, argv[1]);
	} else if(argc == 4) {
		strcpy(server_root, argv[3]);
	}

	// Create our tcp socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		die_with_error("Error: Server couldn't create listening socket.");

	int optval = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	// Setup our socket address structure
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servPort);

	if (bind(sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
		die_with_error("Error: Server couldn't bind listening socket.");
	
	// Listen for incoming connections
	if (listen(sock, DEFINED_LISTEN_VAL) < 0)
		die_with_error("Error: call to listen failed.");
	
	while (1) {
		if ((connection = accept(sock, NULL, NULL)) < 0) // Wait for connection
			die_with_error("Error: failed on calling accept()");

		request_service(connection);

		printf("%s\t", http_method[requested_info.http_method]);
		printf("%s\t", requested_info.current_user_resource);
		print_results();
		printf("\t%d\n", requested_info.http_request_status);
		
		free(requested_info.current_user_resource);
		if (close(connection) < 0)
			die_with_error("Error: Failed when closing connection socket.");
		exit(1);
	}
	
	return EXIT_FAILURE; 
}

void print_results() {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	printf("%d %s %d %d:%02d ", 
		tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
}

int request_service(int connection) {
	int current_user_resource = 0;

	request_initial_info(&requested_info);
	
	if (get_request(connection, &requested_info) < 0) {
		requested_info.http_request_status = 400;
		return -1;
	}

	if (requested_info.http_request_status == 200) {
		if ((current_user_resource = check_resource(&requested_info)) < 0) {
			if (errno == EACCES)
				requested_info.http_request_status = 403;
			else
				requested_info.http_request_status = 404;
		}
	}
	
	if (requested_info.type == FULL)
		out_http_header(connection, &requested_info);

	if (requested_info.http_request_status == 200) {
		if (return_resources(connection, current_user_resource, &requested_info))
			die_with_error("Error: Something went wrong returning current_user_resource.");
	} else
		return_error_message(connection, &requested_info);

	if (current_user_resource > 0)
		if (close(current_user_resource) < 0)
			die_with_error("Error: Failed during closing of current_user_resource.");

	return 0;
}

int parse_http_request(char *buffer, struct request_info *requested_info) {
	char *temp;
	char *endptr;
	int len = 0;

	if (requested_info->http_method == UNSUPPORTED && 
												 requested_info->http_request_status == 200) {
		if (!strncmp(buffer, "GET /../", 8)) {
			requested_info->http_method = GET;
			requested_info->http_request_status = 403;
			buffer +=4;
		} else if (!strncmp(buffer, "GET /", 5)) {
			requested_info->http_method = GET;
			buffer += 4;
		} else if (!strncmp(buffer, "HEAD /", 6)) {
			requested_info->http_method = HEAD;
			buffer += 5;
		} else if (strncmp(buffer, "GET ", 4) == 0 && strncmp(buffer, "GET /", 5) != 0) {
			requested_info->http_method = GET;
			requested_info->http_request_status = 400;
			buffer += 5;
		} else {
			requested_info->http_method = UNSUPPORTED;
			requested_info->http_request_status = 405;
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
			requested_info->http_request_status = 400;
			return -1;
		}

		requested_info->current_user_resource = malloc(len);
		bzero(requested_info->current_user_resource, len);
		strncpy(requested_info->current_user_resource, buffer, len);

		if (strstr(buffer, "HTTP/"))
			requested_info->type = FULL;
		else
			requested_info->type = SIMPLE;

		return 0;
	}

	endptr = strchr(buffer, ':');
	if (endptr == NULL) {
		requested_info->http_request_status = 400;
		return -1;
	}

	temp = malloc((endptr - buffer) + 1);
	strncpy(temp, buffer, (endptr - buffer));
	string_to_upper(temp);

	buffer = endptr + 1;
	while (*buffer && isspace(*buffer))
		++buffer;
	if (*buffer == '\0')
		return 0;

	if (!strcmp(temp, "USER-AGENT")) {
		requested_info->current_user = malloc(strlen(buffer) + 1);
		strcpy(requested_info->current_user, buffer);
	} else if (!strcmp(temp, "REFERER")) {
		requested_info->referer = malloc(strlen(buffer) + 1);
		strcpy(requested_info->referer, buffer);
	}

	free(temp);
	return 0;
}

int get_request(int connection, struct request_info *requested_info) {
	char buffer[MAX_REQUESTED_LINE_SIZE ] = { 0 };
	int return_val;
	fd_set fds;
	struct timeval tv;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	do {
		FD_ZERO(&fds);
		FD_SET(connection, &fds);

		return_val = select(connection + 1, &fds, NULL, NULL, &tv);
		if (return_val < 0) {
			die_with_error("Error: Failed when calling select() \
											in get_request() http_method.");
		} else if (return_val == 0) {
			return -1;
		} else {
			read_requested_line(connection, buffer, MAX_REQUESTED_LINE_SIZE  - 1);
			trim_string_buffer(buffer);

			if (buffer[0] == '\0')
				break;

			if (parse_http_request(buffer, requested_info))
				break;
		}
	} while (requested_info->type != SIMPLE);

	return 0;
}

void request_initial_info(struct request_info *requested_info) {
	requested_info->current_user = NULL;
	requested_info->referer = NULL;
	requested_info->current_user_resource = NULL;
	requested_info->http_method = UNSUPPORTED;
	requested_info->http_request_status = 200;
	bzero(file_path, 1000);
	strcpy(file_path, server_root);
}

void free_requested_information(struct request_info *requested_info) {
	if (requested_info->current_user)
		free(requested_info->current_user);
	if (requested_info->referer)
		free(requested_info->referer);
	if (requested_info->current_user_resource)
		free(requested_info->current_user_resource);
}

int out_http_header(int connection, struct request_info *requested_info) {
	char buffer[100];
	struct stat fileStat;
    stat(server_root,&fileStat);
	time_t curr;
	struct tm * date;
	time(&curr);
	date = localtime(&curr);

	struct tm * la;
	la = localtime(&(fileStat.st_atime));
	
	sprintf(buffer, "HTTP/1.1 %d OK\r\n", requested_info->http_request_status);
	write_http_line(connection, buffer, strlen(buffer));

	memset(buffer, 0, 100);
	sprintf(buffer, "Date: %s, %d %s %d %d:%d:%d\r\n", 
		days[date->tm_wday], date->tm_mday, months[date->tm_mon], date->tm_year + 1900,
		date->tm_hour, date->tm_min, date->tm_sec);
	write_http_line(connection, buffer, strlen(buffer));

	memset(buffer, 0, 100);
	sprintf(buffer, "Last-Modified: %s, %d %s %d %d:%d:%d\r\n", 
		days[la->tm_wday], la->tm_mday, months[la->tm_mon], la->tm_year + 1900,
		la->tm_hour, la->tm_min, la->tm_sec);
	write_http_line(connection, buffer, strlen(buffer));
	write_http_line(connection, "Content -Type: text/html\r\n", 26);

	memset(buffer, 0, 100);
	sprintf(buffer, "Content -Length: %ld\r\n", fileStat.st_size);
	write_http_line(connection, buffer, strlen(buffer));
 
	write_http_line(connection, "Server: simhttp\r\n", 17);
	write_http_line(connection, "\r\n", 2);

	return 0;
}

int return_resources(int connection, int current_user_resource, 
													struct request_info *requested_info) {
	char c;
	int i;

	while ((i = read(current_user_resource, &c, 1))) {
		if (i < 0)
			die_with_error("Error: Failed when reading from file.");
		if (write(connection, &c, 1) < 1)
			die_with_error("Error: Failed when sending file.");
	}

	return 0;
}

int check_resource(struct request_info *requested_info) {
	cleanURL(requested_info->current_user_resource);
	strcat(file_path, requested_info->current_user_resource);
	return open(file_path, O_RDONLY);
}

int return_error_message(int connection, struct request_info *requested_info) {
	char buffer[100];

	if (requested_info->http_request_status == 400) {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Bad Request</TITLE>\n</HEAD>\n\n", 
																	requested_info->http_request_status);
		write_http_line(connection, buffer, strlen(buffer));
	}
	else if (requested_info->http_request_status == 403) {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Forbidden</TITLE>\n</HEAD>\n\n", 
																 requested_info->http_request_status);
		write_http_line(connection, buffer, strlen(buffer));
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>%d Bad</TITLE>\n</HEAD>\n\n", 
															 requested_info->http_request_status);
		write_http_line(connection, buffer, strlen(buffer));
	}
	else {
		sprintf(buffer, "<HTML>\n<HEAD>\n<TITLE>Server Error%d</TITLE>\n</HEAD>\n\n", 
																	requested_info->http_request_status);
		write_http_line(connection, buffer, strlen(buffer));
	}
	sprintf(buffer, "<BODY>\n<H1>Server Error %d</H1>\n", 
											requested_info->http_request_status);
	write_http_line(connection, buffer, strlen(buffer));

	sprintf(buffer, "<P>The request could not be completed.</P>\n"
			"</BODY>\n</HTML>\n");
	write_http_line(connection, buffer, strlen(buffer));

	return 0;
}

void die_with_error(char const *message) {
	fprintf(stderr,"WEBSERV: %s\n", message);
	exit(EXIT_FAILURE);
}

ssize_t read_requested_line(int sockd, void *void_pointer, size_t maxlen) {
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
			die_with_error("Error in read_requested_line()");
		}
	}
	//printf("%s\n", buffer);
	*buffer = 0;
	return n;
}

ssize_t write_http_line(int sockd, const void *void_pointer, size_t n) {
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
				die_with_error("Error in write_http_line()");
		}
		nleft -= nwritten;
		buffer += nwritten;
	}

	return n;
}

int trim_string_buffer(char *buffer) {
	int n = strlen(buffer) - 1;

	while (!isalnum(buffer[n]) && n >= 0)
		buffer[n--] = '\0';

	return 0;
}

int string_to_upper(char *buffer) {
	while (*buffer) {
		*buffer = toupper(*buffer);
		++buffer;
	}
	return 0;
}

void cleanURL(char *buffer) {
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

