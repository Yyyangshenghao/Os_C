//
// Created by ysh on 24-6-17.
//

#include "history.h"
#include "main.h"
#include "lsh_builtins.h"
#include "bg.h"
#include <readline/history.h>

void init_history(const char *history_file) {
    stifle_history(100);
    using_history();
    read_history(history_file);
}

void save_history(const char *history_file) {
    write_history(history_file);
}