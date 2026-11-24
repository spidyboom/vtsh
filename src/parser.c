#define _GNU_SOURCE
#include <stdlib.h>  
#include <string.h>
#include <stdbool.h> 
#include <ctype.h>
#include "parser.h"

// освобождает память занятую командой
void simple_command_free(simple_command_t *cmd) {
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

// освобождает память занятую конвейером
// так как структуры похожи то все как и выше
void pipeline_free(pipeline_t *pipe) {
    if (!pipe) return;
    for (size_t i = 0; i < pipe->count; ++i) {
        simple_command_free(&pipe->commands[i]);
    }
    free(pipe->commands);
    pipe->commands = NULL;
    pipe->count = 0;
}

// освобождает память занятую планом
// так как структуры похожи то все как и выше
void plan_free(plan_t *p) {
    if (!p) return;

    for (size_t i = 0; i < p->count; ++i) {
        pipeline_free(&p->nodes[i].pipeline);
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
                simple_command_free(out);
                return -1;
            }
            out->argv = new_argv;
        }
        
        // заполняем новый аргумент структуры токеном который получили посредством strtok_r
        out->argv[out->argc] = strdup(tok);
        if (!out->argv[out->argc]) {
            free(copy);
            simple_command_free(out);
            return -1;
        }
        out->argc++;
    }
    
    // добавляем нулл в конец как сигнал конца 
    out->argv[out->argc] = NULL;
    free(copy);
    return 0;
}

// ищет оператор ("||" или "|") в строке
// возвращает указатель на позицию оператора или NULL
static const char *find_op(const char *str, const char *op_str, size_t op_len) {
    const char *p = str;

    // ищем подстроки
    while ((p = strstr(p, op_str)) != NULL) {
        // слева пробел или начало строки?
        bool left_ok = (p == str) || isspace((unsigned char)*(p - 1));
        // справа пробел или конец строки?
        bool right_ok = isspace((unsigned char)*(p + op_len)) || (*(p + op_len) == '\0');

        if (left_ok && right_ok) return p; // найден оператор

        p += op_len; // ищем дальше
    }

    return NULL; // оператор не найден
}

// парсит конвейер ('|')
static int parse_pipeline(const char *start, size_t len, pipeline_t *out) {
    out->commands = NULL;
    out->count = 0;
    out->is_bg = false;

    // капасити на 4 узла
    size_t cap = 4;
    out->commands = calloc(cap, sizeof(simple_command_t));
    if (!out->commands) return -1;

    const char *current = start;
    const char *end = start + len;

    while (current < end) {
        // скип ведущих пробелов
        while (current < end && isspace((unsigned char)*current)) current++;
        if (current >= end) break;

        const char *pipe_pos = find_op(current, "|", 1);
        // убедимся, что это не "||"
        if (pipe_pos && (pipe_pos + 1 < end) && *(pipe_pos + 1) == '|') {
            pipe_pos = NULL; 
        }

        // определяем конец текущей команды
        const char *cmd_end = pipe_pos ? pipe_pos : end;

        // длина команды
        size_t cmd_len = cmd_end - current;

        // убираем пробелы в конце команды
        while (cmd_len > 0 && isspace((unsigned char)current[cmd_len - 1])) cmd_len--;

        if (cmd_len == 0) {
            current = pipe_pos ? pipe_pos + 1 : end;
            continue;
        }

        if (out->count >= cap) {
            cap *= 2;
            simple_command_t *nv = realloc(out->commands, cap * sizeof(simple_command_t));
            if (!nv) { pipeline_free(out); return -1; }
            out->commands = nv;
        }

        if (parse_single_command(current, cmd_len, &out->commands[out->count]) != 0) {
            pipeline_free(out);
            return -1;
        }
        
        // если последняя команда в конвейере имеет '&', помечаем весь конвейер
        if (out->commands[out->count].is_bg) {
            out->is_bg = true;
        }
        
        out->count++;
        current = pipe_pos ? pipe_pos + 1 : end;
    }
    return 0;
}

// разбирает строку в план выполнения
// аналогично изначальному парсеру одной команды
int parse_plan(const char *line, plan_t *out) {
    out->nodes = NULL;
    out->count = 0;

    if (!line || line[0] == '\0') return 0;

    // капасити на 4 узла
    size_t cap = 4;
    out->nodes = calloc(cap, sizeof(plan_node_t));
    if (!out->nodes) return -1;

    const char *current = line;
    
    // пока есть чему работать (что парсить)
    while (*current != '\0') {
        while (isspace((unsigned char)*current)) current++;
        if (*current == '\0') break;

        const char *or_pos = find_op(current, "||", 2);
        const char *node_end = or_pos ? or_pos : (current + strlen(current));
        size_t node_len = node_end - current;
        while (node_len > 0 && isspace((unsigned char)current[node_len - 1])) node_len--;

        if (node_len == 0) {
            current = or_pos ? or_pos + 2 : current + strlen(current);
            continue;
        }

        if (out->count >= cap) {
            cap *= 2;
            plan_node_t *nv = realloc(out->nodes, cap * sizeof(plan_node_t));
            if (!nv) { plan_free(out); return -1; }
            out->nodes = nv;
        }

        if (parse_pipeline(current, node_len, &out->nodes[out->count].pipeline) != 0) {
            plan_free(out);
            return -1;
        }

        out->nodes[out->count].next_op = or_pos ? OP_OR : OP_NONE;
        out->count++;
        current = or_pos ? or_pos + 2 : current + strlen(current);
    }
    return 0;
}