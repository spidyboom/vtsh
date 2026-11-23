#ifndef VTSH_PARSER_H
#define VTSH_PARSER_H
#include <stddef.h>  

// имя программы + аргументы
typedef struct {
    char **argv;   // массив строк
    size_t argc;   // кол-во аргументов (нулл не учитываем)
} simple_command_t;

// свобода для динам памяти
void command_free(simple_command_t *cmd);

// парсим строку в простую команду
// 0 при успехе, -1 при ошибке
int parse_simple(const char *line, simple_command_t *out);

#endif