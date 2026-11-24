#define _GNU_SOURCE
#include <stdlib.h>  
#include <string.h>
#include <stdbool.h> 
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

// освобождает память занятую планом
// так как структуры похожи то все как и выше
void plan_free(plan_t *p) {
    if (!p) return;

    for (size_t i = 0; i < p->count; ++i) {
        command_free(&p->nodes[i].cmd);
    }

    free(p->nodes);
    p->nodes = NULL;
    p->count = 0;
}

// парс одной простой команды из фрагмента строки
// start - начало фрагмента, len - длина фрагмента
static int parse_single_command(const char *start, size_t len, simple_command_t *out) {
    out->argv = NULL;
    out->argc = 0;
    out->is_bg = false;

    // копируем ровно len символов так как не имеем желания изменять исходную структуру
    char *copy = strndup(start, len);
    if (!copy) return -1;

    // проверяем, не заканчивается ли на '&' (фоновый процесс)
    // ищем последний непробельный символ
    char *p = copy + len - 1;
    while (p >= copy && isspace((unsigned char)*p)) p--;
    
    if (p >= copy && *p == '&') {
        // вырезаем &
        *p = '\0';
        out->is_bg = true;
    }
    
    // выбираем начальное капасити в 8 и заполняем нулами все элементы
    size_t cap = 8;
    out->argv = calloc(cap, sizeof(char*));
    if (!out->argv) {
        free(copy);
        return -1;
    }

    // strtok_r разбивает строку на отдельные слова ограниченные разделителями
    // save вспомогательный указатель этой функции
     // каждый токен новое слово из строки, последний токен строчки будет нулл, поэтому стопимся когда его встретим
    char *save = NULL;
    for (char *tok = strtok_r(copy, " \t", &save);
         tok != NULL;
         tok = strtok_r(NULL, " \t", &save)) {
        
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
        out->argc++;
    }
    
    // добавляем нулл в конец как сигнал конца 
    out->argv[out->argc] = NULL;
    free(copy);
    return 0;
}

// ищет оператор || в строке
// возвращает указатель на позицию оператора или NULL
static const char *find_or_operator(const char *str) {
    const char *p = str;
    
    // ищем подстроку ||
    while ((p = strstr(p, "||")) != NULL) {
        // слева пробел или начало строки?
        bool left_ok = (p == str) || isspace((unsigned char)*(p - 1));

        // справа пробел или конец строки?
        bool right_ok = isspace((unsigned char)*(p + 2)) || (*(p + 2) == '\0');
        
        if (left_ok && right_ok) {
            return p;  // найден оператор
        }
        p += 2;  // ищем дальше
    }
    
    return NULL;  // оператор не найден
}

// разбирает строку в план выполнения
// аналогично изначальному парсеру одной команды
int parse_plan(const char *line, plan_t *out) {
    out->nodes = NULL;
    out->count = 0;

    if (!line || line[0] == '\0') return 0;

    // капасити на 4 узла
    size_t cap = 4;
    out->nodes = calloc(cap, sizeof(command_node_t));
    if (!out->nodes) return -1;

    const char *current = line;
    
    // пока есть чему работать (что парсить)
    while (*current != '\0') {
        // скип ведущих пробелов
        while (isspace((unsigned char)*current)) current++;
        if (*current == '\0') break;

        // ищем оператор ||
        const char *op_pos = find_or_operator(current);
        
        // определяем конец текущей команды
        const char *cmd_end = op_pos ? op_pos : (current + strlen(current));
        
        // длина команды
        size_t cmd_len = cmd_end - current;
        
        // убираем пробелы в конце команды
        while (cmd_len > 0 && isspace((unsigned char)current[cmd_len - 1])) {
            cmd_len--;
        }
        
        // eсли команда пустая (длина нолик), пропускаем
        if (cmd_len == 0) {
            if (op_pos) {
                current = op_pos + 2;
            } else {
                break;
            }
            continue;
        }

        // умножаем число узлов на два на всякий
        if (out->count >= cap) {
            cap *= 2;
            command_node_t *new_nodes = realloc(out->nodes, cap * sizeof(command_node_t));
            if (!new_nodes) {
                plan_free(out);
                return -1;
            }
            out->nodes = new_nodes;
        }

        // парсим команду
        if (parse_single_command(current, cmd_len, &out->nodes[out->count].cmd) != 0) {
            plan_free(out);
            return -1;
        }
        
        // устанавливаем оператор
        out->nodes[out->count].next_op = op_pos ? OP_OR : OP_NONE;
        out->count++;

        // оператор отстутствует => мы дошли до конца строки
        if (!op_pos) {
            break;
        }

        // переходим к следующей части после оператора "||" (некст узлу)
        current = op_pos + 2;
    }

    return 0;
}