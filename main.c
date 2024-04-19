#include "main.h"
#include "lsh_builtins.h"

#define PATH_MAX 1024

void lsh_loop(const char *history_file)
{
    char cwd[PATH_MAX];
    char *line;
    char **args;
    int status;

    using_history();
    read_history(history_file);

    do {
        getcwd(cwd, sizeof(cwd)); // 获取当前工作目录
        printf("%s", cwd); // 打印当前工作目录
        line = lsh_read_line();
        if(line != NULL){
            add_history(line);
            append_history(1, history_file); // 实时保存到历史文件
        }

        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
        printf("\n");
    } while (status);
}

int lsh_launch(char **args)
{
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
}

extern char *builtin_str[];
extern int (*builtin_func[]) (char **);
int lsh_execute(char **args)
{
    int i;

    if (args[0] == NULL) {
        // 用户输入了一个空命令
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}

void save_history(const char *history_file) {
    write_history(history_file);
}

int main()
{
    char* history_file = ".lsh_history";
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), history_file);

    // 运行命令循环
    lsh_loop(history_path);

    save_history(history_path);
    return EXIT_SUCCESS;
}

