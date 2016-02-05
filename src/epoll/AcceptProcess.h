//
// Created by bensoer on 05/02/16.
//

#ifndef INC_8005_A2_SCALABLESERVERS_ACCEPTPROCESS_H
#define INC_8005_A2_SCALABLESERVERS_ACCEPTPROCESS_H

// need to handle socketDescriptor epollDescriptor and pipeToParent
class AcceptProcess {

public:
    AcceptProcess(int epollDescriptor, int * pipeToMain);

    void start(int socketDescriptor);

private:
    int epollDescriptor;
    int * pipeToMain;

    struct  sockaddr_in client;
};


#endif //INC_8005_A2_SCALABLESERVERS_ACCEPTPROCESS_H
