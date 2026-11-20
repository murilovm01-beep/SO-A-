// proibido.c -- tenta acessar memória que não é dele (o SO tem que matar)

#include "usuario.h"

void afunda(int n)
{
    afunda(n + 1);
}

void main(int tipo)
{
    int *p;
    void (*f)(void);

    if (tipo == 0) {
        p = (int *) 0;          // código do próprio programa: só leitura
        *p = 1;
    } else if (tipo == 1) {
        p = (int *) 0x9000;     // depois da área de dados
        tipo = *p;
    } else if (tipo == 2) {
        f = 0x3000;             // página de código que o programa não tem
        f();
    } else {
        afunda(0);              // pilha passa do começo da área de dados
    }
}
