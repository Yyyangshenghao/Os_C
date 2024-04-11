#include "main.h"


#define LSH_TOK_BUFSIZE 256
#define LSH_TOK_DELIM " \t\r\n\a"
char **lsh_split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*)); // 用于存储分割后的子字符串的字符串数组
    char *token;

    // 检查内存分配是否成功
    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // 使用 strtok 函数分割输入的字符串，并存储分割后的子字符串到 tokens 数组中
    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token; // 存储子字符串到 tokens 数组
        position++; // 更新位置

        // 如果超出了 buffer 的大小，则重新分配
        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE; // 更新数组大小
            tokens = realloc(tokens, bufsize * sizeof(char*)); // 重新分配内存
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n"); // 输出错误信息
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM); // 继续获取下一个子字符串
    }
    tokens[position] = NULL; // 在末尾添加空指针，表示分割结束
    return tokens;
}