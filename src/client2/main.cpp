//
// Created by bensoer on 11/02/16.
//

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <assert.h>
#include "../client/argparcer.h"

using namespace std;

void connectToServer(string host, int port, int socketDescriptor){
    struct hostent	*hp;

    if ((hp = gethostbyname(host.c_str())) == NULL)
    {
        fprintf(stderr, "Unknown server address\n");
        exit(1);
    }else{
        cout << "Main - Hostname Resolved" << endl;
    }

    struct	sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    bcopy(hp->h_addr, (char *)&(server.sin_addr), hp->h_length);

    // Connecting to the server
    if (connect(socketDescriptor, (struct sockaddr *)&(server), sizeof(server)) == -1)
    {
        fprintf(stderr, "Can't connect to server\n");
        perror("connect");
        exit(1);
    }else{
        cout << "Main - Connection Established" << endl;
    }
}

int main(int argc, char * argv[]){


    /**
     * PARAMETERS
     * -s <message> - Set the Message To Be Sent
     * -r 1|0 - Pass 1 or 0. 1 Means to send random character strings of up to 25 characters
     * -h <host> - Set the host to connect to
     * -p <port> - Set the port to connect to the host on
     * -c <number> - Set number of connections this client will make
     */

    ArgParcer parcer;
    string messageToSend = parcer.GetTagData("-s",argv, argc);
    int randomStrings = parcer.GetTagVal("-r", argv, argc);
    string host = parcer.GetTagData("-h", argv, argc);
    int port = parcer.GetTagVal("-p", argv, argc);
    int connections = parcer.GetTagVal("-c", argv, argc);

    int messageToSendLength = 0;
    bool sendRandom = false;

    if(messageToSend.compare("-1")==0){
        messageToSend = "HELLO EVERYBODY";
    }

    if(randomStrings == 1){
        sendRandom = true;
    }

    if(host.compare("-1")==0 || port == -1 || connections == -1){
        cout << "ERROR. Missing Parameters. Exepcted Use:" << endl;
        cout << "client -h <host> -p <port> -c <connections> [-s <message>][-r 1|0]" << endl;
        return 1;
    }

    cout << "Set Message To Send: " << messageToSend << endl;




    int socketDescriptor;
    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Can't create a socket");
        exit(1);
    }else{
        cout << "Main - Socket Created" << endl;
    }

    cout << "Main - Created Socket Descriptor: " << socketDescriptor << endl;
    //descriptors[i] = socketDescriptor;

    //connect to server
    connectToServer("localhost", 4002, socketDescriptor);

    string message= "{HELLO EVERYBODY}";

    //send message
    messageToSendLength = message.size();
    send (socketDescriptor, message.c_str(), messageToSendLength, 0);
    cout << "Main - Sent Initial Message Of: >" << message << "< Over Socket Descriptor: " << to_string(socketDescriptor) << endl;


    //create epoll listener
    int epollDescriptor;
    if((epollDescriptor = epoll_create(10)) < 0){
        cout << getpid() << " Failed To Create epoll Descriptor" << endl;
        exit(1);
    }else{
        cout << getpid() << " Successfully Created epoll Descriptor" << endl;
    }

    struct epoll_event event; //holder for all new events

    //add server socket to the epoll event loop
    event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    event.data.fd = socketDescriptor;
    if(epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, socketDescriptor, &event) == -1){
        cout << getpid() << " Failed To Add Socket Descriptor (" << to_string(socketDescriptor) << ") To The Epoll ("
        << to_string(epollDescriptor) << ") Event Loop" << endl;
        exit(1);
    }else{
        cout << getpid() << " - Successfully Added Socket Descriptor (" << to_string(socketDescriptor)
        << ") To The Epoll (" << to_string(epollDescriptor) << ") Event Loop" << endl;
    }

    struct epoll_event events [10];

    int num_fds = epoll_wait(epollDescriptor, events, 10, -1);
    if (num_fds < 0) {
        cout << getpid() << " - There Was An Error In Epoll Wait" << endl;
        exit(1);
    }

    //sift through reply
    for (unsigned int i = 0; i < num_fds; i++) {
        if (events[i].events & (EPOLLHUP | EPOLLERR)) {
            cout << getpid() << " There Was An Error In An Event From Epoll. Closing File Descriptor" << endl;
            close(events[i].data.fd);
            continue;
            //exit(1);
        }

        if (!(events[i].events & EPOLLIN)) {
            cout << getpid() <<
            " Critical Error. This Event Has Nothing To Read In and Has No Errors. Why Is It Here ?" << endl;
            cout << getpid() << " Now Exploding" << endl;

            assert(events[i].events & EPOLLIN);
            exit(1);
        }


        char recvBuffer[messageToSendLength];
        cout << "Before Read: " << recvBuffer << endl;
        long bytesReceived = read(events[i].data.fd, recvBuffer,
                                  messageToSendLength); //recv(events[i].data.fd, recvBuffer, messageToSendLength, 0);
        cout << "Bytes Received: " << bytesReceived << endl;
        cout << "Main - Got Message Back On SocketDescriptor: " << events[i].data.fd << endl;
        cout << "Message: " << recvBuffer << endl;
    }



 /*   char recvBuffer[messageToSendLength];
    cout << "Before Read: " << recvBuffer << endl;
    long bytesReceived = read(socketDescriptor, recvBuffer, messageToSendLength); //recv(events[i].data.fd, recvBuffer, messageToSendLength, 0);
    cout << "Bytes Received: " << bytesReceived << endl;
    cout << "Main - Got Message Back On SocketDescriptor: " << socketDescriptor << endl;
    cout << "Message: " << recvBuffer << endl;*/

    return 0;

}