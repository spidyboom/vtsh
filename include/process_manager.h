#ifndef VTSH_PROCESS_MANAGER_H
#define VTSH_PROCESS_MANAGER_H

// подбирает завершившиеся фоновые процессы (сироток)
// вызывается перед каждым приглашением
void reap_zombies(void);

#endif