// biblioteca.c -- E/S de mais alto nível para os processos, sobre as
// chamadas de sistema SO_LE/SO_ESCREVE (adaptação de
// compilador_c/rt/biblioteca.c, que acessava as portas diretamente --
// coisa que um processo de usuário não pode mais fazer).

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
    int i;
    int neg;

    i = 0;
    neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    if (v == 0) {
        buf[0] = '0';
        i = 1;
    }
    while (v > 0) {
        buf[i] = (v % 10) + '0';
        i = i + 1;
        v = v / 10;
    }
    if (neg) {
        putchar('-');
    }
    while (i > 0) {
        i = i - 1;
        putchar(buf[i]);
    }
}

void print_hex(int v)
{
    char *digitos;
    int i;

    digitos = "0123456789ABCDEF";
    i = 12;
    while (i >= 0) {
        putchar(digitos[(v >> i) & 15]);
        i = i - 4;
    }
}

int le_linha(char *buf, int max)
{
    int n;
    int c;

    n = 0;
    for (;;) {
        c = getchar();
        if (c < 0 || c == 10) {
            break;
        }
        if (n < max - 1) {
            buf[n] = c;
            n = n + 1;
        }
    }
    buf[n] = 0;
    return n;
}
