/*
 * http server header
* http structure class
*/
#include "common.h"

class http {
   private:
      const char *server_name;
   
      uint16_t port;
      char *dir;	   // The server directory 
      FILE *page;  	   // The file that the request is asking for
      char *file;	   // The file name
      char *fullpath;  // The full path to the file
      char *method;    // The method called by the client (GET | HEAD)
      char *ext;
      int  file_size;
      
      int listenfd;
      int connfd;
      struct sockaddr_in servaddr;

      char *buffer;
      char recvline[MAXLINE + 1];

   public:
      http();
      http(int port, char *directory);
      ~http();

      void tcp_listen();
      void accept_connection();
      void handle_connection();
      int  parse_request();
      int  check_request(char *token);
      int  check_host(char *token);
      void make_header(int statuscode);
      void write_status(int statuscode);
      void write_content_info();
      void make_content_type();
      int  open_file();
      int  get_ext();
};
