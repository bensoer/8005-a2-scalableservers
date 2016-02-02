//
// Created by bensoer on 23/01/16.
//

#include <sys/socket.h>
#include <arpa/inet.h>
#include "ConnectionProcess.h"

ConnectionProcess::ConnectionProcess(int socketDescriptor, int *pipeToParent) {
    this->socketDescriptor = socketDescriptor;
    this->pipeToParent = pipeToParent;
}

void ConnectionProcess::start() {

    struct clientMeta {
        int requestCount = 0;
        long totalData = 0;
    };


    //while 1
    while(1){
        //hang on accept of the socket
        cout << getpid() << " - Now Hanging On Accept" << endl;
        socklen_t client_len= sizeof(this->client);
        if((this->socketSessionDescriptor = accept(this->socketDescriptor, (struct sockaddr *)&client,&client_len)) == -1){
            cout << "ERROR - Can't Accept Client Connection Request" << endl;
            continue;
        }



        clientMeta newClient;

        //get connection info. send through pipe back to main process
        string address = inet_ntoa(client.sin_addr);
        cout << "Connection Accepted On Server From Client: " << address << endl;

        // Pipe Message {N:<address>:<pid>}
        string message = "{N:" + address + ":" + to_string(getpid()) + "}";
        cout << getpid() << " - Sending Message Back: " << message << endl;
        write(this->pipeToParent[1], message.c_str(), message.length());

        cout << "Message Sent Back to Main Process" << endl;

        //while 1
        while(1){
            //hang on read in data from the socket
            //if EOF or termination, BREAK while
            string message = "HELLO EVERYBODY";
            const int BUFFERLEN = message.length()+1;
            char recvBuffer[BUFFERLEN];
            //recvBuffer[0] = '\0';
            long brcvd = recv(this->socketSessionDescriptor, recvBuffer, BUFFERLEN, 0);
            //recvBuffer[BUFFERLEN - 1] = '\0';
            cout << getpid() << " Connection - Recieved A Message: " << recvBuffer << endl;

            if(brcvd == 0){
                cout << "0 BYTES READ. ASSUMING TERMINATION?" << endl;
                break;
            }

            cout << "Doing Accounting" << endl;
            //get and store statistics of connection so far
            newClient.requestCount++;
            newClient.totalData += brcvd;

            cout << "Sending It Back" << endl;
            //echo the data back
            send (this->socketSessionDescriptor, recvBuffer, sizeof(recvBuffer)/sizeof(char) , 0);

            cout << "Bout To Loop Around" << endl;

        }

        //get full connection statistics on how much data was sent - send back through pipe to main process

        //Pipe Message {T:<address>:<requestCount>:<totalData>}

        string terminationMessage = "{T:" + address + ":" + to_string(newClient.requestCount) + ":" + to_string(newClient.totalData) + "}";
        cout << getpid() << " - Sending Termination Message: " << terminationMessage << endl;
        write(this->pipeToParent[1], terminationMessage.c_str(), terminationMessage.length());

        //close the socket ?

    }

}