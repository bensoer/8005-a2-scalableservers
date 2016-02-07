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


string * ConnectionProcess:: readInMessage(int socketDescriptor, clientMeta * clientInfo){

    // Message Structure: { <textfromclient> }

    const int BUFFERSIZE = 2;
    string * totalMessage = new string("");
    long totalBytes = 0;

    //cout << "going to read from pipe now" << endl;

    while(1){
        char inbuf[BUFFERSIZE];
        long bytesRead = read (socketDescriptor, inbuf, BUFFERSIZE-1);

        totalBytes += bytesRead;

        if(bytesRead == 0){
            cout << "readMessage is Assuming The Client Has Terminated. Returning Null String" << endl;
            return nullptr ;
        }

        //cout << "BUFFER CONTENT >" << inbuf << "<" << endl;
        inbuf[BUFFERSIZE-1] = '\0';

        string segment(inbuf);

        //cout << segment << endl;

        if(segment.compare("}") == 0){
            *totalMessage += segment;

            clientInfo->requestCount++;
            clientInfo->totalData += totalBytes;


            return totalMessage;
        }else{
            //cout << "Not a }" << endl;
            *totalMessage += segment;
        }
    }
}

void ConnectionProcess::start() {



    //hang on accept of the socket
    cout << getpid() << " - Now Hanging On Accept" << endl;

    //while 1
    while(1){

        socklen_t client_len= sizeof(this->client);
        if((this->socketSessionDescriptor = accept(this->socketDescriptor, (struct sockaddr *)&client,&client_len)) == -1){
            cout << getpid << " ERROR - Can't Accept Client Connection Request" << endl;
            continue;
        }

        clientMeta newClient;
        newClient.handlingProcess = to_string(getpid());
        newClient.socketSessionDescriptor = this->socketDescriptor;


        //get connection info. send through pipe back to main process
        string address = inet_ntoa(client.sin_addr);
        cout << getpid() << " Connection Accepted On Server From Client: " << address << endl;

        // MESSAGE FORMAT {N:<address>:<handlingProcess>:<socketSessionDescriptor>}
        string message = "{N:" + address + ":" + newClient.handlingProcess + ":" + to_string(newClient.socketSessionDescriptor) + "}";
        cout << getpid() << " - Sending Message Back: " << message << endl;
        write(this->pipeToParent[1], message.c_str(), message.length());

        cout << getpid() << " Message Sent Back to Main Process" << endl;

        //while 1
        while(1){
            //hang on read in data from the socket
            //if EOF or termination, BREAK while
            string * message = readInMessage(this->socketSessionDescriptor, &newClient);

            if(message == nullptr){
                cout << getpid() << " 0 BYTES READ, ASSUMING TERMINATION?" << endl;
                break;
            }

            //cout << "Sending It Back" << endl;
            //echo the data back
            send (this->socketSessionDescriptor, message->c_str(), message->length() , 0);

            //cout << "Bout To Loop Around" << endl;
            delete(message);

        }

        //get full connection statistics on how much data was sent - send back through pipe to main process
        // MESSAGE FORMAT: {T:<handlingProcess>:<requestCount>:<totalData>:<socketSessionDescriptor>}
        string terminationMessage = "{T:" + newClient.handlingProcess + ":" + to_string(newClient.requestCount) + ":"
                                    + to_string(newClient.totalData) + ":" + to_string(newClient.socketSessionDescriptor)
                                    + "}";
        cout << getpid() << " - Sending Termination Message: " << terminationMessage << endl;
        write(this->pipeToParent[1], terminationMessage.c_str(), terminationMessage.length());

        newClient.active = false;

        //close the socket ?

    }

}