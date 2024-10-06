#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>

#define BUF 10240

/*---Function declarations---*/
int getUserInput(char buffer[], int create_socket);

/*---Main Function---*/
int main(int argc, char **argv)
{
   // Check for incorrect command usage
   if (argc != 3)
   {
      perror("Wrong command usage! Correct use: ./twmailer-client <ip> <port>");
      return EXIT_FAILURE;
   }

   // Socket Creation
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
   int isQuit;
   int port = (int)strtol(argv[2], NULL, 10);

   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   // Adress initiliation, argv[1] = ip
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_port = htons(port);
   inet_aton(argv[1], &address.sin_addr);

   // Connect to server
   if (connect(create_socket,
               (struct sockaddr *)&address,
               sizeof(address)) == -1)
   {
      perror("Connect error - no server available");
      return EXIT_FAILURE;
   }


   // Receive Welcome Message
   size = recv(create_socket, buffer, BUF - 1, 0);
   if (size == -1)
   {
      perror("recv error");
   }
   else if (size == 0)
   {
      printf("Server closed remote socket\n");
   }
   else
   {
      
      printf("Connection with server (%s) established\n",inet_ntoa(address.sin_addr));
      buffer[size] = '\0';
      printf("%s\n", buffer);
     
   }

   // Input Loop
   do
   {
      // CMD: UserInput in buffer
      printf(">> ");
      if (fgets(buffer, BUF, stdin) != NULL)
      {
         int size = strlen(buffer);

         // Clean Input
         if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
         {
            size -= 2;
            buffer[size] = 0;
         }
         else if (buffer[size - 1] == '\n')
         {
            --size;
            buffer[size] = 0;
         }

         // Check if input is QUIT to end loop
         isQuit = strcmp(buffer, "QUIT") == 0;

         // Get User Input [Check Function]
         if (getUserInput(buffer, create_socket))
         {
            break;
         }

         // Receive Server Response
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1)
         {
            perror("recv error");
            break;
         }
         else if (size == 0)
         {
            printf("Server closed remote socket\n");
            break;
         }
         else
         {
            buffer[size] = '\0';

            // Server sends response as VAL1;VAL2;VAL3 OR a single word response
            // Remove all ; and save the values in a vector
            std::vector<std::string> result;
            int i = 0;
            for (int j = 0; j < strlen(buffer); j++)
            {
               if (buffer[j] == ';')
               {
                  char temp[j - i + 1];
                  memcpy(temp, &buffer[i], j);
                  temp[j - i] = '\0';
                  std::string realString = temp;
                  result.push_back(realString);
                  i = j + 1;
               }
            }

            // Print response
            // If the vector is empty -> server sent a single word response
            if (!result.empty())
            {
               for (auto a : result)
               {
                  std::cout << a << std::endl;
               }
            }
            else
            {
               std::cout << buffer << std::endl;
            }
         }
      }
   } while (!isQuit);

   // Close Socket
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown create_socket");
      }
      if (close(create_socket) == -1)
      {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}

/*---Function implementations---*/
int getUserInput(char buffer[], int create_socket)
{
   // User Input = SEND
   if (strcmp(buffer, "SEND") == 0)
   {
      // Get all the required user input, within allowed values
      std::string receiver;
      char subject[80];
      std::string message;
      char *line = NULL;


      std::cout << "Receiver: ";
      std::cin >> receiver;

      std::cout << "Subject: ";
      std::cin >> subject;

      std::cout << "Message: " << std::endl;
      // Multiline input, ends on \n.\n (not included in the final message string)
      size_t n = 0;
      while (getline(&line, &n, stdin))
      {
         if (strcmp(line, ".\n") == 0)
         {
            break;
         }
         message += line;
      }

      // Concat the input with ;
      strcat(buffer, ";");
      strcat(buffer, receiver.c_str());
      strcat(buffer, ";");
      strcat(buffer, subject);
      strcat(buffer, ";");
      strcat(buffer, message.c_str());
      strcat(buffer, ";");

      // Send buffer to server
      if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
      {
         perror("send error");
         return 1;
      }
   }
   else if (strcmp(buffer, "LIST") == 0)
   {
      std::string username;
     std::cout << "Username: ";
      std::cin >> username;

       //Concat the input with ;
     strcat(buffer, ";");
      strcat(buffer, username.c_str());
      strcat(buffer, ";");

      // Send buffer to server
      if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
      {
         perror("send error");
         return 1;
      }
   }
   else if (strcmp(buffer, "READ") == 0)
   {
      std::string username;
      std::string messageNumber;

      std::cout << "Username: ";
      std::cin >> username;
      std::cout << "Message Number: ";
      std::cin >> messageNumber;

      //Concat the input with ;
      strcat(buffer, ";");
      strcat(buffer, username.c_str());
      strcat(buffer, ";");
      strcat(buffer, messageNumber.c_str());
      strcat(buffer, ";");

      // Send buffer to server
      if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
      {
         perror("send error");
         return 1;
      }
   }
   else if (strcmp(buffer, "DEL") == 0)
   {
      
      std::string username;
      std::string messageNumber;

      std::cout << "Username: ";
      std::cin >> username;
      std::cout << "Message Number: ";
      std::cin >> messageNumber;

      //Concat the input with ;
      strcat(buffer, ";");
      strcat(buffer, username.c_str());
      strcat(buffer, ";");
      strcat(buffer, messageNumber.c_str());
      strcat(buffer, ";");

      // Send buffer to server
      if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
      {
         perror("send error");
         return 1;
      }
   }

     else if (strcmp(buffer, "LOGIN") == 0)
   {
      std::string username;
      std::string password;

      std::cout << "Username: ";
      std::cin >> username;
      password=getpass("Password:");

      // Concat the input with ;
      strcat(buffer, ";");
      strcat(buffer, username.c_str());
      strcat(buffer, ";");
      strcat(buffer, password.c_str());
      strcat(buffer, ";");

      // Send buffer to server
      if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
      {
         perror("send error");
         return 1;
      }
   }


     else if (strcmp(buffer, "LOGOUT") == 0)
   {

      // Concat the input with ;
      strcat(buffer, ";");

      // Send buffer to server
      if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
      {
         perror("send error");
         return 1;
      }
   }


   // Command checking happens serverside!
   else
   {
      // Single word input like QUIT still needs the ; for the server to be able to read it
      strcat(buffer, ";");

      // Send buffer to server
      if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
      {
         perror("send error");
         return 1;
      }
   }
   return 0;
}
