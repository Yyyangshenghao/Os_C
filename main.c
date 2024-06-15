#include "main.h"
#include "lsh_builtins.h"
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

#define PATH_MAX 1024
extern char *builtin_str[];
extern int (*builtin_func[]) (char **);

volatile sig_atomic_t background_counter = 0; // 记录后台任务的序号

void sigchld_handler(int sig) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        background_counter--; // 后台任务完成，减少计数器
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
    stifle_history(1000);
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

    return lsh_launch(args, is_background);
}

int main() {
    char* history_file = ".lsh_history";
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), history_file);
    // 运行命令循环
    lsh_loop(history_path);

    return EXIT_SUCCESS;
}
