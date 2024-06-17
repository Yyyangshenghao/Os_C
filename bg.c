//
// Created by ysh on 24-6-16.
//
#include "main.h"
#include "lsh_builtins.h"
#include "bg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>


volatile sig_atomic_t background_counter = 0; // 记录后台任务的序号
volatile sig_atomic_t child_done = 0; //记录有多少个子进程完成
BackgroundTask *completed_tasks = NULL;

void add_completed_task(int task_number, pid_t pid){
    BackgroundTask *task = (BackgroundTask *)malloc(sizeof(BackgroundTask));
    task->task_number = task_number;
    task->pid = pid;
    task->next = completed_tasks;
    completed_tasks = task;
}

void print_completed_tasks(){
    BackgroundTask *task = completed_tasks;
    while(task != NULL){
        printf("[%d]+ 已完成 %d\n", task->task_number, task->pid);
        BackgroundTask *temp = task;
        task = task->next;
        free(temp);
    }
    completed_tasks = NULL;
}

void sigchld_handler(int sig) {
    int saved_errno = errno;
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        background_counter--; // 后台任务完成，减少计数器
        add_completed_task(background_counter + 1, pid);
        child_done++;
    }
    errno = saved_errno;
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}
