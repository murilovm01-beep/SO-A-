// comum.h -- constantes usadas pelo SO e pelos programas

#ifndef COMUM_H
#define COMUM_H

// chamadas de sistema (número em r0)
#define SO_LE           1
#define SO_ESCREVE      2
#define SO_CRIA_PROC    7
#define SO_MATA_PROC    8
#define SO_ESPERA_PROC  9

// programas, na ordem em que estão no disco (ver o Makefile)
#define PROG_INIT      0
#define PROG_PROIBIDO  1
#define PROG_CONTA     2
#define PROG_FASES     3

// área de dados/pilha de todo processo: começa no endereço virtual 0x8000
#define PAG_DADOS   64
#define PAGS_DADOS  1

#define ESC_SIMPLES      1
#define ESC_ROUND_ROBIN  2
#define ESC_PRIORIDADE   3

#define CARGA_TOTAL    1   // parte I
#define CARGA_DEMANDA  2   // partes II e III

#define SUBST_FIFO  1
#define SUBST_LRU   2

#define CEN_TESTES   1
#define CEN_FASES1   2   // 1 processo fases
#define CEN_FASES3   3   // 3 processos fases ao mesmo tempo

#endif
