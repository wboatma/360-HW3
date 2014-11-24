#include "http.h"
#include "common.h"

http::http() {  }


http::http(int port, char *directory)
{
   server_name = "StillBetterThanApache/1.3";
   this->port = port;
   dir = directory;
}


http::~http()
{
   free(method);
}


int main(int argc, char **argv)
{
 	char dir[64] = "./";
 
   if(argc > 4)
   {
      err_n_die(BADARGS);
   } 

   //copy the directory and get the port	
	if(argc == 3 || argc == 4) {
		memcpy(dir, argv[argc - 1], strlen(argv[argc - 1]));	
	} 
   
   int port = 8080;

   int c;
   while((c = getopt(argc, argv, "p:")) != -1)
   {
      switch(c) {
         case 'p':
            port = atoi(optarg);
            break;
      }
   }

   //Create our http class sending the port and directory
   http *server = new http(port, dir);
   cout << "Server open on port: " << port << endl;
   cout << "Server directory:    " << dir << endl;
   
   //Setup our server to listen on port <port>
   server->tcp_listen();

   delete(server);

   return 0;
}


void http::tcp_listen()
{
   if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      err_n_die(SOCKERR);
   }
   
   bzero(&servaddr, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
   servaddr.sin_port = htons(this->port);
   
   if((bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) < 0)
   {
      err_n_die(BINDERR);
   }
   
   if((listen(listenfd, 1024)) < 0)
   {
      err_n_die(LISTERR);
   }
   
   //Wait for a connection, and then connect to it
   accept_connection();
}


void http::accept_connection()
{
   while(true)
   {
      connfd = accept(listenfd, (struct sockaddr *) NULL, NULL);
      handle_connection();
   }
}


void http::handle_connection()
{
   int n;
   int status_code;
   
   while((n = read(connfd, recvline, MAXLINE)) > 0)
   {
      recvline[n] = 0; 
      
      //if end of message, break out of loop
      if(recvline[n - 1] == '\n')
      {
         break;
      }
   }
   if(n < 0)
   {
      err_n_die(READERR);
   }

   //Parse the request
   status_code = parse_request();
   
   write(connfd, buffer, strlen(buffer) + file_size);
   
   close(connfd);
}


int http::parse_request()
{
   buffer = (char *)malloc(MAXLINE);
   char *request = strtok(recvline, "\r\n");
   char *host;
   
   char *tok = strtok(NULL, "\r\n");
   while(tok != NULL)
   {
      if(strncmp(tok, "Host:", 5) == 0)
      {
         host = tok;
      }

      tok = strtok(NULL, "\r\n");
   }
   
   int status_code = check_request(request);
   if(status_code != 200)
   {
      make_header(status_code);
      return status_code;
   }
   
   char *fpath = (char *)malloc(strlen(dir) + strlen(file) + 1);
   strcpy(fpath, dir);
   strcat(fpath, "/"); // MIGHT NOT NEED.... REVIEW THIS
   strcat(fpath, file);
   fullpath = fpath;
   
   status_code = open_file();
   if(status_code != 200)
   {
      make_header(status_code);
      free(fpath);
      return status_code;
   }
   
   status_code = check_host(host);
   
   make_header(status_code);
   free(fpath);
   return status_code;
}



void http::make_header(int statuscode)
{
   time_t ticks;
   
   //Find the files last modified time
   char date[30];
   struct stat modified;
   
   
   //add the response and status code
   strcpy(buffer, "HTTP/1.1 ");
   write_status(statuscode);
   
   //add the date header field
   ticks = time(NULL);
   strcat(buffer, "Date: ");
   strcat(buffer, ctime(&ticks));
   
   if(fullpath != NULL && (statuscode == 200 || statuscode == 405))
   {
      stat(fullpath, &modified);
      strcpy(date, ctime(&(modified.st_mtime)));
   
      //add the Last Modified header to the buffer
      strcat(buffer, "Last-Modified: ");
      strcat(buffer, date);
   }
   
   strcat(buffer, "Connection: close\r\n");
   
   //Server output
   char output[400];
   bzero(output, 400);
   char status[4];
   sprintf(status, "%d", statuscode);

   strcpy(output, method); strcat(output, "\t"); 
   if(file != NULL && strcmp(file, "ico") != 0) 
   {
      strcat(output, file); 
   }
   
   strcat(output, "\t"); strncat(output, ctime(&ticks), strlen(ctime(&ticks))-1); 
   strcat(output, "\t"); strcat(output, status); 
   
   cout << output << "\n";

   if(statuscode == 200)
   {
      write_content_info();
   }
   fflush(stdout);
}


void http::write_status(int statuscode)
{
   switch(statuscode) 
   {
      case 200:
         strcat(buffer, "200 OK\r\n");
         break;
      case 400:
         strcat(buffer, "400 Bad Request\r\n");
         break;
      case 403:
         strcat(buffer, "403 Forbidden\r\n");
         break;
      case 404:
         strcat(buffer, "404 Not Found\r\n");
         break;
      case 405:
         strcat(buffer, "405 Method Not Allowed\r\n");
         strcat(buffer, "Allow: \r\n");
         break;
   }
}


void http::write_content_info()
{
   char content[file_size];
   bzero(&content, sizeof(content));
   fread(content, 1, file_size, page);

   fclose(page);
   
   make_content_type();
   strcat(buffer, "Content-Length: ");
   
   
   snprintf(buffer+strlen(buffer), sizeof(buffer), "%d", file_size);
   strcat(buffer, "\r\n");
   
   strcat(buffer, "Server: ");
   strcat(buffer, server_name);
   strcat(buffer, "\r\n");
   
   //the empty line indicating end of header
   strcat(buffer, "\r\n");
   
   memcpy(buffer + strlen(buffer), content, file_size);
}


int http::check_request(char *token)
{
   char *tok = strtok(token, " ");
   int dir_depth = 1;
   if(dir[1] == '/')
   {
      dir_depth = 2;
   }
   else if(dir[2] == '/')
   {
      dir_depth = 3;
   }

   method = (char *)malloc(strlen(tok));
   strcpy(method, tok);
   
   if(strcmp(tok, "GET") == 0 || strcmp(tok, "HEAD") == 0)
   {
      tok = strtok(NULL, " ");
      char *tok_temp = strtok(tok, "/");
      //Make sure the requested page is within the server directory
      if(strcmp(tok_temp, dir+dir_depth) == 0)
      {
         tok = strtok(NULL, " ");
         if(tok != NULL)
         {
            file = tok;
            return get_ext();
         }
         return 200;
      }
      else
      {
         //returns 403 page
         return 403;
      }
   }
   else
   {
      //returns 405 page
      return 405;
   }
}


int http::check_host(char *token)
{
   char *tok = strtok(token, " "); //parse past the 'Host:' identifier
   if((tok = strtok(NULL, "\r\n")) != NULL) 
   {
      return 200;
   }
   return 400;
}


int http::open_file()
{
   struct stat filesize;

   stat(fullpath, &filesize);
   file_size = filesize.st_size;
   
   free(buffer);
   buffer = (char *)malloc(file_size + MAXLINE);
   bzero(buffer, file_size + MAXLINE);
   
   if(strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0 || strcmp(ext, "js") == 0 || strcmp(ext, "php") == 0)
   {
      page = fopen(fullpath, "r");
   }
   else 
   {
      page = fopen(fullpath, "rb");
   }
   
   if(page == NULL)
   {
      return 404;
   }
   return 200;
}


void http::make_content_type()
{   
   strcat(buffer, "Content-Type: ");
   if(strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0)
   {
      strcat(buffer, "text/html\r\n");
   } 
   else if(strcmp(ext, "js") == 0)
   {
      strcat(buffer, "application/javascript\r\n");
   }
   else if(strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
   {
      strcat(buffer, "image/jpeg\r\n");
   }
   else if(strcmp(ext, "pdf") == 0)
   {
      strcat(buffer, "application/pdf\r\n");
   }
   else
   {
      strcat(buffer, "application/octet-stream\r\n");
   }
}


int http::get_ext()
{
   char file_type[strlen(file)];
   strcpy(file_type, file);
   
   char *tok;
   if((tok = strtok(file_type, ".")) == NULL)
   {
      return 404;
   }
   if((tok = strtok(NULL, "\0")) == NULL)
   {
      return 404;
   }
   
   ext = (char *)malloc(strlen(tok));
   strcpy(ext, tok);
   
   return 200;
}