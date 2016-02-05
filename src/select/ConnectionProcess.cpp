//
// Created by bensoer on 23/01/16.
//

#include <sys/socket.h>
#include <arpa/inet.h>
#include "ConnectionProcess.h"
#include <algorithm>

ConnectionProcess::ConnectionProcess(int socketDescriptor, int *pipeToParent) {
    this->socketDescriptor = socketDescriptor;
    this->pipeToParent = pipeToParent;
}

string * ConnectionProcess:: readInMessage(int socketDescriptor){

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


            //cout << "Found end of message. Now Accounting For It" << endl;
            for_each(this->clientMetaList.begin(), this->clientMetaList.end(), [socketDescriptor, totalBytes](clientMeta &client){
                if(client.socketDescriptor == socketDescriptor){
                    //cout << "Found Matching Socket Descriptor Record" << endl;
                    //cout << "Old Values: " << client.requestCount << ", " << client.totalData << endl;
                    client.requestCount++;
                    client.totalData += totalBytes;

                    //cout << "New Values: " << client.requestCount << ", " << client.totalData << endl;
                }
            });


            return totalMessage;
        }else{
            //cout << "Not a }" << endl;
            *totalMessage += segment;
        }
    }
}

void ConnectionProcess::start() {

    fd_set rset, allset;
    FD_ZERO(&allset);
    FD_SET(this->socketDescriptor, &allset);

    this->highestFileDescriptor = this->socketDescriptor;

    for(unsigned int i = 0; i < FD_SETSIZE; i++){
        this->clients[i] = -1; //our setting to mean not in use
    }


    //while 1
    while(1){
        //hang on accept of the socket
        cout << getpid() << " - Now Hanging On Select" << endl;

        rset = allset;
        int nready = select(this->highestFileDescriptor + 1, &rset, NULL, NULL,NULL);

        //if the socketDescriptor is set then we have a new connection
        if(FD_ISSET(this->socketDescriptor, &rset)){

            bool isRoom = false;
            //check if we have space to accept the new connections
            for(unsigned int i = 0 ; i < FD_SETSIZE; i++){
                if(this->clients[i] < 0){
                    isRoom = true;
                    break;
                }
            }

            //if there is not room we have to loop around and try again
            if(!isRoom){
                //TODO: Check handling is correct for this
                continue;
            }



            //accept the connection
            int socketSessionDescriptor;
            socklen_t client_len= sizeof(this->client);
            if((socketSessionDescriptor = accept(this->socketDescriptor, (struct sockaddr *)&client,&client_len)) == -1){
                cout << "ERROR - Can't Accept Client Connection Request" << endl;
                continue;
            }

            //get connection info. send through pipe back to main process
            string address = inet_ntoa(client.sin_addr);
            cout << "Connection Accepted On Server From Client: " << address << endl;

            // Pipe Message {N:<address>:<pid>}
            string message = "{N:" + address + ":" + to_string(getpid()) + "}";
            cout << getpid() << " - Sending Message Back: " << message << endl;
            write(this->pipeToParent[1], message.c_str(), message.length());

            cout << "Message Sent Back to Main Process" << endl;

            //save connection info for taking accounting information
            clientMeta newClient;
            newClient.socketDescriptor = socketSessionDescriptor;
            newClient.address = address;

            //inner scoping to avoid clashing. This should never fail or error
            {

                // Check if we have room to add the socket

                unsigned int i = 0;
                for(i = 0 ; i < FD_SETSIZE; i++){
                    if(this->clients[i] < 0){
                        //we have room, add it in the space
                        this->clients[i] = socketSessionDescriptor;

                        //keep track of how high we need to search through this array when checking for data
                        if(i > this->highestClientsIndex){
                            this->highestClientsIndex = i;
                        }

                        break;
                    }
                }

                /* -- NOTE EXIT HERE IS WHY CLION DOES NOT DETECT ENDLESS LOOP -- */
                //means there are no more spaces. We shouldn't accept this socket
                if(i >= FD_SETSIZE){
                    cout << getpid() << " There Are Too Many Sockets On The System. We Can Not Have More. Earlier Catch Failed. Terminating Process" << endl;
                    exit(1);
                }
            }


            //save the new descriptor for the now future session
            this->clientMetaList.push_back(newClient);
            cout << "Client List Size: " << this->clientMetaList.size() << endl;

            //add new socketSessionDescriptor to set
            FD_SET(socketSessionDescriptor, &allset);
            //check and set the new highest socket descriptor - need this for select
            if(this->highestFileDescriptor < socketSessionDescriptor){
                this->highestFileDescriptor = socketSessionDescriptor;
            }

            //since we read from 1 descriptor. decrement the amount apparently available. If we have no more, then
            //there is no point continueing and checking for data to read
            nready = nready - 1;
            if(nready <= 0){
                continue;
            }

        } //END OF FD_ISSET FOR LISTENER SOCKET

        //now since we made it here, lets check all of the socket descriptors for data
        for(unsigned int i = 0; i <= this->highestClientsIndex; i++){

            //check this isn't a dud or removed socket now
            if(this->clients[i] < 0){
                continue;
            }

            //if this socket descriptor is set in the read descriptors set
            if(FD_ISSET(this->clients[i], &rset)){

                //read its content
                string * message = this->readInMessage(this->clients[i]);

                //if this message is to terminate
                if(message == nullptr){
                    cout << "0 BYTES READ, ASSUMING TERMINATION?" << endl;
                    close(this->clients[i]);
                    FD_CLR(this->clients[i], &allset);

                    int currentClientSocketDescriptor = this->clients[i];
                    cout << "Client MEta List Length: " << this->clientMetaList.size();
                    for_each(this->clientMetaList.begin(), this->clientMetaList.end(), [currentClientSocketDescriptor, this](clientMeta client){
                        if(client.socketDescriptor == currentClientSocketDescriptor){

                            cout << "Found Matching Socket Record. To Send Termination Message For" << endl;

                            string terminationMessage = "{T:" + client.address + ":" + to_string(client.requestCount) + ":" + to_string(client.totalData) + "}";
                            cout << getpid() << " - Sending Termination Message: " << terminationMessage << endl;
                            write(this->pipeToParent[1], terminationMessage.c_str(), terminationMessage.length());
                        }
                    });

                    this->clients[i] = -1;
                }else{

                    //send back its content
                    cout << getpid() << " Recieved Message >" << (*message) << "<" << endl;
                    write(this->clients[i], message->c_str(), message->length());
                }

                //delete the message reserve
                delete(message);

                //if there are no more to read, then lets stop now
                nready = nready - 1;
                if(nready <= 0){
                    break;
                }



            }
        }
    }

}