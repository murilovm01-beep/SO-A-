// usuario.h -- o que os programas podem usar
// (eles rodam em modo usuário, então toda E/S tem que passar pelo SO)

#ifndef USUARIO_H
#define USUARIO_H

#include "comum.h"

// chamadas de sistema (usuario.asm)
int so_le(void);
int so_escreve(int c);
int so_cria_proc(char *nome, int arg);
int so_mata_proc(int pid);          // 0 = ele mesmo
int so_espera_proc(int pid);

// coisas que um processo NÃO pode fazer (pra testar a proteção)
void tenta_es_direta(void);
void tenta_halt(void);

// biblioteca.c (a do compilador_c, mas usando as chamadas de sistema)
int putchar(int c);
int getchar(void);
void print_str(char *s);
void puts(char *s);
void print_int(int v);
int le_linha(char *buf, int max);

#endif
