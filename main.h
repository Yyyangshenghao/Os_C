#ifndef OS_C_MAIN_H
#define OS_C_MAIN_H

#endif //OS_C_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>

char *lsh_read_line(void);
char **lsh_split_line(char *line);
void lsh_loop(void);
int lsh_launch(char **args);
int lsh_execute(char **args);
