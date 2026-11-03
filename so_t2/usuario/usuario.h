// usuario.h -- interface dos programas de usuário com o SO.
//
// Os processos rodam em modo usuário (S=0) e sem permissão de E/S (I=0):
// tudo o que envolve a console passa pelas chamadas de sistema abaixo. É a
// "biblioteca C adaptada" de que fala o enunciado: as mesmas funções de
// compilador_c/inc/mancha.h (putchar, getchar, puts, print_int,
// print_hex), mas implementadas sobre o SO em vez de acessar as portas.

#ifndef USUARIO_H
#define USUARIO_H

#include "comum.h"

// chamadas de sistema (usuario.asm: carrega r0..r2 e faz "trap 7")
int so_le(void);                        // caractere lido, ou -1
int so_escreve(int c);                  // 0, ou -1
int so_cria_proc(char *nome, int arg);  // pid do novo processo, ou -1
int so_mata_proc(int pid);              // 0 = o próprio processo; devolve 0 ou -1
int so_espera_proc(int pid);            // 0 quando pid morrer; -1 se não existe/é o próprio

// biblioteca (biblioteca.c)
int putchar(int c);
int getchar(void);
void print_str(char *s);                // escreve a string (sem '\n')
void puts(char *s);                     // escreve a string e '\n'
void print_int(int v);
void print_hex(int v);
int le_linha(char *buf, int max);       // lê até '\n'; devolve o tamanho

// para os testes de proteção (usuario.asm): tentam fazer o que só o SO pode
void tenta_es_direta(void);             // "outb" direto na console
void tenta_halt(void);                  // "halt"

#endif
