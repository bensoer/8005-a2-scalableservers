//
// Created by bensoer on 23/01/16.
//

#include <sys/socket.h>
#include <arpa/inet.h>
#include "ConnectionProcess.h"
#include <algorithm>
#include <sys/epoll.h>
#include <assert.h>
#include <fcntl.h>


string * ConnectionProcess:: readInMessage(int incomingMessageDescriptor){

    // Message Structure: { <textfromclient> }

    const int BUFFERSIZE = 2;
    string * totalMessage = new string("");
    long totalBytes = 0;

    //cout << "going to read from pipe now" << endl;

    while(1){
        char inbuf[BUFFERSIZE];
        long bytesRead = read (incomingMessageDescriptor, inbuf, BUFFERSIZE-1);

        totalBytes += bytesRead;

        if(bytesRead == 0){
            cout << getpid() << " - readMessage is Assuming The Client Has Terminated. Returning Null String" << endl;
            return nullptr ;
        }

        //cout << "BUFFER CONTENT >" << inbuf << "<" << endl;
        inbuf[BUFFERSIZE-1] = '\0';

        string segment(inbuf);

        //cout << segment << endl;

        if(segment.compare("}") == 0){
            *totalMessage += segment;

            //account message into records
            for_each(this->clientMetaList.begin(), this->clientMetaList.end(),
                 [incomingMessageDescriptor, totalBytes](clientMeta &client){
                    if(client.socketDescriptor == incomingMessageDescriptor && client.active){
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


    struct epoll_event events [this->EPOLL_QUEUE_LENGTH];
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
    event.data.fd = this->socketDescriptor;
    if(epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, this->socketDescriptor, &event) == -1){
        cout << getpid() << " Failed To Add Socket Descriptor To The Epoll Event Loop" << endl;
        exit(1);
    }else{
        cout << getpid() << " - Successfully Added Socket Descriptor To The Epoll Event Loop" << endl;
    }


    cout << getpid() << " - Now Hanging On Epoll" << endl;

    //while 1
    while(1) {
        //hang on the epoll_wait

        int num_fds = epoll_wait(epollDescriptor, events, this->EPOLL_QUEUE_LENGTH, -1);
        if (num_fds < 0) {
            cout << getpid() << " - There Was An Error In Epoll Wait" << endl;
            exit(1);
        }

        for (unsigned int i = 0; i < num_fds; i++) {

            //check first for errors
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                cout << getpid() << " There Was An Error In An Event From Epoll. Closing File Descriptor" << endl;


                //send back a termination message to main
                //send accounting information back to main process
                int currentClientSocketDescriptor = events[i].data.fd;
                for_each(this->clientMetaList.begin(), this->clientMetaList.end(),
                         [currentClientSocketDescriptor, this](clientMeta client) {
                             if (client.socketDescriptor == currentClientSocketDescriptor && client.active) {

                                 cout << "Found Matching Socket Record. To Send Termination Message For" << endl;

                                 // MESSAGE FORMAT: {T:<handlingProcess>:<requestCount>:<totalData>:<socketSessionDescriptor>}

                                 string terminationMessage =
                                         "{T:" + client.handlingProcess + ":" + to_string(client.requestCount) + ":" +
                                         to_string(client.totalData) + ":" + to_string(client.socketDescriptor) + "}";
                                 cout << getpid() << " - Sending Termination Message: " << terminationMessage << endl;
                                 write(this->pipeToParent[1], terminationMessage.c_str(), terminationMessage.length());

                                 client.active = false;
                             }
                         });

                close(events[i].data.fd);

                continue;
            }
            if (!(events[i].events & EPOLLIN)) {
                cout << getpid() <<
                " Critical Error. This Event Has Nothing To Read In and Has No Errors. Why Is It Here ?" << endl;
                cout << getpid() << " Now Exploding" << endl;

                assert (events[i].events & EPOLLIN);
                exit(1);
            }


            //check if recieving new connections request
            if (events[i].data.fd == this->socketDescriptor) {

                //accept the connection
                int socketSessionDescriptor;
                socklen_t client_len = sizeof(this->client);
                if ((socketSessionDescriptor = accept(this->socketDescriptor, (struct sockaddr *) &client,
                                                      &client_len)) == -1) {
                    perror("ERROR - Can't Accept Client Connection Request ");
                    continue;
                }

                cout << getpid() << " SocketSessionDescriptor: " << socketSessionDescriptor << endl;

                //send record information back to the main process
                string address = inet_ntoa(client.sin_addr);
                cout << getpid() << "Connection Accepted On Server From Client: " << address << endl;
                // MESSAGE FORMAT {N:<address>:<pid>:<socketSessionDescriptor>}
                string message = "{N:" + address + ":" + to_string(getpid()) + ":" + to_string(socketSessionDescriptor) + "}";
                cout << getpid() << " - Sending Message Back: " << message << endl;
                write(this->pipeToParent[1], message.c_str(), message.length());

                cout << "Message Sent Back to Main Process" << endl;

                //save connection info for taking accounting information
                clientMeta newClient;
                newClient.socketDescriptor = socketSessionDescriptor;
                newClient.address = address;
                newClient.handlingProcess = to_string(getpid());

                //save the new descriptor for the now future session
                this->clientMetaList.push_back(newClient);
                cout << "Client List Size: " << this->clientMetaList.size() << endl;

                cout << getpid() << " Now Making New Connection Non Blocking" << endl;
                // Make the fd_new non-blocking
                if (fcntl(socketSessionDescriptor, F_SETFL, O_NONBLOCK | fcntl(socketSessionDescriptor, F_GETFL, 0)) ==
                    -1) {
                    cout << getpid() << " Failed To MAke New Connection Non-Blocking. Aborting" << endl;
                    exit(1);
                }

                event.data.fd = socketSessionDescriptor;
                if (epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, socketSessionDescriptor, &event) == -1) {
                    cout << "Failed To Add Socket Descriptor To The Epoll Event Loop" << endl;
                    exit(1);
                } else {
                    cout << getpid() << " Successfully Added Socket Descriptor To The Epoll Event Loop" << endl;
                }

            //if its not a new connection then check it for data
            }else{
                //check for data to be read
                string *message = this->readInMessage(events[i].data.fd);

                //if nullptr means the client disconnected
                if (message == nullptr) {
                    cout << "Connection Process (" << getpid() << ") 0 BYTES READ, ASSUMING TERMINATION?" << endl;

                    //send accounting information back to main process
                    int currentClientSocketDescriptor = events[i].data.fd;
                    for_each(this->clientMetaList.begin(), this->clientMetaList.end(),
                             [currentClientSocketDescriptor, this](clientMeta client) {
                                 if (client.socketDescriptor == currentClientSocketDescriptor && client.active) {

                                     cout << getpid() << " Found Matching Socket Record. To Send Termination Message For" << endl;

                                     // MESSAGE FORMAT: {T:<handlingProcess>:<requestCount>:<totalData>:<socketSessionDescriptor>}

                                     string terminationMessage =
                                             "{T:" + client.handlingProcess + ":" + to_string(client.requestCount) + ":" +
                                             to_string(client.totalData) + ":" + to_string(client.socketDescriptor) + "}";
                                     cout << getpid() << " - Sending Termination Message: " << terminationMessage << endl;
                                     write(this->pipeToParent[1], terminationMessage.c_str(), terminationMessage.length());

                                     client.active = false;
                                 }
                             });

                    //then terminate and close the socket
                    close(events[i].data.fd);

                } else {
                    //send back its content
                    //cout << getpid() << " Recieved Message >" << (*message) << "<" << endl;
                    write(events[i].data.fd, message->c_str(), message->length());
                }

                //delete the dynamic message when were done with it
                delete(message);
            }




        }

    }

}