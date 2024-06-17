#include "main.h"
#include "lsh_builtins.h"
#include "bg.h"
#include "history.h"
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

#define PIPE_BUF_SIZE 128

extern char *builtin_str[];
extern int (*builtin_func[]) (char **);
extern volatile sig_atomic_t background_counter; // 记录后台任务的序号
extern volatile sig_atomic_t child_done; //记录有多少个子进程完成
extern BackgroundTask *completed_tasks;
extern Alias aliases[MAX_ALIASES];
extern int num_aliases;

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
        dup2(0, STDOUT_FILENO);
        dup2(1, STDIN_FILENO);

        if (child_done > 0) {
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
                free(last_command); // 释放上一个命令的内存
                last_command = strdup(line); //更新最后一条命令
            }
        }

        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);
    save_history(history_file);

    free(last_command);
}

// 解析命令中的重定向
int parse_redirection(char **args, int *in_fd, int *out_fd) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "lsh: 未预期的记号 \"newline\" 附近有语法错误\n");
                return -1;
            }
            *out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (*out_fd == -1) {
                perror("lsh");
                return -1;
            }
            args[i] = NULL;
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
        } else if (strcmp(args[i], ">>") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "lsh: 未预期的记号 \"newline\" 附近有语法错误\n");
                return -1;
            }
            *out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (*out_fd == -1) {
                perror("lsh");
                return -1;
            }
            args[i] = NULL;
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
        } else if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "lsh: 未预期的记号 \"newline\" 附近有语法错误\n");
                return -1;
            }
            *in_fd = open(args[i + 1], O_RDONLY);
            if (*in_fd == -1) {
                perror("lsh");
                return -1;
            }
            args[i] = NULL;
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
        }
    }
    return 0;
}

// 执行内部命令
int execute_internal_command(int i, char **args, int in_fd, int out_fd, bool is_background) {
    if (is_background) {
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程执行内置命令
            if (in_fd != 0) {
                dup2(in_fd, 0);
                close(in_fd);
            }
            if (out_fd != 1) {
                dup2(out_fd, 1);
                close(out_fd);
            }
            exit((*builtin_func[i])(args));
        } else if (pid < 0) {
            perror("fork");
            return 1;
        } else {
            // 父进程
            printf("[%d] %d\n", ++background_counter, pid);
            return 1;
        }
    } else {
        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }
        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }
        return (*builtin_func[i])(args);
    }
}

// 执行单个命令
int lsh_launch_single(char **args, int in_fd, int out_fd, bool is_background) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // 子进程
        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }
        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }

        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Fork出错
        perror("fork");
    } else {
        // 父进程
        if (is_background) {
            printf("[%d] %d\n", ++background_counter, pid);
        } else {
            waitpid(pid, &status, WUNTRACED); // 等待子进程结束
        }
    }
    return 1;
}

// 执行管道命令
int lsh_launch_pipeline(char ***commands, int num_commands, bool is_background) {
    int in_fd = 0, fd[2];
    pid_t pid;
    int status;

    for (int i = 0; i < num_commands; i++) {
        if (i != num_commands - 1) {
            pipe(fd);
        }

        pid = fork();
        if (pid == 0) {
            // 子进程
            if (in_fd != 0) {
                dup2(in_fd, 0);
                close(in_fd);
            }
            if (i != num_commands - 1) {
                dup2(fd[1], 1);
                close(fd[1]);
            }
            if (is_builtin(commands[i][0])) {
                exit((*builtin_func[find_builtin_index(commands[i][0])])(commands[i]));
            } else {
                if (execvp(commands[i][0], commands[i]) == -1) {
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            }
        } else if (pid < 0) {
            perror("fork");
            return 1;
        } else {
            // 父进程
            if (in_fd != 0) {
                close(in_fd);
            }
            if (i != num_commands - 1) {
                close(fd[1]);
                in_fd = fd[0];
            }
            if (!is_background) {
                waitpid(pid, &status, WUNTRACED);
            } else {
                printf("[%d] %d\n", ++background_counter, pid);
            }
        }
    }
    return 1;
}

// 选择执行命令
int lsh_execute(char **args) {
    bool is_background = false;
    int in_fd = 0, out_fd = 1;
    char ***pipe_commands = NULL;
    int num_commands = 0, cmd_index = 0;

    // 用户输入了一个空命令
    if (args[0] == NULL) {
        return 1;
    }

    // 检查是否有后台运行标记
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "&") == 0) {
            args[i] = NULL; // 将"&"从参数中删除
            is_background = true;
            if (args[0] == NULL) {
                printf("未预期的记号 '&' 附近有语法错误\n");
                return 1;
            }
            break;
        }
    }

    // 计算命令的数量
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            num_commands++;
        }
    }
    num_commands++; // 最后一个命令

    // 分配内存
    pipe_commands = malloc(num_commands * sizeof(char **));
    for (int i = 0; i < num_commands; i++) {
        pipe_commands[i] = malloc(PIPE_BUF_SIZE * sizeof(char *));
    }

    // 解析管道命令
    num_commands = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_commands[num_commands][cmd_index] = NULL;
            num_commands++;
            cmd_index = 0;
        } else {
            pipe_commands[num_commands][cmd_index] = args[i];
            cmd_index++;
        }
    }
    pipe_commands[num_commands][cmd_index] = NULL;
    num_commands++;

    // 解析重定向
    if (parse_redirection(args, &in_fd, &out_fd) == -1) {
        return 1;
    }

    // 解析别名
    for (int i = 0; i < num_commands; i++) {
        for (int j = 0; j < num_aliases; j++) {
            if (strcmp(pipe_commands[i][0], aliases[j].name) == 0) {
                free(pipe_commands[i][0]);
                pipe_commands[i][0] = strdup(aliases[j].cmd);
                break;
            }
        }
    }

    int result;
    if (num_commands > 1) {
        result = lsh_launch_pipeline(pipe_commands, num_commands, is_background);
    } else {
        if (is_builtin(args[0])) {
            result = execute_internal_command(find_builtin_index(args[0]), args, in_fd, out_fd, is_background);
        } else {
            result = lsh_launch_single(args, in_fd, out_fd, is_background);
        }
    }

    // 释放内存
    for (int i = 0; i < num_commands; i++) {
        free(pipe_commands[i]);
    }
    free(pipe_commands);

    return result;
}


int main() {
    char* history_file = ".lsh_history";
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), history_file);
    // 运行命令循环
    lsh_loop(history_path);

    return EXIT_SUCCESS;
}
