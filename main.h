#ifndef OS_C_MAIN_H
#define OS_C_MAIN_H

#endif //OS_C_MAIN_H

#include <stdbool.h>


char **lsh_split_line(char *line);
void lsh_loop(const char *history_file);
int lsh_execute(char **args);