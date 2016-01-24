#include <iostream>

using namespace std;

int main() {

    //create a socket
    //bind the socket


    //setup 1 way pipe - child2parent

    //pre-build 10 processes - pass ConnectionProcess the socket

    //while 1
        //wait on pipe for messages

            // get message about new connection details -> store those details
                //store state that this process is in use
                //check if we have used half the processes
                    //if half used create 10 more processes - pass ConnectionProcess the socket

            // get message about connection terminated and data summary -> store those details
                //store state that this process is idle





    return 0;
}