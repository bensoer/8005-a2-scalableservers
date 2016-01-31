#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <netdb.h>

#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <bits/signum.h>
#include <signal.h>
#include <vector>
#include <algorithm>

#include "ConnectionProcess.h"

using namespace std;

bool continueRunning = true;
int pipeConnectionToParent[2];
vector<pid_t> children;

void createChildProcesses(int socketDescriptor, int * pipeToParent, vector<pid_t> children, unsigned int howMany = 10){

    pid_t pid;
    for(unsigned int i = 0 ; i < howMany; i++){
        if((pid = fork()) == 0){

            ConnectionProcess * cp = new ConnectionProcess(socketDescriptor, pipeToParent);
            cp->start();

            delete(cp);

        }else{
            children.push_back(pid);
        }
    }
}

string readInPipeMessage(int * pipeToParent){

    // Message Structure: { TYPE: DATA }

    const int BUFFERSIZE = 1;
    string totalMessage = "";

    while(1){
        char inbuf[BUFFERSIZE];
        read (pipeToParent[0], inbuf, BUFFERSIZE);

        string segment(inbuf);

        if(segment.compare("}") == 0){
            totalMessage += segment;
            return totalMessage;
        }
    }
}

void shutdownServer(int signo){
    continueRunning = false;

    close(pipeConnectionToParent[0]);
    close(pipeConnectionToParent[1]);

    for_each(children.begin(),children.end(), [](pid_t pid){
        kill(pid, SIGKILL);
    });

    children.clear();
}

int main() {

    int idleProcesses = -1;

    cout << "Setting Up SIGINT Listener" << endl;

 /*   struct sigaction act;
    act.sa_handler = shutdownServer;
    act.sa_flags = 0;
    if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
    {
        perror ("Failed to set SIGINT handler");
        exit(1);
    }
*/
    cout << "Creating Socket" << endl;

    //create a socket
    int socketDescriptor;
    if((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Can't create Socket");
        exit(1);
    }

    cout << "Editing Socket Options" << endl;

 /*   int arg = 1;
    if (setsockopt (socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1){
        perror("Setting Of Socket Option Failed");
        exit(1);
    }
*/
    cout << "Binding Socket" << endl;

    struct	sockaddr_in server;
    //bind the socket
    server.sin_family = AF_INET;
    server.sin_port = htons(4001);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socketDescriptor, (struct sockaddr *)&(server), sizeof(server)) == -1)
    {
        perror("Can't bind name to socket");
        exit(1);
    }else{
        cout << "Port Binding Complete" << endl;
    }

    //set socket to start listening
    listen(socketDescriptor, 10);

    cout << "Setting Up Pipe Communication" << endl;
    //setup 1 way pipe - child2parent

    if(pipe(pipeConnectionToParent) < 0){
        cout << "Pipe Establishment To Parent Failed" << endl;
        exit(1);
    }else{
        cout << "Pipe Establishment To Parent Successful" << endl;
    }

    cout << "PreCreating 10 ConnectionProcesses" << endl;
    //pre-build 10 processes - pass ConnectionProcess the socket
    createChildProcesses(socketDescriptor,pipeConnectionToParent, children, 2);
    idleProcesses = 2;

    cout << "Entering Pipe Loop" << endl;

    char inbuf[1];
    //while 1
    while(continueRunning){
        //wait on pipe for messages
        string message = readInPipeMessage(pipeConnectionToParent);
        cout << "Recieved Message: " << message << endl;

        // get message about new connection details -> store those details
            //store state that this process is in use
            //check if we have used half the processes
                //if half used create 10 more processes - pass ConnectionProcess the socket

        // get message about connection terminated and data summary -> store those details
            //store state that this process is idle
        break;
    }

    return 0;
}