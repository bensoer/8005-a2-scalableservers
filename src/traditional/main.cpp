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
#include <fstream>

#include "ConnectionProcess.h"

using namespace std;

bool continueRunning = true;
int pipeConnectionToParent[2];
vector<pid_t> children;
const unsigned int INCR_NUM_OF_PROCESSES = 2;

/**
 * usage is a struct that represents a single connection between a single client. This is the main processes storage struct
 * that keeps record of each connection, the amount of data passed, the amount of packets sent, client information, and
 * state information as to whether the connection is still active
 */
struct usage {
    string clientIP;
    int totalPackets;
    long totalBytes;
    string handlingProcess;
    int socketDescriptor;
    bool active = true;
};

/**
 * clientData is a dynamic vector that stores a colleciton of all of the usage records during the servers execution
 */
vector<usage> clientData;
bool isChild = false;

/**
 * createChildProcess is a helper method that creates child processes and locks them into only running within the scope
 * of the start method of the ConnectionProcess object. This is to keep the code tidier in the main.cpp. If the start
 * method returns, the ConnectionProcess object is deleted and the process exits. Additionaly when a child is made the
 * isChild global variable is set to true. This is because the Ctrl+C event handler will have been copied over to the
 * children. To avoid confusion all children do not terminate themselves (see shutdownServer method) and instead wait
 * for a SIGTERM call from the parent. In the fork call, if the return is in the parent, the child pid is stored in the
 * children vector for reference later and program termination.
 * @param socketDescriptor:int - The socketDescriptor the child process will function with
 * @param pipeToParent: *int - The pipe to communicate with the parent with
 * @param children: *vector<pid_t> - The colleciton of children processes, to be added to by the parent when fork returns
 * @param howMAny: unsigned int - How many children processes to make
 */
void createChildProcesses(int socketDescriptor, int * pipeToParent, vector<pid_t> * children, unsigned int howMany){

    pid_t pid;
    for(unsigned int i = 0 ; i < howMany; i++){
        if((pid = fork()) == 0){

            isChild = true;
            ConnectionProcess * cp = new ConnectionProcess(socketDescriptor, pipeToParent);
            cp->start();

            delete(cp);

            exit(0);

        }else{
            children->push_back(pid);
        }
    }
}
/**
 * readInPipeMessage is a helper method that reads in messages from the pipeToParent pipe. Data send through the pipe
 * follows a very specific format and thus is read 1 byte at a time until the termination letter '}' is found. At this
 * point the message is assembled and returned
 * @param pipeToParent:*int - The pipe to be read from
 * @return string - The assembled message from the pipe
 */
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
/**
 * shutdownServer is a helper method that handles the shutdown of ther server. This method is triggered when Ctrl+C is
 * pressed. As this is the only way to shutdown the server, this method ensures all portions of the server are cleaned
 * up before self terminating the main process. Since both parent and child processes inherit this functionality, shutdownServer
 * ensures that all child processes to not self terminate. Instead it enforces that the parent process goes through its
 * list of children process and issues a SIGTERM to each of them. As part of the cleanup shutdownServer also closes all
 * pipe connections before self terminating the main process
 * @param signo:int - The signal number (not used)
 */
void shutdownServer(int signo){

    if(isChild){
        return;
    }

    cout << "Shutdown Sigterm Detected . Terminating Program" << endl;
    continueRunning = false;


    close(pipeConnectionToParent[0]);
    close(pipeConnectionToParent[1]);

    cout << "Killing Children" << endl;
    for_each(children.begin(),children.end(), [](pid_t pid){
        cout << "Killing " << pid << endl;
        kill(pid, SIGTERM);
    });

    cout << "Children Killed. Clearing Children List" << endl;

    children.clear();

    cout << "Self Terminating" << endl;
    exit(0);
}
/**
 * main is the main entrance point of the program. It sets up all portions of the server. Including setting up the Ctrl+C
 * handler, setting up the socket and pipes between children, prebuilding the child processes and then waiting on the pipe
 * for messages. The main task of the main method after setting all portions up is recording status connections so that
 * they can be written to file when the server is finaly terminated. It does this by using its helper methods to read
 * from the pipe and parse out the necessary information depending on what type of message it is. Records are then updated/added
 * and during new connections, additional checks for the ratio of processes to connections is checked
 */
int main() {

    int idleProcesses = -1;

    cout << "Main - Setting Up SIGINT Listener" << endl;

    struct sigaction act;
    act.sa_handler = shutdownServer;
    act.sa_flags = 0;
    if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
    {
        perror ("Failed to set SIGINT handler");
        exit(1);
    }

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

    cout << "Main - Binding Socket" << endl;

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
            // MESSAGE FORMAT {N:<address>:<handlingProcess>:<socketSessionDescriptor>}

            //create a record for this client
            unsigned long firstSegregation = message.find(':');
            unsigned long secondSegregation = message.find(':', firstSegregation + 1);
            unsigned long thirdSegregation = message.find(':', secondSegregation + 1);
            unsigned long endBracket = message.find('}', thirdSegregation + 1);
            string address = message.substr(firstSegregation + 1, (secondSegregation-firstSegregation) - 1 );
            string strProcess = message.substr(secondSegregation + 1, (thirdSegregation-secondSegregation) - 1);
            int socketSessionDescriptor = stoi(message.substr(thirdSegregation + 1, (endBracket-thirdSegregation) - 1));
            cout << "found address: >" << address << "<" << endl;
            cout << "found handling process: >" << strProcess << "<" << endl;
            cout << "found socketSessionDescriptor: >"<< socketSessionDescriptor << "<" << endl;

            usage record;
            record.clientIP = address;
            record.handlingProcess = strProcess;
            record.socketDescriptor = socketSessionDescriptor;
            clientData.push_back(record);


            //check if we have used half the processes
            //if the number of idleProcesses is less then half of the current connections. we need more
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

            // MESSAGE FORMAT: {T:<handlingProcess>:<requestCount>:<totalData>:<socketSessionDescriptor>}

            //store state that this process is idle
            idleProcesses = idleProcesses + 1;

            unsigned long firstSegregation = message.find(':');
            unsigned long secondSegregation = message.find(':', firstSegregation + 1);
            unsigned long thirdSegregation = message.find(':', secondSegregation + 1);
            unsigned long fourthSegregation = message.find(':', thirdSegregation + 1);
            unsigned long endBracket = message.find('}', secondSegregation + 1);

            string handlingProcess = message.substr(firstSegregation + 1, (secondSegregation-firstSegregation) - 1 );
            string totalRequests = message.substr(secondSegregation + 1, (thirdSegregation-secondSegregation) - 1);
            string totalBytes = message.substr(thirdSegregation + 1, (fourthSegregation-thirdSegregation) - 1 );
            int socketSessionDescriptor = stoi(message.substr(fourthSegregation + 1, (endBracket-fourthSegregation) - 1));

            for_each(clientData.begin(), clientData.end(), [handlingProcess,totalRequests,totalBytes, socketSessionDescriptor](usage client){
                //find the matching client
                if(client.handlingProcess.compare(handlingProcess)==0 && client.socketDescriptor == socketSessionDescriptor && client.active){

                    client.totalBytes = stoi(totalBytes);
                    client.totalPackets = stoi(totalRequests);
                    client.active = false;

                    //write the record to file
                    string reportMessage = "Connection Record -  ClientIP: " + client.clientIP
                                           + " SocketSessionDescriptor: " + to_string(client.socketDescriptor)
                                           + " HandlingProcess: " + client.handlingProcess + " TotalPackets: "
                                           + to_string(client.totalPackets) + " TotalBytes: "
                                           + to_string(client.totalBytes);
                    std::fstream fs;
                    fs.open ("./traditional-syslog.log", std::fstream::in | std::fstream::out | std::fstream::app);

                    fs << reportMessage << endl;

                    fs.close();

                }
            });


        }


    }

    return 0;
}