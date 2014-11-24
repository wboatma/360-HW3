#include "common.h"

void err_n_die(int error)
{
   switch(error) {
      case BADARGS:  
         cout << "Error Incorrect Usage.\n";
         cout << "./simhttp [-p <port>] <directory>\n";
         break;
      case SOCKERR:
         cout << "Error opening socket!\n";
         cout << "Please try your request again.\n";
         break;
      case BINDERR:
         cout << "Error binding port to socket!\n";
         cout << "Please check your port and try your request again.\n";
         break;
      case LISTERR:
         cout << "Error trying to listen on port!\n";
         cout << "Please check your port and try your request again.\n";
         break;
      case RECVERR:
         cout << "Error receiving response.\n";
         cout << "Please try your request again.\n";
         break;
      case READERR:
         cout << "Error reading response.\n";
         break;
   }
   exit(1);
}
