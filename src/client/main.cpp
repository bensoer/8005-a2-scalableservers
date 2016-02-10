#include <iostream>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "argparcer.h"
#include <sys/epoll.h>
#include <assert.h>
#include <sys/time.h>
#include <vector>
#include <fstream>
#include <algorithm>

using namespace std;

bool continueRunning = true;
const unsigned int EPOLL_QUEUE_LENGTH = 10;

/*
 * request is a struct used to represent a request made from the client to the server. It stores the sendTime, recieveTime
 * and time difference between the two times
 */
struct request {
    long sendTime;
    long recieveTime;
    long deltaTime;
};
/**
 * connection is a struct that is used to represent a connection from a client to a server. It stores the request
 * structures that it has made to the server aswell as a record of the total amount of data it has sent
 */
struct connection {
    vector<request> requests;
    long totalDataSent = 0;
};

connection transactions;

/**
 * connectToServer is a helper method that establishes a connection to the passed in host on the passed in port using
 * the passed in socketDescriptor.
 * @param host:String - The host to connect to. This will be DNS resolved to its IP
 * @param port:int - The port number of the host to connect to
 * @param socketDescriptor:int - The socket descriptor to make the connnection through
 */
void connectToServer(string host, int port, int socketDescriptor){
    struct hostent	*hp;

    if ((hp = gethostbyname(host.c_str())) == NULL)
    {
        fprintf(stderr, "Unknown server address\n");
        exit(1);
    }else{
        cout << "Main - Hostname Resolved" << endl;
    }

    struct	sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    bcopy(hp->h_addr, (char *)&(server.sin_addr), hp->h_length);

    // Connecting to the server
    if (connect(socketDescriptor, (struct sockaddr *)&(server), sizeof(server)) == -1)
    {
        fprintf(stderr, "Can't connect to server\n");
        perror("connect");
        exit(1);
    }else{
        cout << "MAin - Connection Established" << endl;
    }
}

/**
 * gen_random is a helper method to generate random text messages when sending random messages to the server. The method
 * will fill the passed in char pointer to the length passed in
 * @param s:char * - The string to be filled with random letters as a message
 * @param len:int - The length of the string to be filled with random letters as a message
 */
void gen_random(char *s, const int len) {
    static const char alphanum[] =
            "0123456789"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}
/**
 * shutdownClient is a helper method that enabled the client to shutdown appropriatly when Ctrl+C is called. Since this
 * is the only way to shutdown the client, when this method is triggered it will write all of the gathered record results
 * to file so that it can be further examined. It then self terminates the client
 * @param signo:int - The signal number (not used)
 */
void shutdownClient(int signo){
    continueRunning = false;


    cout << " ..  Writing Client Records To File .. This May Take A Second .." << endl;

    double totalTime = 0;

    std::fstream fs;
    fs.open ("./client-syslog.log", std::fstream::in | std::fstream::out | std::fstream::app);

    for_each(transactions.requests.begin(), transactions.requests.end(), [&fs, &totalTime](request record){
        fs << "Transaction Record - SendTime: " << record.sendTime << " microsec ReceiveTime: "
            << record.recieveTime << " microsec Delta: " << record.deltaTime << " microsc" << endl;

        totalTime += record.deltaTime;
    });

    fs << "Total Data Transfered: " << transactions.totalDataSent << " bytes" << endl;

    double numOfRequests = transactions.requests.size();
    double averageTime = totalTime/numOfRequests;

    fs << "Average RTT: " << averageTime << endl;

    fs.close();

}
/**
 * the main entrance point of the client. It first grabs a number of optional parameters to determine what to send. It then
 * sets up handlers for Ctrl+c, followed by setting up the socket, connecting to the server, setting up epoll to listen
 * for responses, and then cycling through sending the message, listening for a response, and sending the message again
 * upon reciept
 * @param argc:int - the number of parameters in the *argv[] parameter
 * @param argv:char*[] - An array of pointers to chars representing the passed in parameters set at initialization of
 * the program
 * @return int - Value as to whether the program has executed successfully. 0 for success 1 for failure
 */
int main(int argc, char * argv[]) {

    ArgParcer parcer;
    string messageToSend = parcer.GetTagData("-s",argv, argc);
    int randomStrings = parcer.GetTagVal("-r", argv, argc);
    bool sendRandom = false;

    if(messageToSend.compare("-1")==0){
        messageToSend = "HELLO EVERYBODY";
    }

    if(randomStrings == 1){
        sendRandom = true;
    }


    //register sig terminate
    struct sigaction act;
    act.sa_handler = shutdownClient;
    act.sa_flags = 0;
    if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
    {
        perror ("Failed to set SIGINT handler");
        exit(1);
    }

    //create socket
    int socketDescriptor;
    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Can't create a socket");
        exit(1);
    }else{
        cout << "Main - Socket Created" << endl;
    }

    //connect to server
    connectToServer("localhost", 4002, socketDescriptor);

    struct epoll_event events [EPOLL_QUEUE_LENGTH];
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
    event.data.fd = socketDescriptor;
    if(epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, socketDescriptor, &event) == -1){
        cout << getpid() << " Failed To Add Socket Descriptor To The Epoll Event Loop" << endl;
        exit(1);
    }else{
        cout << getpid() << " - Successfully Added Socket Descriptor To The Epoll Event Loop" << endl;
    }


    //while 1
    while(continueRunning){

        string message;

        if(sendRandom){
            char chrMessage[25];
            gen_random(chrMessage, 25);
            message = "{" + to_string(*chrMessage) + "}";

        }else{
            message = "{" + messageToSend + "}";
        }

        struct timeval initiationTime;
        gettimeofday(&initiationTime,NULL);

        request record;
        record.sendTime = initiationTime.tv_usec;
        transactions.totalDataSent += sizeof(message.c_str());

        //send message
        int length = message.size();
        send (socketDescriptor, message.c_str(), length, 0);
        cout << "Main - Sent Message" << endl;


        //wait for reply

        int num_fds = epoll_wait(epollDescriptor, events, EPOLL_QUEUE_LENGTH, -1);
        if (num_fds < 0) {
            cout << getpid() << " - There Was An Error In Epoll Wait" << endl;
            exit(1);
        }

        //sift through reply
        for (unsigned int i = 0; i < num_fds; i++) {
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                cout << getpid() << " There Was An Error In An Event From Epoll. Closing File Descriptor" << endl;
                close(events[i].data.fd);
                exit(1);
            }

            if (!(events[i].events & EPOLLIN)) {
                cout << getpid() <<
                " Critical Error. This Event Has Nothing To Read In and Has No Errors. Why Is It Here ?" << endl;
                cout << getpid() << " Now Exploding" << endl;

                assert (events[i].events & EPOLLIN);
                exit(1);
            }

            char recvBuffer[length];
            long bytesReceived = recv(events[i].data.fd, recvBuffer, length+1, 0);
            cout << "Main - Got Message Back!" << endl;
            cout << recvBuffer << endl;

            struct timeval recieveTime;
            gettimeofday(&recieveTime,NULL);


            //write down record information about reply
            record.recieveTime = recieveTime.tv_usec;
            record.deltaTime = record.recieveTime - record.sendTime;

            transactions.requests.push_back(record);




        }





    }

    close(socketDescriptor);


    return 0;
}