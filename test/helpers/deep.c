#include <stdio.h>

static volatile int depth = 0;

__attribute__((noinline)) void recur(void) {
    char buf[1024];  // 1 КБ на каждый вызов
    buf[0] = (char)(depth++);
    recur();  // бесконечная рекурсия
}

int main(void) {
    recur();
    return 0;
}