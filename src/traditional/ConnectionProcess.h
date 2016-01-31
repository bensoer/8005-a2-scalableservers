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

class ConnectionProcess {

public:
    void start();
    ConnectionProcess(int socketDescriptor, int * pipeToParent);

private:
    int socketDescriptor;
    int socketSessionDescriptor;
    int * pipeToParent;

    struct  sockaddr_in client;
};


#endif //INC_8005_A2_SCALABLESERVERS_CONNECTIONPROCESS_H
