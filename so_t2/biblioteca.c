
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

void print_str(char *s)
{
    while (*s != 0) {
        putchar(*s);
        s = s + 1;
    }
}

void puts(char *s)
{
    print_str(s);
    putchar(10);
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

// lê até o enter
int le_linha(char *buf, int max)
{
    int n = 0;
    int c = getchar();
    while (c != 10 && c >= 0) {
        if (n < max - 1) {
            buf[n] = c;
            n++;
        }
        c = getchar();
    }
    buf[n] = 0;
    return n;
}
