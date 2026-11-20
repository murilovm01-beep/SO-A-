// biblioteca.c -- montada junto com cada programa

#include "usuario.h"

int putchar(int c)
{
    so_escreve(c);
    return c;
}

int getchar(void)
{
    return so_le();
}

void print_int(int v)
{
    char buf[8];
    int i = 0;

    if (v < 0) {
        putchar('-');
        v = -v;
    }
    if (v == 0) {
        buf[0] = '0';
        i = 1;
    }
    while (v > 0) {
        buf[i] = (v % 10) + '0';
        i++;
        v = v / 10;
    }
    while (i > 0) {
        i--;
        putchar(buf[i]);
    }
}
