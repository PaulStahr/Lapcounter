/*
Copyright (c) 2019 Paul Stahr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>


#include <iostream>
//#include <ncurses.h>
#include <sched.h>
#include <string.h>
#include <stdint.h>
#include <sstream>
#include <fstream>

#include "server.h"


void WebServer::error(const char *msg)
{
    perror(msg);
}


void *server_thread_fkt(void *x_void_ptr)
{
   WebServer *obj = (WebServer *)x_void_ptr;
   obj->run();
   return NULL;
}

void WebServer::start(){
    running = true;
    pthread_t inc_x_thread;
    if(pthread_create(&inc_x_thread, NULL, server_thread_fkt, this)) {
        std::cerr << "Error creating thread" << std::endl;
    }
}

void WebServer::stop(){
    running = false;
}

void WebServer::setCommandExecutor (std::function<void(std::string, std::ostream &)> ce){
    _command_execute = ce;
}

void WebServer::run(){
     struct sockaddr_in serv_addr, cli_addr;

     int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = 2223;
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
     {   
         error("ERROR on binding");
     }
     listen(sockfd,5);
     socklen_t clilen = sizeof(cli_addr);
     while (running) {
         int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         if (newsockfd < 0) 
             error("ERROR on accept");
         
          //close(sockfd);
          send_and_read(newsockfd);
          close(newsockfd);
     } /* end of while */
     close(sockfd);
}

void WebServer::send_and_read (int sock)
{
   int n;
   char buffer[256];
      
   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) error("ERROR reading from socket");
   std::ostringstream stream;
   std::string command = buffer;
   _command_execute(command, stream);
   std::string answer = stream.str();
   //n = write(sock,answer.c_str(),answer.size());
   n = send(sock,answer.c_str(),answer.size(),0);
   n = read(sock,buffer,255);
   if (n < 0) error("ERROR reading from socket");
   
   if (n < 0) error("ERROR writing to socket");
}
