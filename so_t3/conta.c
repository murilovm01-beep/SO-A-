// conta.c -- roda em vários processos ao mesmo tempo (mesmo binário).
// todos usam o mesmo endereço virtual, mas cada um tem o seu quadro

#include "usuario.h"

void main(int n)
{
    int *x = (int *) 0x8000;
    int i;
    int j;

    *x = n;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 3000; j++) {
        }
        putchar('a' + *x);
        print_int(i);
        putchar(' ');
    }
}
