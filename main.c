#include "main.h"
#include "lsh_builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#define PATH_MAX 1024
extern char *builtin_str[];
extern int (*builtin_func[]) (char **);

void lsh_loop(const char *history_file){
    char cwd[PATH_MAX];
    char *line;
    char **args;
    int status;

    stifle_history(1000); // 限制的最大条目数为1000条
    using_history();
    read_history(history_file);
    HIST_ENTRY *last_history_entry = history_get(history_length);
    char *last_command = (last_history_entry != NULL) ? last_history_entry->line : NULL;

    do {
        getcwd(cwd, sizeof(cwd)); // 获取当前工作目录
        char* prompt = malloc(PATH_MAX + 3);
        if (prompt != NULL) {
            snprintf(prompt, PATH_MAX + 3, "%s> ", cwd); // 将当前工作目录格式化到提示符中
        }
        line = readline(prompt);
        if(line != NULL && (last_command == NULL || strcmp(line, last_command) != 0)){
            add_history(line);
            append_history(1, history_file); // 实时保存到历史文件，可有可无
            last_command = strdup(line); //更新最后一条命令
        }

        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
        free(prompt);
        printf("\n");
    } while (status);
    free(last_command);
}

int lsh_launch(char **args, bool is_background) {
    if (!is_background) {
        // 前台处理
        pid_t pid;
        int status;

        pid = fork();
        if (pid == 0) {
            // 子进程
            if (execvp(args[0], args) == -1) {
                printf( "'%s' 不是内部或外部命令，也不是可运行的程序\n"
                        "或批处理文件。\n", args[0]);
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            // Fork 出错
            printf("Fork Error");
        } else {
            // 父进程
            waitpid(pid, &status, WUNTRACED); // 等待子进程结束
        }
        return 1;
    } else {
        // 后台处理
        pid_t pid;

        pid = fork();
        if (pid == 0) {
            // 子进程
            for (int i = 0; i < lsh_num_builtins(); i++) {
                if (strcmp(args[0], builtin_str[i]) == 0) {
                    if ((*builtin_func[i])(args) == -1) {
                        printf("内置命令 '%s' 未找到\n", args[0]);
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
            }

            if (execvp(args[0], args) == -1) {
                printf( "'%s' 不是内部或外部命令，也不是可运行的程序\n"
                        "或批处理文件。\n", args[0]);
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            // Fork 出错
            printf("Fork Error");
        } else {
            // 父进程
            printf("Background process with PID: %d\n", pid);
            return 1;
        }
    }
}



int lsh_execute(char **args) {

    //检查是否有后台运行标记
     for(int i = 0; args[i] != NULL; i++) {
         if (strcmp(args[i], "&") == 0) {
             args[i] = NULL; // 将"&"从参数中删除
             if(args[0] == NULL) {
                 printf("未预期的记号 '&' 附近有语法错误\n");
                 return 1;
             }
             return lsh_launch(args, true);
         }
     }

    if (args[0] == NULL) {
        // 用户输入了一个空命令
        return 1;
    }

    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args, false);
}

int main()
{
    char* history_file = ".lsh_history";
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), history_file);
    // 运行命令循环
    lsh_loop(history_path);

    write_history(history_path);
    return EXIT_SUCCESS;
}

