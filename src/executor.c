#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h> 
#include <errno.h>
#include "executor.h"

// мегабайт для ребенка
#define CHILD_STACK_SIZE (1024 * 1024)

// лимит стека
// при нуле нету лимита
static rlim_t stack_limit = 0;

// всмпомогтательная структура для передачи аргументов в дочерний процесс
// clone необходимо передать указание на исполняемую функцию + обязательно передать аргумент (даже если он null)
typedef struct {
    char **argv;   // аргументы cmd->argv
    char **envp;   // переменные окружения
} clone_arg_t;

// переменные окружения (глобальные)
extern char **environ;

void executor_set_stack_limit(rlim_t limit);

// функция, которую передаем дочернему процессу
static int child_main(void *arg) {
    // clone требует передачу посредством void *, поэтому пере-пере-образовываем
    clone_arg_t *a = (clone_arg_t *)arg;
    
    // устанавливаем лимиты если требуется
    if (stack_limit > 0) {
        struct rlimit rl;
        
        // получаем текущие лимиты (не можем => проброс ошибки)
        if (getrlimit(RLIMIT_STACK, &rl) != 0) {
            perror("getrlimit");
            _exit(126);
        }
        
        // новый soft limit
        rl.rlim_cur = stack_limit;
        
        // если новый soft > hard, поднимаем и hard
        // Мб возникнут ошибки при доступе, так как макс поднимается от имени рута
        if (stack_limit > rl.rlim_max) {
            rl.rlim_max = stack_limit;
        }
        
        // применяем лимит
        if (setrlimit(RLIMIT_STACK, &rl) != 0) {
            perror("setrlimit");
            _exit(126);
        }
    }
    
    // стартуем команду
    // execve заменяет образ процесса на внешнюю программу
    // передаем имя, аргументы, переменные окружения
    // если все успешно то execve не вернется
    // если дропается пробрасываем ошибку
    execve(a->argv[0], a->argv, a->envp);
    
    int exit_code;
    if (errno == ENOENT) {
        // не нашли файлик
        exit_code = 127;
    } else {
        // файлик не запсукается(
        exit_code = 126;
    }
    
    // ошибку во второй поток
    fprintf(stderr, "vtsh: %s: %s\n", a->argv[0], strerror(errno));
    
    // так как execve не сработал и мы по прежнему делим ресы с потоком родителем, дабы не зачистить данные бросаем _exit а не exit
    _exit(exit_code);
}

// установка лимита стека (из REPL или конфигурации)
void executor_set_stack_limit(rlim_t limit) {
    stack_limit = limit;
}

// запуск команды
int execute_simple(const simple_command_t *cmd) {
    if (cmd->argc == 0) {
        return 0;
    }
    
    // АПДЕЙТ ДРУЗЬЯ
    // ВСТРОЕННАЯ КОМАНДА: setstacklimit
    // устанавливает лимит стека для следующих команд
    if (strcmp(cmd->argv[0], "setstacklimit") == 0) {
        // есть ли аргумент
        if (cmd->argc < 2) {
            fprintf(stderr, "usage: setstacklimit <bytes>\n");
            return 1;
        }
        rlim_t limit = (rlim_t)strtoull(cmd->argv[1], NULL, 10);
        executor_set_stack_limit(limit);
        return 0;
    }
    
    // ВСТРОЕННАЯ КОМАНДА: exit 
    if (strcmp(cmd->argv[0], "exit") == 0) {
        // без _ так как мы в родительском процессе
        exit(0);
    }
    
    // ВСТРОЕННАЯ КОМАНДА: cd 
    if (strcmp(cmd->argv[0], "cd") == 0) {
        const char *dir = (cmd->argc > 1) ? cmd->argv[1] : getenv("HOME");
        if (chdir(dir) != 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }
    
    // UNKNOWN WORLDS PRESENTS
    
    // выделяем стек не важно где | 1 мб | чтение/запись | приватно для процесса | ядро это крч стек
    char *stack = mmap(NULL, CHILD_STACK_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                       -1, 0);
    
    if (stack == MAP_FAILED) {
        perror("mmap stack");
        return 1;
    }
    
    // стек растет вниз, берем наивысший index
    void *stack_top = stack + CHILD_STACK_SIZE;
    
    clone_arg_t arg = {
        .argv = cmd->argv,
        .envp = environ
    };
    
    // КЛОНЧИК
    // наша функция | указатель стека | сигнал возврата | ссылка на аргмент (наша структура)
    pid_t pid = clone(child_main, stack_top, SIGCHLD, &arg);
    
    if (pid < 0) {
        perror("clone");
        munmap(stack, CHILD_STACK_SIZE);
        return 1;
    }
    
    // ждем окончания дочернего процесса
    // waitpid(id процесса от clone, статус заврешнения, ждем пока не помрет)
    // ждем так как не хотим чтобы он стал зомби процессом
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        // чистим стек
        munmap(stack, CHILD_STACK_SIZE);
        return 1;
    }
    
    // чистим стек
    munmap(stack, CHILD_STACK_SIZE);
    
    // статус завершения
    if (WIFEXITED(status)) {
        // процесс завершился корректно через exit() или _exit()
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        // процесс убит сигналом 
        return 128 + WTERMSIG(status);
    }
    
    // pass
    return 1;
}

// выполняет план команд
int execute_plan(const plan_t *plan) {
    int last_status = 0;

    // проходим по всем узлам плана
    for (size_t i = 0; i < plan->count; ++i) {
        const command_node_t *node = &plan->nodes[i];
        
        // нужно ли вообще выполнять команду
        if (i > 0) {
            op_kind_t prev_op = plan->nodes[i - 1].next_op;
            
            // если предыдущий оператор был || и последняя команда успешна (статус 0), то пропускаем команду
            if (prev_op == OP_OR && last_status == 0) {
                continue;
            }
        }

        // выполняем команду
        if (node->cmd.argc > 0) {
            last_status = execute_simple(&node->cmd);
        }
    }

    return last_status;
}