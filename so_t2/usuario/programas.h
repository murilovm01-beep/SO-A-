// programas.h -- os programas que o SO sabe executar (ver a tabela
// "programas" em nucleo/so.c, que associa o nome usado em SO_CRIA_PROC à
// função). Todo programa é "void nome(int arg)": arg é o 2º argumento de
// SO_CRIA_PROC; quando a função retorna, o processo termina.

#ifndef PROGRAMAS_H
#define PROGRAMAS_H

void init(int cenario);      // primeiro processo; arg = cenário (comum.h)
void cpu(int n);             // limitado por CPU: n unidades de cálculo
void eco(int n);             // limitado por E/S: lê n linhas da console e as repete
void escritor(int n);        // escreve n linhas, com pouco cálculo entre elas
void proibido(int tipo);     // tenta E/S direta (0), halt (1) ou divisão por zero (2)
void laco(int x);            // laço infinito (para ser morto por outro processo)
void filho(int n);           // calcula um pouco e termina retornando de main
void pai(int n);             // cria n filhos e espera por eles

#endif
