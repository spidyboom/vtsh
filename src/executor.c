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

// мегабайт для ребенка (для каждого дочернего процесса)
#define CHILD_STACK_SIZE (1024 * 1024)

// лимит стека
// при нуле нету лимита
static rlim_t stack_limit = 0;

// всмпомогтательная структура для передачи аргументов в дочерний процесс
// clone необходимо передать указание на исполняемую функцию + обязательно передать аргумент (даже если он null)
typedef struct {
    char **argv;   // аргументы cmd->argv
    char **envp;   // переменные окружения
    int stdin_fd;  // -1 если не нужно перенаправлять
    int stdout_fd; // -1 если не нужно перенаправлять
    int *close_fds;       // массив файловых дескрипторов 
    int close_fds_count;  // количество дескрипторов в массиве
} clone_arg_t;

// переменные окружения (глобальные)
extern char **environ;

void executor_set_stack_limit(rlim_t limit);
static pid_t spawn_command(const simple_command_t *cmd, int stdin_fd, int stdout_fd,
                           int *close_fds, int close_fds_count);
static int execute_pipeline(const pipeline_t *pl, int *out_status);

// функция, которую передаем дочернему процессу
static int child_main(void *arg) {
    // clone требует передачу посредством void *, поэтому пере-пере-образовываем
    clone_arg_t *a = (clone_arg_t *)arg;

    // закрываем все ненужные файловые дескрипторы каналов
    // пайплайн работает так, что пока открыты концы на запись/чтение, процесс с другого конца будет ждать этого бесконечно долго
    for (int i = 0; i < a->close_fds_count; i++) {
        int fd = a->close_fds[i];
        // не закрываем stdin_fd и stdout_fd - они нужны для dup2
        if (fd != a->stdin_fd && fd != a->stdout_fd) {
            close(fd);
        }
    }

    if (a->stdin_fd != -1) {
        dup2(a->stdin_fd, STDIN_FILENO);
        // закрываем оргинал
        close(a->stdin_fd);
    }
    if (a->stdout_fd != -1) {
        dup2(a->stdout_fd, STDOUT_FILENO);
        // закрываем оргинал
        close(a->stdout_fd);
    }

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

// запуск одной команды с возможностью перенаправления stdin/stdout
// возвращает PID созданного процесса (0 для встроенных команд)
static pid_t spawn_command(const simple_command_t *cmd, int stdin_fd, int stdout_fd,
                           int *close_fds, int close_fds_count) {
    if (cmd->argc == 0) return 0;

    // встроенные команды 
    if (stdin_fd == -1 && stdout_fd == -1) {
        // АПДЕЙТ ДРУЗЬЯ
        // ВСТРОЕННАЯ КОМАНДА: setstacklimit
        // устанавливает лимит стека для следующих команд
        if (strcmp(cmd->argv[0], "setstacklimit") == 0) {
            // есть ли аргумент
            if (cmd->argc < 2) {
                fprintf(stderr, "usage: setstacklimit <bytes>\n");
                return 0;
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
            }
            return 0;
        }
    }

    // UNKNOWN WORLDS PRESENTS
    // выделяем стек не важно где | 1 мб | чтение/запись | приватно для процесса | ядро это крч стек
    char *stack = mmap(NULL, CHILD_STACK_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                       -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap stack");
        return -1;
    }

    // стек растет вниз, берем наивысший index
    void *stack_top = stack + CHILD_STACK_SIZE;

    clone_arg_t arg = {
        .argv = cmd->argv,
        .envp = environ,
        .stdin_fd = stdin_fd,
        .stdout_fd = stdout_fd,
        .close_fds = close_fds,
        .close_fds_count = close_fds_count
    };

    // КЛОНЧИК
    // наша функция | указатель стека | сигнал возврата | ссылка на аргмент (наша структура)
    pid_t pid = clone(child_main, stack_top, SIGCHLD, &arg);
    if (pid < 0) {
        perror("clone");
        munmap(stack, CHILD_STACK_SIZE);
        return -1;
    }

    // родитель сразу освобождает свой стек, он нужен только при clone
    munmap(stack, CHILD_STACK_SIZE);
    return pid;
}

// запуск команды (для одиночных команд)
int execute_simple(const simple_command_t *cmd) {
    if (cmd->argc == 0) return 0;

    pid_t pid = spawn_command(cmd, -1, -1, NULL, 0);
    if (pid < 0) return 1;
    if (pid == 0) return 0; // встроенная команда

    // если процесс фоновый
    if (cmd->is_bg) {
        // не ждём завершения, сразу освобождаем стек
        printf("[%d]\n", pid);
        return 0;
    }

    // ждем окончания дочернего процесса (не фонового)
    // waitpid(id процесса от clone, статус заврешнения, ждем пока не помрет)
    // ждем так как не хотим чтобы он стал зомби процессом
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

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

// выполнение конвейера
static int execute_pipeline(const pipeline_t *pl, int *out_status) {
    if (pl->count == 0) {
        *out_status = 0;
        return 0;
    }

    // одна команда — особый случай, без каналов
    if (pl->count == 1) {
        if (pl->is_bg) {
            pid_t pid = spawn_command(&pl->commands[0], -1, -1, NULL, 0);
            if (pid > 0) {
                printf("[%d]\n", pid);
            }
            *out_status = 0;
            return 0;
        }
        *out_status = execute_simple(&pl->commands[0]);
        return *out_status;
    }

    // несколько команд тада создаём каналы
    int num_pipes = (int)pl->count - 1;
    int pipes[num_pipes][2];

    // создаём все каналы заранее (канал = два файловых дескриптора)
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            // закрываем уже созданные каналы если упало с ошибкой
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            *out_status = 1;
            return -1;
        }
    }

    // массив всех файловых дескрипторов каналов
    int all_fds[num_pipes * 2];
    for (int i = 0; i < num_pipes; i++) {
        all_fds[i * 2] = pipes[i][0];
        all_fds[i * 2 + 1] = pipes[i][1];
    }
    int all_fds_count = num_pipes * 2;

    pid_t pids[pl->count];

    // создаём процессы для каждого звена
    for (size_t i = 0; i < pl->count; i++) {
        // первая команда: stdin из терминала, stdout в первый канал
        // мидовая команда: stdin из предыдущего канала, stdout в следующий
        // последняя команда: stdin из последнего канала, stdout в терминал
        int in_fd = (i == 0) ? -1 : pipes[i - 1][0];
        int out_fd = (i == pl->count - 1) ? -1 : pipes[i][1];

        pid_t pid = spawn_command(&pl->commands[i], in_fd, out_fd, all_fds, all_fds_count);
        if (pid < 0) {
            // ошибка создания процесса => закрываем все каналы
            for (int j = 0; j < num_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            *out_status = 1;
            return -1;
        }
        pids[i] = pid;
    }

    // родитель закрывает все концы всех каналов
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // фоновый режим для всего конвейера
    if (pl->is_bg) {
        printf("[%d]\n", pids[pl->count - 1]);
        *out_status = 0;
        return 0;
    }

    // ждём завершения всех процессов конвейера
    int last_status = 0;
    for (size_t i = 0; i < pl->count; i++) {
        int status = 0;
        if (waitpid(pids[i], &status, 0) < 0) {
            perror("waitpid");
            continue;
        }
        // статус конвейера = статус последнего звена
        if (i == pl->count - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);
            } else {
                last_status = 1;
            }
        }
    }

    *out_status = last_status;
    return last_status;
}

// выполняет план команд
int execute_plan(const plan_t *plan) {
    int last_status = 0;
    // проходим по всем узлам плана
    for (size_t i = 0; i < plan->count; ++i) {
        const plan_node_t *node = &plan->nodes[i];
        // нужно ли вообще выполнять команду
        if (i > 0) {
            op_kind_t prev_op = plan->nodes[i - 1].next_op;
            // если предыдущий оператор был || и последняя команда успешна (статус 0), то пропускаем команду
            if (prev_op == OP_OR && last_status == 0) {
                continue;
            }
        }
        // выполняем конвейер
        if (node->pipeline.count > 0) {
            execute_pipeline(&node->pipeline, &last_status);
        }
    }
    return last_status;
}