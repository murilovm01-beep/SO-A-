// usuario.h -- o que os programas podem usar
//
// os programas não usam strings nem variáveis globais: o mcc coloca as
// duas junto com o código, e com carga por demanda a página pode ainda
// não estar na memória quando o programa lê ela como dado

#ifndef USUARIO_H
#define USUARIO_H

#include "comum.h"

int so_le(void);
int so_escreve(int c);
int so_cria_proc(int prog, int arg);
int so_mata_proc(int pid);
int so_espera_proc(int pid);

int putchar(int c);
int getchar(void);
void print_int(int v);

#endif
