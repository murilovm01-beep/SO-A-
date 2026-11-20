// init.c -- primeiro processo (o argumento é o CENARIO do config.h)

#include "usuario.h"

void roda(int prog, int arg)
{
    so_espera_proc(so_cria_proc(prog, arg));
}

void testes(void)
{
    int a;
    int b;
    int c;

    // cada um desses tem que ser morto pelo SO
    roda(PROG_PROIBIDO, 0);
    roda(PROG_PROIBIDO, 1);
    roda(PROG_PROIBIDO, 2);
    roda(PROG_PROIBIDO, 3);

    // o mesmo programa em 3 processos ao mesmo tempo
    a = so_cria_proc(PROG_CONTA, 0);
    b = so_cria_proc(PROG_CONTA, 1);
    c = so_cria_proc(PROG_CONTA, 2);
    so_espera_proc(a);
    so_espera_proc(b);
    so_espera_proc(c);
    putchar(10);
}

// o mesmo trabalho (12 rodadas do fases) em 1 processo ou dividido em 3
void memoria(int n)
{
    int pid[3];
    int i;
    for (i = 0; i < n; i++) {
        pid[i] = so_cria_proc(PROG_FASES, 12 / n);
    }
    for (i = 0; i < n; i++) {
        so_espera_proc(pid[i]);
    }
    putchar(10);
}

void main(int cenario)
{
    if (cenario == CEN_TESTES) {
        testes();
    } else if (cenario == CEN_FASES1) {
        memoria(1);
    } else {
        memoria(3);
    }
}
