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
#include <vector>

using namespace std;

struct clientMeta {
    int requestCount = 0;
    long totalData = 0;
    string address = "";
    int socketDescriptor;
    string handlingProcess;
    bool active = true;
};

class ConnectionProcess {

public:
    void start();
    ConnectionProcess(int socketDescriptor, int * pipeToParent);

private:
    int socketDescriptor;
    int * pipeToParent;

    int clients[FD_SETSIZE];
    int highestClientsIndex = -1;

    int highestFileDescriptor;

    vector<clientMeta> clientMetaList;

    struct  sockaddr_in client;

    const int BUFFLEN = 255;

    string * readInMessage(int socketDescriptor);
};


#endif //INC_8005_A2_SCALABLESERVERS_CONNECTIONPROCESS_H
