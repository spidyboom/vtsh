#include <stdio.h>

static volatile int depth = 0;

__attribute__((noinline)) void recur(void) {
    volatile char buf[1024]; 
    buf[0] = (char)(depth++);
    recur();
}
int main(void) {
    recur();
    return 0;
}