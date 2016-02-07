#include <iostream>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "argparcer.h"

using namespace std;

bool continueRunning = true;

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
        char recvBuffer[length];
        recv(socketDescriptor, recvBuffer, length+1, 0);

        cout << "Main - Got Message Back!" << endl;
        cout << recvBuffer << endl;


    }

    close(socketDescriptor);


    return 0;
}