#include "main.h"
#include "lsh_builtins.h"

char *builtin_str[] = {
        "cd",
        "help",
        "exit",
        "ls",
};

int (*builtin_func[]) (char **) = {
        &lsh_cd,
        &lsh_help,
        &lsh_exit,
        &lsh_ls,
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}


/*
  内置命令的函数实现。
*/
#define PATH_MAX 1024
int lsh_cd(char **args)
{
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

int lsh_help(char **args)
{
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

int lsh_ls(char **args)
{
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    int show_all = 0;           // -a
    int show_hidden = 0;        // -l
    int long_format = 0;        // -h

    // 如果没有参数，执行默认的 ls 操作
    if(args[1] == NULL) {
        int i = 1; // 跳过命令名
        while(args[i] != NULL){
            // 如果参数是选项“-a”，则设置显示隐藏文件标志
            if(strcmp(args[i], "-a") == 0){
                show_hidden = 1;
            }

            // 如果参数是选项“-l”，则设置以长格式显示标志
            else if(strcmp(args[i], "-l") == 0){
                long_format = 1;
            }
        }


        dir = opendir(".");
        if (dir == NULL) {
            perror("opendir");
            return 1; //返回1表示执行失败
        }

        while ((entry = readdir(dir)) != NULL) {
            //过滤掉以 . 开头的目录项
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

        while((entry = readdir(dir)) != NULL){
            if(stat(entry->d_name, &file_stat) == -1){
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

    return 1;
}


int lsh_exit(char **args)
{
    return 0;
}