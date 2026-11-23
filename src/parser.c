#define _GNU_SOURCE
#include <stdlib.h>  
#include <string.h> 
#include <ctype.h>
#include "parser.h"

// освобождает память занятую командой
void command_free(simple_command_t *cmd) {
    if (!cmd) return;                 
    
    // освобождаем каждую строку принадлежащую нашей структуре
    for (size_t i = 0; i < cmd->argc; ++i) {
        free(cmd->argv[i]); 
        cmd->argv[i] = NULL;  
    }
    
    // освобождаем сам массив, элементы освободили выше
    free(cmd->argv);
    cmd->argv = NULL;
    cmd->argc = 0;
}

// сам парсер
int parse_simple(const char *line, simple_command_t *out) {
    // структура команды
    out->argv = NULL;
    out->argc = 0;
    
    if (!line) return -1;
    
    // strtok_r меняет строку над которой работает
    char *copy = strdup(line);
    if (!copy) return -1;
    
    // выбираем начальное капасити в 8 и заполняем нулами все элементы
    size_t cap = 8;
    out->argv = calloc(cap, sizeof(char*));
    if (!out->argv) {
        free(copy);
        return -1;
    }
    
    // strtok_r разбивает строку на отдельные слова ограниченные разделителями
    // save вспомогательный указатель этой функции
    char *save = NULL;
    char *tok = strtok_r(copy, " \t", &save);
    
    // каждый токен новое слово из строки, последний токен строчки будет нулл, поэтому стопимся когда его встретим
    while (tok != NULL) {
        // проверка на переполнение, если не хватает увеличиваем в два раза
        if (out->argc + 1 >= cap) {
            cap *= 2;
            char **new_argv = realloc(out->argv, cap * sizeof(char*));
            if (!new_argv) {
                free(copy);
                command_free(out);
                return -1;
            }
            out->argv = new_argv;
        }
        
        // заполняем новый аргумент структуры токеном который получили посредством strtok_r
        out->argv[out->argc] = strdup(tok);
        if (!out->argv[out->argc]) {
            free(copy);
            command_free(out);
            return -1;
        }

        // инкрементим счетчик
        out->argc++;
        
        // ну и некст токен
        tok = strtok_r(NULL, " \t", &save);
    }
    
    // добавляем нулл в конец как сигнал конца 
    out->argv[out->argc] = NULL;
    
    free(copy);
    
    return 0;
}