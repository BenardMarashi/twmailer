#include <sys/types.h>
#include <fstream>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <vector>
#include<chrono>
#include <signal.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <vector>
#include <dirent.h>
#include <arpa/inet.h>
#include <ldap.h>
#include <time.h>
#include <algorithm>
#define LDAP_URI "ldap://ldap.technikum-wien.at:389"
#define SEARCHBASE "dc=technikum-wien,dc=at"
#define SCOPE LDAP_SCOPE_SUBTREE
#define BIND_USER "" /* anonymous bind with user and pw empty */
#define BIND_PW ""

#define _DEFAULT_SOURCE
   	
#define BUF 10240
char* u;
char* p;
char* username;
int attemps=0;
char* iptemp;
std::vector<char*> bList;
int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;
int loggedIn=0;
pid_t pid;
 struct sockaddr_in address, cliaddress;
/*---Function declarations---*/
int ldapLogin(std::string  usr, std::string pwd);
void *clientCommunication(void *data, char *dirPath);
void signalHandler(int sig);
int findText(std::string ip);
void createText(std::string ip);
void *getServerResponse(int *current_socket, std::string result[], char *buffer, std::string path);
void changeText(std::string ip);
void no_zombie(int signr);
/*---Main Function---*/
int main(int argc, char **argv)
{
   // Check for incorrect command usage
   if (argc != 3)
   {
      perror("Wrong command usage! Correct use: ./twmailer-server <port> <mail-spool-directoryname>");
      return EXIT_FAILURE;
   }

   // Make the mail-spool-directory (returns 0 if directory already exists)
   int result = mkdir(argv[2], 0777);

   socklen_t addrlen;
   int reuseValue = 1;
   int port = (int)strtol(argv[1], NULL, 10);

   // Signal Handler
   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   // Create Socket
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   // Set Socket Options
   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }

   // Initialize Address
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(port);

   // Bind Socket to Address
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   // Allow Connections
   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested)
   {
      printf("Waiting for connections...\n");

      // Accept incoming connection
      addrlen = sizeof(struct sockaddr_in);
      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }
    if(new_socket>0){

	   // if client in blacklist, close the socket
	    if(findText(inet_ntoa(cliaddress.sin_addr))){	
		printf("client in blacklist\n");
		shutdown(new_socket,SHUT_RDWR);
		close(new_socket);
	     }
	    else
	    {	    
      		// Start communcation with client
      		printf("Client connected from %s:%d...\n",
             	inet_ntoa(cliaddress.sin_addr),
             	ntohs(cliaddress.sin_port));
        //child process is created for serving each new clients
		pid=fork();
	    }
      }

	// child process
	if(pid==0){

      		clientCommunication(&new_socket, argv[2]);
      		new_socket = -1;
		
	}

	if(pid>0){
	
	//parent process
	//Zombies vermeiden
                        (void) signal (SIGCHLD, no_zombie);


	}
   }

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
void *clientCommunication(void *data, char *dirPath)
{
   std::string path = dirPath;
   char buffer[BUF];
   int size;
   int *current_socket = (int *)data;

   // Welcome Message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

   // Response Array
   std::string result[6];

   do
   {
      // Receive Client Response
      size = recv(*current_socket, buffer, BUF - 1, 0);
      if (size == -1)
      {
         if (abortRequested)
         {
            perror("recv error after aborted");
         }
         else
         {
            perror("recv error");
         }
         break;
      }

      if (size == 0)
      {
         printf("Client closed remote socket\n");
         break;
      }

      // Clean up Response
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }

      // Turn Response into readable string
      buffer[size] = '\0';

      // Client sends response as VAL1;VAL2;VAL3
      // Remove all ; and save the values in an array
      int i = 0;
      int count = 0;
      for (int j = 0; j < strlen(buffer); j++)
      {
         if (buffer[j] == ';')
         {
            char temp[j - i + 1];
            // Substring
            memcpy(temp, &buffer[i], j);
            temp[j - i] = '\0';
            result[count] = temp;
            count++;
            i = j + 1;
         }
      }

      // Server side printing of the received command
      std::cout << "Message received! Command: " << result[0] << std::endl;
//	printf("%d",attemps);
      // Send Response to client
      getServerResponse(current_socket, result, buffer, path);

   } while (result[0] != "QUIT" && !abortRequested);

   // Close Socket
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown new_socket");

      }
      if (close(*current_socket) == -1)
      {
         perror("close new_socket");
      }
      *current_socket = -1;
   }

   return NULL;

}

// Signal Handler
void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... ");

      abortRequested = 1;
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("close new_socket");
         }
         new_socket = -1;
      }

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
   }
   else
   {
      exit(sig);
   }
}

// Server response
void *getServerResponse(int *current_socket, std::string result[], char *buffer, std::string path)
{
   // First element in array is always the command
   if (result[0] == "SEND" && loggedIn==1)
   {
      DIR *folder;
      struct dirent *entry;
      int files = 0;
      FILE *fptr;
      int accessible = 1;

      // Extract user input from array
      std::string sender = username;
      std::string receiver = result[1];
      std::string subject = result[2];
      std::string message = result[3];

      // Make and open a user specific directory (returns 0 if directory already exists)
      int result = mkdir((path + "/" + sender).c_str(), 0777);
      folder = opendir((path + "/" + sender).c_str());

      if (folder == NULL)
      {
         perror("Unable to read directory");
         accessible = 0;
      }

      if (accessible)
      {
         // Save all the file names to a vector
         std::vector<std::string> fileNames;
         while ((entry = readdir(folder)))
         {
            files++;
            std::string filename = entry->d_name;
            if (filename == "." || filename == "..")
            {
               continue;
            }
            fileNames.push_back(path + "/" + sender + "/" + filename);
         }

         // Close directory
         closedir(folder);

         int newMsg = 0;

         // Get current biggest message number + 1 (deletion safe)
         if (!fileNames.empty())
         {
            std::string lastFile = fileNames[fileNames.size() - 1];
            lastFile = lastFile.substr((path + "/" + sender + "/" + "message").length(), lastFile.length());
            lastFile = lastFile.substr(0, lastFile.length() - 4);
            newMsg = (int)strtol(lastFile.c_str(), NULL, 10) + 1;
         }

         // Create new file for the message
         fptr = fopen((path + "/" + sender + "/" + "message" + std::to_string(newMsg) + ".txt").c_str(), "w");

         if (fptr == NULL)
         {
            printf("Error!");
            accessible = 0;
         }

         if (accessible)
         {
            // Write input to file
            fprintf(fptr, "%s", (sender + "\n").c_str());
            fprintf(fptr, "%s", (receiver + "\n").c_str());
            fprintf(fptr, "%s", (subject + "\n").c_str());
            fprintf(fptr, "%s", message.c_str());

            fclose(fptr);

            // Send success response
            if (send(*current_socket, "OK", 3, 0) == -1)
            {
               perror("send answer failed");
               return NULL;
            }
         }
      }
      else
      {
         // Send error response
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
   }
   else if (result[0] == "LIST" && loggedIn==1)
   {
      DIR *folder;
      struct dirent *entry;
      int files = 0;
      FILE *fptr;
      int accessible = 1;

      std::string sender = result[1];

      // Open a user specific directory (returns 0 if directory already exists)
      folder = opendir((path + "/" + sender).c_str());

      if (folder == NULL)
      {
         accessible = 0;
      }

      if (accessible)
      {
         // Get all the files(messages) in the user directory
         std::vector<std::string> fileNames;
         while ((entry = readdir(folder)))
         {
            files++;
            std::string filename = entry->d_name;
            if (filename == "." || filename == "..")
            {
               continue;
            }
            fileNames.push_back(path + "/" + sender + "/" + filename);
         }

         closedir(folder);

         std::vector<std::string> subjectLines;

         // Open every file in the folder to access the subject lines
         for (auto a : fileNames)
         {
            fptr = fopen(a.c_str(), "r");

            if (fptr == NULL)
            {
               printf("Error!");
               continue;
            }

            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            int count = 0;
            while ((read = getline(&line, &len, fptr)) != -1)
            {
               count++;
               // Subject line is always in the 3rd line (3 because the count++ is before the if, early iteration)
               if (count == 3)
               {
                  // Save every subject line in the subjectLine vector
                  subjectLines.push_back(line);
               }
            }

            fclose(fptr);
         }

         // Amout of messages = fileNames size
         std::string temp = std::to_string(fileNames.size()) + ";";

         // Add every subject line to the final response string
         for (auto a : subjectLines)
         {
            temp += a.substr(0, a.length() - 1) + ";";
         }

         strcpy(buffer, temp.c_str());

         // Send success response with list of messages
         if ((send(*current_socket, buffer, strlen(buffer), 0)) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
      else
      {
         // Send error response
         if (send(*current_socket, "User does not exist", 20, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
   }
   else if (result[0] == "READ" && loggedIn==1)
   {
      std::string sender = result[1];
      std::string messageNumber = result[2];
      FILE *fptr;
      int accessible = 1;
      std::vector<std::string> msg;

      // Open specific message file
      fptr = fopen((path + "/" + sender + "/message" + messageNumber + ".txt").c_str(), "r");

      if (fptr == NULL)
      {
         printf("Error!");
         accessible = 0;
      }

      if (accessible)
      {
         char *line = NULL;
         size_t len = 0;
         ssize_t read;
         int count = 0;
         while ((read = getline(&line, &len, fptr)) != -1)
         {
            count++;
            // Any line past 3 is the message (multiline safe)
            if (count > 3)
            {
               msg.push_back(line);
            }
         }
         fclose(fptr);

         std::string temp = "OK;";

         // Create final output string
         for (auto a : msg)
         {
            temp += a;
         }

         temp += ";";

         strcpy(buffer, temp.c_str());

         // Send success response with message content
         if ((send(*current_socket, buffer, strlen(buffer), 0)) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
      else
      {
         // Send error response
         if (send(*current_socket, "Message not found", 18, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
   }
   else if (result[0] == "DEL" && loggedIn==1)
   {
      std::string sender = result[1];
     std::string messageNumber = result[2];
      // Delete specific message file
      if (remove((path + "/" + sender + "/message" + messageNumber + ".txt").c_str()) == 0)
      {
         // Send success response
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
      else
      {
         // Send error response
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
   }
   else if(result[0] == "LOGIN"){

	if((ldapLogin(result[1],result[2]))==1)
      {
	 //succesfully logged
	 loggedIn=1;
	 attemps=0;
         // Send success response
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
      else
      {  
	//failed login
	attemps=attemps+1;
	printf("\n Attemps:%d\n",attemps);
	
    			if(attemps>2)
    			{
					char* ipa;
					strcpy(ipa,inet_ntoa(cliaddress.sin_addr));
	  			//	printf("\n%s - %s\n",inet_ntoa(cliaddress.sin_addr),ipa);
				//	ip added to the file of blacklisted ips.
	     				createText(ipa);

	 			   auto start = std::chrono::steady_clock::now(); // start time
    	   			   while(1)
	   			{	//check if 60 secs already passed.
			     		if(std::chrono::steady_clock::now() - start > std::chrono::seconds(60))  
	  				{
						//delete ip from the blacklist
						changeText(ipa);
						printf("\nClient unbanned\n");
						//set attemps to 0
					  	 attemps=0;
		  				 break;
					 }
	   				else
					{
						// send message of the one minute-ban
        	 				send(*current_socket, "YOU ARE BLOCKED FOR 1 MINUTE!",30 , 0);
						close(*current_socket);
						
					}
	   			}
			}
         // Send error response
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }

   
   }
   else if (result[0] == "QUIT")
   {
      return NULL;
   }

   else if(result[0] == "LOGOUT")
   {

	loggedIn=0;
	if(loggedIn==0)
      {
	 //succesfully loggedOUT
         // Send success response
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
      else
      {
         // Send error response
         if (send(*current_socket, "ERR", 4, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
   }
   else
   {
      if (!result[0].empty() && loggedIn==1)
      {
         // Send invalid command response
         if (send(*current_socket, "Not a valid command!", 21, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
	
      else if (!result[0].empty() && loggedIn==0)
      {
         // Send invalid command response
         if (send(*current_socket, "You must login!", 16, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }



      else
      {
         // Fix for a weird bug that loops an extra time
         if (send(*current_socket, " ", 2, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
   }

   return NULL;
}


   int ldapLogin(std::string usr, std::string pwd)
{
    username = strcpy(new char[usr.length() + 1], usr.c_str());
    char* password = strcpy(new char[pwd.length() + 1], pwd.c_str());
  // printf("%s %s", username,password);

    LDAP *ld;                /* LDAP resource handle */
    LDAPMessage *result, *e; /* LDAP result handle */
    BerElement *ber;         /* array of attributes */
    char *attribute;
    char filter[50];
    BerValue **vals;

    BerValue *servercredp;
    BerValue cred;
    cred.bv_val = BIND_PW;
    cred.bv_len = strlen(BIND_PW);
    int i, rc = 0;

    sprintf(filter, "(uid=%s)", username);
    printf("Try login for: %s\n", filter);

    const char *attribs[] = {"uid", "cn", NULL}; /* attribute array for search */

    int ldapversion = LDAP_VERSION3;

    /* setup LDAP connection */
    if (ldap_initialize(&ld, LDAP_URI) != LDAP_SUCCESS)
    {
        fprintf(stderr, "ldap_init failed");
        return 0;
    }

    printf("connected to LDAP server %s\n", LDAP_URI);

    if ((rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &ldapversion)) != LDAP_SUCCESS)
    {
        fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return 0;
    }

 if ((rc = ldap_start_tls_s(ld,NULL,NULL)) != LDAP_SUCCESS)
    {
        fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return 0;
    }

    /* anonymous bind */
    rc = ldap_sasl_bind_s(ld, BIND_USER, LDAP_SASL_SIMPLE, &cred, NULL, NULL, &servercredp);

    if (rc != LDAP_SUCCESS)
    {
        fprintf(stderr, "LDAP bind error: %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return 0;
    }
    else
    {
        printf("bind successful\n");
    }

    /* perform ldap search */
    rc = ldap_search_ext_s(ld, SEARCHBASE, SCOPE, filter, (char **)attribs, 0, NULL, NULL, NULL, 500, &result);

    if (rc != LDAP_SUCCESS)
    {
        fprintf(stderr, "LDAP search error: %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return 0;
    }

    if (ldap_count_entries(ld, result) != 1)
    {
        return 0;
    }
    //Takes the result makes new binding with the user's name and password
    if (ldap_first_entry(ld, result) != NULL)
    {
        e = ldap_first_entry(ld, result);
        strcpy(filter, ldap_get_dn(ld, e));
        printf("FILTER: %s\n", filter);

        cred.bv_val = password;
        cred.bv_len = strlen(password);
        rc = ldap_sasl_bind_s(ld, filter, LDAP_SASL_SIMPLE, &cred, NULL, NULL, &servercredp);

        if (rc != LDAP_SUCCESS)
        {
            fprintf(stderr, "LDAP search error: %s\n", ldap_err2string(rc));
            ldap_unbind_ext_s(ld, NULL, NULL);
            return 0;
        }
    }

    ldap_unbind_ext_s(ld, NULL, NULL);
    return 1;
}

// write the banned-ip in the file
void  createText (std::string ip) {
	std::ofstream myfile;
  myfile.open ("ip.txt",std::ios_base::app);
  myfile << ip;
  myfile << "\n";
  myfile.close();
 
}
// check if ip wanting to connect is banned
int findText(std::string ip){
  std::string line;
  std::ifstream myfile ("ip.txt");
 
  if (myfile.is_open() && myfile.peek() != std::ifstream::traits_type::eof())
  {
    while ( getline (myfile,line) )
    {
      if(ip==line) return 1;
    }
    myfile.close();
    return 0;
  }

  else {
          printf("Unable to open file");
          return 0;
  }

}
// delete banned ip 
void changeText(std::string ip){
	std::string line;
    // open input file
	std::ifstream in("ip.txt");
    if( !in.is_open())
    {
          printf("Input file failed to open\n");
          
    }
    // now open temp output file
    std::ofstream out("outfile.txt");
    // loop to read/write the file.  Note that you need to add code here to check
    // if you want to write the line
    while( getline(in,line) )
    {
        if(line != ip)
            out << line << "\n";
    }
    in.close();
    out.close();    
    // delete the original file
    remove("ip.txt");
    // rename old to new
    rename("outfile.txt","ip.txt");

    // all done!


}

//No Zombie Code aus den Folien
void no_zombie(int signr)
{
        pid_t pid;
        int ret;
        while ((pid = waitpid(-1, &ret, WNOHANG)) > 0)
        {
		std::cout << "Child-process with pid " << pid << " stopped." << std::endl;
        }
        return;
}

