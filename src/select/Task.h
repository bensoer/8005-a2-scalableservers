//
// Created by bensoer on 04/02/16.
//

#ifndef INC_8005_A2_SCALABLESERVERS_TASK_H
#define INC_8005_A2_SCALABLESERVERS_TASK_H

enum TaskType { NewConnection, NewData };

class Task {

public:
    TaskType taskType;
    int socketDescriptor;
};


#endif //INC_8005_A2_SCALABLESERVERS_TASK_H
