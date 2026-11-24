#ifndef VTSH_EXECUTOR_H
#define VTSH_EXECUTOR_H

#include <sys/resource.h>
#include "parser.h"

// экзекутор
int execute_simple(const simple_command_t *cmd);

// сет лимит стека
void executor_set_stack_limit(rlim_t limit);

// выполняет план команд
// возвращает статус последней выполненной команды
int execute_plan(const plan_t *plan);

#endif