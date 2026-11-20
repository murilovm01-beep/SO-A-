// fases.c -- programa grande (várias páginas de código) pra testar a
// substituição de páginas. cada fase é uma conta diferente, fica num
// pedaço diferente do código e roda um tempo antes de passar pra próxima.
// todas usam mistura(), que fica na página 0, então a página 0 é usada o
// tempo todo

#include "usuario.h"

int mistura(int a, int b)
{
    return (a * 31 + b) % 1009;
}

int quadrados(int n);
int primos(int n);
int fibonacci(int n);
int ordena(int n);
int collatz(int n);
int inverte(int n);

void main(int rodadas)
{
    int r;
    int x = 0;

    for (r = 0; r < rodadas; r++) {
        x = mistura(x, quadrados(500));
        x = mistura(x, primos(250));
        x = mistura(x, fibonacci(30));
        x = mistura(x, ordena(3));
        x = mistura(x, collatz(30));
        x = mistura(x, inverte(400));
        putchar('a' + r);
    }
    putchar('=');
    print_int(x);
    putchar(' ');
}

int quadrados(int n)
{
    int i;
    int s = 0;
    for (i = 1; i <= n; i++) {
        s = mistura(s, (i % 100) * (i % 100));
    }
    return s;
}

int primos(int n)
{
    int i;
    int d;
    int conta = 0;
    int eh;
    for (i = 2; i <= n; i++) {
        eh = 1;
        d = 2;
        while (d * d <= i && eh) {
            if (i % d == 0) {
                eh = 0;
            }
            d++;
        }
        if (eh) {
            conta = mistura(conta, i);
        }
    }
    return conta;
}

int fibonacci(int n)
{
    int a;
    int b;
    int c;
    int i;
    int k;
    int s = 0;
    for (k = 0; k < n; k++) {
        a = 0;
        b = 1;
        for (i = 0; i < 20; i++) {
            c = mistura(a, b);
            a = b;
            b = c;
        }
        s = mistura(s, b);
    }
    return s;
}

int ordena(int n)
{
    int v[20];
    int i;
    int j;
    int t;
    int k;
    int s = 0;
    for (k = 0; k < n; k++) {
        for (i = 0; i < 20; i++) {
            v[i] = mistura(i, k) % 50;
        }
        for (i = 0; i < 19; i++) {
            for (j = 0; j < 19 - i; j++) {
                if (v[j] > v[j + 1]) {
                    t = v[j];
                    v[j] = v[j + 1];
                    v[j + 1] = t;
                }
            }
        }
        s = mistura(s, v[0] + v[19]);
    }
    return s;
}

int collatz(int n)
{
    int i;
    int x;
    int passos = 0;
    for (i = 1; i <= n; i++) {
        x = i;
        while (x != 1) {
            if (x % 2 == 0) {
                x = x / 2;
            } else {
                x = 3 * x + 1;
            }
            passos = mistura(passos, x % 100);
        }
    }
    return passos;
}

int inverte(int n)
{
    int i;
    int x;
    int r;
    int pal = 0;
    for (i = 1; i <= n; i++) {
        x = i;
        r = 0;
        while (x > 0) {
            r = r * 10 + x % 10;
            x = x / 10;
        }
        if (r == i) {
            pal = mistura(pal, i);
        }
    }
    return pal;
}
