//
// Created by ysh on 24-6-16.
//

#include <sys/wait.h>
#include <malloc.h>

#ifndef OS_C_BG_H
#define OS_C_BG_H

#endif //OS_C_BG_H


typedef struct BackgroundTask{
    int task_number;
    pid_t pid;
    struct BackgroundTask *next;
} BackgroundTask;

void add_completed_task(int task_number, pid_t pid);

void print_completed_tasks();
void sigchld_handler(int sig) ;
void setup_signal_handlers();
