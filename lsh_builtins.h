#include <stdbool.h>

#ifndef OS_C_LSH_BUILTINS_H
#define OS_C_LSH_BUILTINS_H

#endif //OS_C_LSH_BUILTINS_H

#define MAX_ALIASES 50
#define MAX_ALIAS_NAME 50
#define MAX_ALIAS_CMD 100

typedef struct Alias{
    char name[MAX_ALIAS_NAME];
    char cmd[MAX_ALIAS_CMD];
} Alias;


bool is_builtin(char *command);
int find_builtin_index(char *command);
int lsh_num_builtins();
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_ls(char **args);
int lsh_cat(char **args);
int lsh_history(char **args);
int lsh_grep(char **args);
int lsh_echo(char **args);
int lsh_type(char **args);
int lsh_alias(char **args);