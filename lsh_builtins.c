#include "lsh_builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <readline/history.h>
#include <stdbool.h>

Alias aliases[MAX_ALIASES];
int num_aliases = 0;

char *builtin_str[] = {
        "cd",
        "help",
        "exit",
        "ls",
        "cat",
        "history",
        "grep",
        "echo",
        "type",
        "alias",
};

int (*builtin_func[]) (char **) = {
        &lsh_cd,
        &lsh_help,
        &lsh_exit,
        &lsh_ls,
        &lsh_cat,
        &lsh_history,
        &lsh_grep,
        &lsh_echo,
        &lsh_type,
        &lsh_alias,
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

// 检查命令是否为内置命令
bool is_builtin(char *command) {
    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(command, builtin_str[i]) == 0) {
            return true;
        }
    }
    return false;
}

// 获取内置命令的索引
int find_builtin_index(char *command) {
    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(command, builtin_str[i]) == 0) {
            return i;
        }
    }
    return -1;
}

/*
  内置命令的函数实现。
*/
#define PATH_MAX 1024
int lsh_cd(char **args) {
    char cwd[PATH_MAX];
    if (args[1] == NULL) {
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    } else {
        if (chdir(args[1]) != 0) {
            printf("系统找不到指定的路径。\n");
        }
    }
    return 1;
}

int lsh_help(char **args) {
    int i;
    printf("21013139's LSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_ls(char **args) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    // 如果没有参数，执行默认的 ls 操作
    if (args[1] == NULL) {
        dir = opendir(".");
        if (dir == NULL) {
            perror("opendir");
            return 1;  // 返回1表示执行失败
        }

        while ((entry = readdir(dir)) != NULL) {
            // 过滤掉以 . 开头的目录项
            if (entry->d_name[0] != '.') {
                printf("%s\n", entry->d_name);
            }
        }

        closedir(dir);
    }

    else if(strcmp(args[1], "-l") == 0){
        dir = opendir(".");
        if(dir == NULL){
            perror("opendir");
            return 1;
        }

        while((entry = readdir(dir)) != NULL) {
            if(stat(entry->d_name, &file_stat) == -1) {
                perror("stat");
                return 1;
            }

            printf((S_ISDIR(file_stat.st_mode)) ? "d" : "-");
            printf((file_stat.st_mode & S_IRUSR) ? "r" : "-");
            printf((file_stat.st_mode & S_IWUSR) ? "w" : "-");
            printf((file_stat.st_mode & S_IXUSR) ? "x" : "-");
            printf((file_stat.st_mode & S_IRGRP) ? "r" : "-");
            printf((file_stat.st_mode & S_IWGRP) ? "w" : "-");
            printf((file_stat.st_mode & S_IXGRP) ? "x" : "-");
            printf((file_stat.st_mode & S_IROTH) ? "r" : "-");
            printf((file_stat.st_mode & S_IWOTH) ? "w" : "-");
            printf((file_stat.st_mode & S_IXOTH) ? "x" : "-");
            printf(" %ld", file_stat.st_nlink);
            printf(" %s", getpwuid(file_stat.st_uid)->pw_name);
            printf(" %s", getgrgid(file_stat.st_gid)->gr_name);
            printf(" %lld", file_stat.st_size);
            printf(" %s", strtok(ctime(&file_stat.st_mtime), "\n"));
            printf(" %s\n", entry->d_name);
        }
    }

    // 如果有参数"-a"，显示所有文件，包括隐藏文件
    else if(strcmp(args[1], "-a") == 0){
        dir = opendir(".");
        if(dir == NULL){
            perror("opendir");
            return 1; // 返回1表示执行失败
        }

        while((entry = readdir(dir)) != NULL){
            printf("%s\n", entry->d_name);
        }

        closedir(dir);
    }

    else{
        printf("'%s'为未知的参数\n",args[1]);
        return 1;
    }

    return 1;
}

int lsh_cat(char **args) {
    char *filename = args[1];
    FILE *file = fopen(filename,"r");
    if(file == NULL) {
        fprintf(stderr,"cat:无法打开文件 %s\n", filename);
        return 1;
    }
    int c;
    while((c = fgetc(file)) != EOF) {
        putchar(c);
    }
    fclose(file);
    printf("\n");
    return 1;
}

int lsh_history(char **args) {
    if(args[1] == NULL) {
        HIST_ENTRY **my_history_list = history_list();
        if(history_list == NULL) {
            printf("No history available\n");
            return 1;
        }
        int i = 0;
        while(my_history_list[i] != NULL) {
            printf("%d\t%s\n", i + history_base, my_history_list[i]->line);
            i++;
        }
        return 1;
    }

    else {
        printf("'%s'为未知的参数\n",args[1]);
        return 1;
    }
}

int lsh_grep(char **args){
    if (args[1] == NULL){
        fprintf(stderr, "Usage: grep <pattern> [<file>]\n");
        return 1;
    }

    char *pattern = args[1];
    char *filename = NULL;
    FILE *file = stdin; // 默认从标准输入读取

    if (args[2] != NULL){
        filename = args[2];
        file = fopen(filename, "r");
        if (file == NULL){
            perror("fopen");
            return 1;
        }
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // 从文件或标准输入中读取每一行数据
    while ((read = getline(&line, &len, file)) != -1){
        if (strstr(line, pattern) != NULL){
            printf("%s", line);
        }
    }

    free(line);
    if (file != stdin){
        fclose(file); // 关闭文件，不关闭 stdin
    }

    return 1;
}

int lsh_echo(char **args){
    bool newline = true; // 默认情况下在最后输出换行符
    int i = 1;

    // 检查是否有 -n 选项
    if (args[1] != NULL && strcmp(args[1], "-n") == 0){
        newline = false;
        i++;
    }

    // 输出所有参数
    for (; args[i] != NULL; i++){
        if (i > 1){
            printf(" ");
        }
        printf("%s", args[i]);
    }

    if (newline){
        printf("\n");
    }

    return 1;
}

int lsh_type(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: 未提供命令名称\n");
        return 1;
    }

    char *command = args[1];

    // 检查是否是内置命令
    if (is_builtin(command)) {
        printf("%s 是一个内置命令\n", command);
        return 1;
    }

    // 检查是否是别名
    for (int i = 0; i < num_aliases; i++) {
        if (strcmp(command, aliases[i].name) == 0) {
            printf("%s 是一个别名: %s\n", command, aliases[i].cmd);
            return 1;
        }
    }

    // 检查是否是外部命令
    char *path = getenv("PATH");
    if (path == NULL) {
        fprintf(stderr, "lsh: PATH 环境变量未设置\n");
        return 1;
    }

    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        perror("strdup");
        return 1;
    }

    char *dir = strtok(path_copy, ":");
    while (dir != NULL) {
        char cmd_path[PATH_MAX];
        snprintf(cmd_path, sizeof(cmd_path), "%s/%s", dir, command);
        if (access(cmd_path, X_OK) == 0) {
            printf("%s 是一个外部命令: %s\n", command, cmd_path);
            free(path_copy);
            return 1;
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);

    printf("%s: 未找到命令\n", command);
    return 1;
}


int lsh_alias(char **args){
    if (args[1] == NULL){
        // 如果没有提供参数，则显示所有别名
        for (int i = 0; i < num_aliases; i++){
            printf("%s='%s'\n", aliases[i].name, aliases[i].cmd);
        }
        return 1;
    } else if (args[2] == NULL){
        // 如果只提供了别名名称，则显示对应的命令
        for (int i = 0; i <= num_aliases; i++){
            if (strcmp(args[1], aliases[i].name) == 0){
                printf("%s='%s'\n", aliases[i].name, aliases[i].cmd);
                return 1;
            }
        }
        fprintf(stderr, "alias: '%s' 未定义\n", args[1]);
        return 1;
    } else if (args[3] == NULL){
        // 如果提供了别名名称和命令，则创建或更新别名
        for (int i = 0; i < num_aliases; i++){
            if (strcmp(args[1], aliases[i].name) == 0){
                strncpy(aliases[i].cmd, args[2], MAX_ALIAS_CMD - 1);
                return 1;
            }
        }

        // 如果没有找到同名别名，则添加新的别名
        if (num_aliases >= MAX_ALIASES){
            fprintf(stderr, "alias: 别名太多，无法添加更多别名\n");
            return 1;
        }
        strncpy(aliases[num_aliases].name, args[1], MAX_ALIAS_NAME - 1);
        strncpy(aliases[num_aliases].cmd, args[2], MAX_ALIAS_CMD - 1);
        num_aliases++;
        return 1;
    } else{
        //如果提供了过多参数，则输出错误信息
        fprintf(stderr, "alias: 过多参数\n");
        return 1;
    }
}

int lsh_exit(char **args) {
    return 0;
}