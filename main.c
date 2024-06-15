#include "main.h"
#include "lsh_builtins.h"
#include "bg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>


extern char *builtin_str[];
extern int (*builtin_func[]) (char **);

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

void init_history(const char *history_file){
    stifle_history(100);
    using_history();
    read_history(history_file);
}

void save_history(const char *history_file){
    write_history(history_file);
}

void lsh_loop(const char *history_file) {
    char cwd[PATH_MAX];
    char *line;
    char **args;
    int status;

    setup_signal_handlers(); // 设置信号处理函数

    init_history(history_file);
    HIST_ENTRY *last_history_entry = history_get(history_length);
    char *last_command = (last_history_entry != NULL) ? last_history_entry->line : NULL;

    do {
        if(child_done > 0){
            print_completed_tasks();
            child_done = 0;
        }

        getcwd(cwd, sizeof(cwd)); // 获取当前工作目录
        char* prompt = malloc(PATH_MAX + 3);
        if (prompt != NULL) {
            snprintf(prompt, PATH_MAX + 3, "%s> ", cwd); // 将当前工作目录格式化到提示符中
        }
        line = readline(prompt);
        free(prompt);
        if (line != NULL && *line) { // 检查用户输入是否为空
            if (last_command == NULL || strcmp(line, last_command) != 0) {
                add_history(line);
                append_history(1, history_file); // 实时保存到历史文件，可有可无
                last_command = strdup(line); //更新最后一条命令
            }
        }

        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
        printf("\n");
    } while (status);
    save_history(history_file);

    free(last_command);
}

// 解析命令中的重定向
int parse_redirection(char **args, int *in_fd, int *out_fd){
    for (int i = 0; args[i] != NULL; i++){
        if (strcmp(args[i], ">") == 0){
            if (args[i + 1] == NULL){
                fprintf(stderr, "lsh: 未预期的记号 \"newline\" 附近有语法错误\n");
                return -1;
            }
            *out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (*out_fd == -1){
                perror("lsh");
                return -1;
            }
            args[i] = NULL;
            // 将后面的参数前移一位
            for (int j = i + 1; args[j] != NULL; j++){
                args[j] = args[j + 1];
            }
        } else if (strcmp(args[i], ">>") == 0){
            if (args[i + 1] == NULL){
                fprintf(stderr, "lsh: 未预期的记号 \"newline\" 附近有语法错误\n");
                return -1;
            }
            *out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (*out_fd == -1){
                perror("lsh");
                return -1;
            }
            args[i] = NULL;
            // 将后面的参数前移一位
            for (int j = i + 1; args[j] != NULL; j++){
                args[j] = args[j + 1];
            }
        } else if (strcmp(args[i], "<") == 0){
            if (args[i + 1] == NULL){
                fprintf(stderr, "lsh: 未预期的记号 \"newline\" 附近有语法错误\n");
                return -1;
            }
            *in_fd = open(args[i + 1], O_RDONLY);
            if (*in_fd == -1){
                perror("lsh");
                return -1;
            }
            args[i] = NULL;
            // 将后面的参数前移一位
            for (int j = i + 1; args[j] != NULL; j++){
                args[j] = args[j + 1];
            }
        }
    }
    return 0;
}

// 执行单个命令
int lsh_launch_single(char **args, int in_fd, int out_fd, bool is_background){
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0){
        // 子进程
        if (in_fd != 0){
            dup2(in_fd, 0);
            close(in_fd);
        }
        if (out_fd != 1){
            dup2(out_fd, 1);
            close(out_fd);
        }

        if (execvp(args[0], args) == -1){
            printf("'%s' 不是内部或外部命令，也不是可运行的程序\n"
                   "或批处理文件。\n", args[0]);
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0){
        // Fork出错
        printf("Fork Error");
    } else{
        // 父进程
        if (is_background){
            printf("[%d] %d\n", ++background_counter, pid);
        } else{
            waitpid(pid, &status, WUNTRACED); // 等待子进程结束
        }
    }
    return 1;
}

// 处理管道命令
int lsh_launch_pipeline(char **commands, int num_commands, bool is_background){
    int i, in_fd =0, fd[2];

    for (i = 0; i < num_commands - 1; i++){
        pipe(fd);
        lsh_launch_single(commands[i], in_fd, fd[1], is_background);
        close(fd[1]);
        in_fd = fd[0];
    }

    lsh_launch_single(commands[i], in_fd, 1, is_background);

    return 1;
}


// 外置命令
int lsh_launch(char **args, bool is_background) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // 子进程
        if (execvp(args[0], args) == -1) {
            printf("'%s' 不是内部或外部命令，也不是可运行的程序\n"
                   "或批处理文件。\n", args[0]);
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Fork 出错
        printf("Fork Error");
    } else {
        // 父进程
        if (is_background) {
            printf("[%d] %d\n", ++background_counter, pid); // 打印后台任务的序号和进程号
        } else {
            waitpid(pid, &status, WUNTRACED); // 等待子进程结束
        }
    }
    return 1;
}

// 选择执行命令
int lsh_execute(char **args) {
    bool is_background = false;
    int in_fd = 0, out_fd = 1;
    char *pipe_commands[128][128];
    int num_commands = 0, cmd_index = 0, arg_index = 0;

    //检查是否有后台运行标记
    for(int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "&") == 0) {
            args[i] = NULL; // 将"&"从参数中删除
            is_background = true;
            if(args[0] == NULL) {
                printf("未预期的记号 '&' 附近有语法错误\n");
                return 1;
            }
            break;
        }
    }

    if (args[0] == NULL) {
        // 用户输入了一个空命令
        return 1;
    }

    // 解析管道
    for (int i = 0; args[i] != NULL; i++){
        if (strcmp(args[i], "|") == 0){
            pipe_commands[num_commands][cmd_index] = NULL;
            num_commands++;
            cmd_index = 0;
        } else{
            pipe_commands[num_commands][cmd_index] = args[i];
        }
    }
    pipe_commands[num_commands][cmd_index] = NULL;
    num_commands++;

    // 解析重定向
    if (parse_redirection(args, &in_fd, &out_fd) == -1){
        return 1;
    }

    // 检查是否为内置命令
    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            if(is_background){
                pid_t pid = fork();
                if(pid == 0){
                    // 子进程执行内置命令
                    exit((*builtin_func[i])(args));
                } else if (pid < 0){
                    perror("fork");
                    return 1;
                } else {
                    // 父进程
                    printf("[%d] %d\n", ++background_counter, pid); // 打印后台任务的序号和进程号
                    return 1;
                }
            } else{
                return (*builtin_func[i])(args);
            }
        }
    }

    // 执行命令
    if (num_commands > 1){
        return lsh_launch_pipeline(pipe_commands, num_commands, is_background);
    } else{
        return lsh_launch_single(args, in_fd, out_fd, is_background);
    }

//    return lsh_launch(args, is_background);
}

int main() {
    char* history_file = ".lsh_history";
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), history_file);
    // 运行命令循环
    lsh_loop(history_path);

    return EXIT_SUCCESS;
}
