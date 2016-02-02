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
const unsigned int INCR_NUM_OF_PROCESSES = 2;

struct usage {
    string clientIP;
    int totalPackets;
    long totalBytes;
    string handlingProcess;
};

vector<usage> clientData;


void createChildProcesses(int socketDescriptor, int * pipeToParent, vector<pid_t> * children, unsigned int howMany){

    pid_t pid;
    for(unsigned int i = 0 ; i < howMany; i++){
        if((pid = fork()) == 0){

            ConnectionProcess * cp = new ConnectionProcess(socketDescriptor, pipeToParent);
            cp->start();

            delete(cp);

            exit(0);

        }else{
            children->push_back(pid);
        }
    }
}

string readInPipeMessage(int * pipeToParent){

    // Message Structure: { TYPE: DATA }

    const int BUFFERSIZE = 2;
    string totalMessage = "";

    //cout << "going to read from pipe now" << endl;

    while(1){
        char inbuf[BUFFERSIZE];
        read (pipeToParent[0], inbuf, BUFFERSIZE-1);

        //cout << "BUFFER CONTENT >" << inbuf << "<" << endl;
        inbuf[BUFFERSIZE-1] = '\0';

        string segment(inbuf);

        //cout << segment << endl;

        if(segment.compare("}") == 0){
            totalMessage += segment;
            return totalMessage;
        }else{
            //cout << "Not a }" << endl;
            totalMessage += segment;
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

    cout << "Main - Setting Up SIGINT Listener" << endl;

 /*   struct sigaction act;
    act.sa_handler = shutdownServer;
    act.sa_flags = 0;
    if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
    {
        perror ("Failed to set SIGINT handler");
        exit(1);
    }
*/
    cout << "Main - Creating Socket" << endl;

    //create a socket
    int socketDescriptor;
    if((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Can't create Socket");
        exit(1);
    }

    cout << "Main - Editing Socket Options" << endl;

    int arg = 1;
    if (setsockopt (socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1){
        perror("Setting Of Socket Option Failed");
        exit(1);
    }

    cout << "MAin - Binding Socket" << endl;

    struct	sockaddr_in server;
    //bind the socket
    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(4002);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socketDescriptor, (struct sockaddr *)&(server), sizeof(server)) == -1)
    {
        perror("Can't bind name to socket");
        exit(1);
    }else{
        cout << "Main - Port Binding Complete" << endl;
    }

    //set socket to start listening
    listen(socketDescriptor, 10);

    cout << "Setting Up Pipe Communication" << endl;
    //setup 1 way pipe - child2parent

    if(pipe(pipeConnectionToParent) < 0){
        cout << "Pipe Establishment To Parent Failed" << endl;
        exit(1);
    }else{
        cout << "Main - Pipe Establishment To Parent Successful" << endl;
    }

    cout << "PreCreating 10 ConnectionProcesses" << endl;
    //pre-build 10 processes - pass ConnectionProcess the socket
    createChildProcesses(socketDescriptor,pipeConnectionToParent, &children, INCR_NUM_OF_PROCESSES);
    cout << "PreCreate. There Are : " << children.size() << " children" << endl;
    idleProcesses = 2;

    cout << "Main - Entering Pipe Loop" << endl;

    char inbuf[1];
    //while 1
    while(continueRunning){
        //wait on pipe for messages
        string message = readInPipeMessage(pipeConnectionToParent);
        cout << "Main - Recieved Message: " << message << endl;

        // get message about new connection details -> store those details
        string firstTwoLetters = message.substr(0, 2);
        if(firstTwoLetters.compare("{N")==0){
            //store state that this process is in use
            idleProcesses = idleProcesses - 1;

            //{N:<address>:<pid>}

            //create a record for this client
            unsigned long firstSegregation = message.find(':');
            unsigned long secondSegregation = message.find(':', firstSegregation + 1);
            unsigned long endBracket = message.find('}', secondSegregation + 1);
            string address = message.substr(firstSegregation + 1, (secondSegregation-firstSegregation) - 1 );
            string strProcess = message.substr(secondSegregation + 1, (endBracket-secondSegregation) - 1);
            cout << "found address: >" << address << "<" << endl;
            cout << "found handling process: >" << strProcess << "<" << endl;

            usage record;
            record.clientIP = address;
            record.handlingProcess = strProcess;
            clientData.push_back(record);


            //check if we have used half the processes
            //if the number of processes is less then half of the total processes. we need more
            if(idleProcesses < (children.size()/2) ){
                cout << "Main - Process Count Is Too Low. Adding More Processes" << endl;
                //if half used create 10 more processes - pass ConnectionProcess the socket
                createChildProcesses(socketDescriptor, pipeConnectionToParent, &children, INCR_NUM_OF_PROCESSES);
                idleProcesses = idleProcesses + INCR_NUM_OF_PROCESSES;
            }else{
                cout << "Main - Process Count Is Fine. idleProcesses: " << idleProcesses << " childrenSize: " << children.size() << endl;
            }

        }

        // get message about connection terminated and data summary -> store those details
        if(firstTwoLetters.compare("{T")==0){

            //store state that this process is idle
            idleProcesses = idleProcesses + 1;

            unsigned long firstSegregation = message.find(':');
            unsigned long secondSegregation = message.find(':', firstSegregation + 1);
            unsigned long thirdSegregation = message.find(':', secondSegregation + 1);
            unsigned long endBracket = message.find('}', secondSegregation + 1);

            string address = message.substr(firstSegregation + 1, (secondSegregation-firstSegregation) - 1 );
            string totalRequests = message.substr(secondSegregation + 1, (thirdSegregation-secondSegregation) - 1);
            string totalBytes = message.substr(thirdSegregation + 1, (endBracket-thirdSegregation) - 1 );

            for_each(clientData.begin(), clientData.end(), [address,totalRequests,totalBytes](usage client){
                //find the matching client
                if(client.clientIP.compare(address)==0){

                    client.totalBytes = stoi(totalBytes);
                    client.totalPackets = stoi(totalRequests);

                }
            });


        }


    }

    return 0;
}