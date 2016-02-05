//
// Created by bensoer on 05/02/16.
//

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "AcceptProcess.h"
#include "ConnectionProcess.h"

AcceptProcess::AcceptProcess(int epollDescriptor, int *pipeToMain) {
    this->epollDescriptor = epollDescriptor;
    this->pipeToMain = pipeToMain;
}

void AcceptProcess::start(int socketDescriptor) {


    //accept the connection
    int socketSessionDescriptor;
    socklen_t client_len = sizeof(this->client);
    if ((socketSessionDescriptor = accept(socketDescriptor, (struct sockaddr *) &client,
                                          &client_len)) == -1) {
        perror("ERROR - Can't Accept Client Connection Request ");
        return;
    }

    //send record information back to the main process
    string address = inet_ntoa(client.sin_addr);
    cout << getpid() << "Connection Accepted On Server From Client: " << address << endl;
    // Pipe Message {N:<address>:<pid>}
    string message = "{N:" + address + ":" + to_string(getpid()) + "}";
    cout << getpid() << " - Sending Message Back: " << message << endl;
    write(this->pipeToMain[1], message.c_str(), message.length());

    cout << "Message Sent Back to Main Process" << endl;

    //save connection info for taking accounting information
    clientMeta newClient;
    newClient.socketDescriptor = socketSessionDescriptor;
    newClient.address = address;

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
}