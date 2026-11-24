#ifndef VTSH_PARSER_H
#define VTSH_PARSER_H
#include <stddef.h>  
#include <stdbool.h>

// типы операторов
typedef enum {
    OP_NONE,  // ластовая команда в строке (нет оператора после)
    OP_OR     // || - выполнить следующую, если эта упала (моя по заданию)
} op_kind_t;


// имя программы + аргументы
typedef struct {
    char **argv;   // массив строк
    size_t argc;   // кол-во аргументов (нулл не учитываем)
    bool is_bg;    // true если это фоновая команда
} simple_command_t;

// узел плана выполнения
typedef struct {
    simple_command_t cmd;    // команда
    op_kind_t next_op;       // оператор, который идёт после этой команды (|| или конец)
} command_node_t;

// весь план выполнения строки
typedef struct {
    command_node_t *nodes;   // массив узлов
    size_t count;            // количество узлов
} plan_t;

// свобода для динам памяти команды
void command_free(simple_command_t *cmd);

// свобода для динам памяти плана команд
void plan_free(plan_t *p);

// парсим строку в план выполнения
// 0 при успехе, -1 при ошибке
int parse_plan(const char *line, plan_t *out);

#endif