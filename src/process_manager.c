#define _GNU_SOURCE
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "process_manager.h"

void reap_zombies(void) {
    int status;
    pid_t pid;
    
    // в цикле подбираем всех завершившихся детей ждем в неблокирующем режиме
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // процесс завершился
        if (WIFEXITED(status)) {
            printf("[%d] done (status %d)\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[%d] terminated by signal %d\n", pid, WTERMSIG(status));
        }
    }
}