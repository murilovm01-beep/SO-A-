// comum.h -- constantes usadas pelo SO e pelos programas

#ifndef COMUM_H
#define COMUM_H

// números das chamadas de sistema (vão em r0 antes do trap 7)
#define SO_LE           1
#define SO_ESCREVE      2
#define SO_CRIA_PROC    7
#define SO_MATA_PROC    8
#define SO_ESPERA_PROC  9

// escalonadores
#define ESC_SIMPLES      1   // parte I
#define ESC_ROUND_ROBIN  2   // parte III
#define ESC_PRIORIDADE   3   // parte III

// cenários (o que o init faz)
#define CEN_TESTES  1
#define CEN_MISTO   2
#define CEN_CPU     3

#endif
