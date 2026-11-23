#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "repl.h"

void repl_run(void) {
    char *line = NULL;      // указатель на буфер куда будем читать строку с помощью getline
    size_t cap = 0;         // капасити буфера
    // проверяем связан ли стандартный поток ввода (0) с терминалом, так как он может идти например с файла и нам не нужно выводить vtsh> постоянно
    int is_tty = isatty(STDIN_FILENO);  

    // while true
    for (;;) {
        // принтуем vtsh> только если stdin - это терминал
        if (is_tty) {
            printf("vtsh> ");
            fflush(stdout);  // принудительно выводим буфер чтобы отпечатолось сразу
        }
        
        ssize_t n = getline(&line, &cap, stdin);
        
        // если гетлайн вернул < 0, значит EOF или ошибка
        if (n < 0) {
            if (is_tty) printf("\n");
            break;
        }
        
        // заменяем новую строку на конец файла в сишке (\0)
        if (n > 0 && line[n-1] == '\n') {
            line[n-1] = '\0';
        }
        
        if (line[0] == '\0') {
            continue;
        }
        
        // pass
        printf("[echo] %s\n", line);
    }
    
    // освобождаем динамическую память, нам её выделил getline
    free(line);
}