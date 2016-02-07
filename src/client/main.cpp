#include <iostream>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "argparcer.h"
#include <sys/epoll.h>
#include <assert.h>

using namespace std;

bool continueRunning = true;
const unsigned int EPOLL_QUEUE_LENGTH = 10;

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
        cout << "MAin - Connection Established" << endl;
    }
}

void gen_random(char *s, const int len) {
    static const char alphanum[] =
            "0123456789"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

void shutdownClient(int signo){
    continueRunning = false;
}

int main(int argc, char * argv[]) {

    ArgParcer parcer;
    string messageToSend = parcer.GetTagData("-s",argv, argc);
    int randomStrings = parcer.GetTagVal("-r", argv, argc);
    bool sendRandom = false;

    if(messageToSend.compare("-1")==0){
        messageToSend = "HELLO EVERYBODY";
    }

    if(randomStrings == 1){
        sendRandom = true;
    }


    //register sig terminate
    struct sigaction act;
    act.sa_handler = shutdownClient;
    act.sa_flags = 0;
    if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
    {
        perror ("Failed to set SIGINT handler");
        exit(1);
    }

    //create socket
    int socketDescriptor;
    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Can't create a socket");
        exit(1);
    }else{
        cout << "Main - Socket Created" << endl;
    }

    //connect to server
    connectToServer("localhost", 4002, socketDescriptor);

    struct epoll_event events [EPOLL_QUEUE_LENGTH];
    struct epoll_event event; //holder for all new events


    int epollDescriptor;
    if((epollDescriptor = epoll_create(EPOLL_QUEUE_LENGTH)) < 0){
        cout << getpid() << " Failed To Create epoll Descriptor" << endl;
        exit(1);
    }else{
        cout << getpid() << " Successfully Created epoll Descriptor" << endl;
    }

    //add server socket to the epoll event loop
    event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    event.data.fd = socketDescriptor;
    if(epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, socketDescriptor, &event) == -1){
        cout << getpid() << " Failed To Add Socket Descriptor To The Epoll Event Loop" << endl;
        exit(1);
    }else{
        cout << getpid() << " - Successfully Added Socket Descriptor To The Epoll Event Loop" << endl;
    }


    //while 1
    while(continueRunning){

        string message;

        if(sendRandom){
            char chrMessage[25];
            gen_random(chrMessage, 25);
            message = "{" + to_string(*chrMessage) + "}";

        }else{
            message = "{" + messageToSend + "}";
        }

        //send message
        int length = message.size();
        send (socketDescriptor, message.c_str(), length, 0);
        cout << "Main - Sent Message" << endl;

        //wait for reply

        int num_fds = epoll_wait(epollDescriptor, events, EPOLL_QUEUE_LENGTH, -1);
        if (num_fds < 0) {
            cout << getpid() << " - There Was An Error In Epoll Wait" << endl;
            exit(1);
        }

        for (unsigned int i = 0; i < num_fds; i++) {
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                cout << getpid() << " There Was An Error In An Event From Epoll. Closing File Descriptor" << endl;
                close(events[i].data.fd);
                exit(1);
            }

            if (!(events[i].events & EPOLLIN)) {
                cout << getpid() <<
                " Critical Error. This Event Has Nothing To Read In and Has No Errors. Why Is It Here ?" << endl;
                cout << getpid() << " Now Exploding" << endl;

                assert (events[i].events & EPOLLIN);
                exit(1);
            }

            char recvBuffer[length];
            recv(events[i].data.fd, recvBuffer, length+1, 0);
            cout << "Main - Got Message Back!" << endl;
            cout << recvBuffer << endl;

        }





    }

    close(socketDescriptor);


    return 0;
}