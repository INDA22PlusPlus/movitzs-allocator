#include <stdlib.h>
#include <stdio.h>
#include "molloch.c"

int main() {

    debug();

    void* x;


    for (int i = 0; i < 128 + 64; i++) {
        x = molloch(0x1);
        printf("%d\n", i);
    }

    debug();



    printf("%p\n", x);
    sacrifice(x);
    debug();


}