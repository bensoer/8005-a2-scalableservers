//
// Created by bensoer on 23/01/16.
//

#ifndef INC_8005_A2_SCALABLESERVERS_CONNECTIONPROCESS_H
#define INC_8005_A2_SCALABLESERVERS_CONNECTIONPROCESS_H

#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>

using namespace std;

struct clientMeta {
    int requestCount = 0;
    long totalData = 0;
    string handlingProcess;
    int socketSessionDescriptor;
    bool active = true;
};


class ConnectionProcess {

public:
    void start();
    ConnectionProcess(int socketDescriptor, int * pipeToParent);

private:
    int socketDescriptor;
    int socketSessionDescriptor;
    int * pipeToParent;

    struct  sockaddr_in client;

    string * readInMessage(int socketDescriptor, clientMeta * clientInfo);


};


#endif //INC_8005_A2_SCALABLESERVERS_CONNECTIONPROCESS_H
