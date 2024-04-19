cmake_minimum_required(VERSION 3.27)
project(Os_C C)

set(CMAKE_C_STANDARD 11)

add_executable(Os_C main.c
        main.h
        lsh_readline.c
        lsh_split_line.c
        lsh_builtins.c
        lsh_builtins.h
        main.c
)

target_link_libraries(Os_C /usr/lib/x86_64-linux-gnu/libreadline.so.8)