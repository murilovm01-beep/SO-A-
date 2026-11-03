// comum.h -- constantes compartilhadas entre o núcleo (nucleo/so.c) e os
// programas de usuário (usuario/*.c). Incluído pelos dois lados.

#ifndef COMUM_H
#define COMUM_H

// ------------------------------------------------- chamadas de sistema
//
// Convenção (a mesma da BIOS, ver bios_mancha/README.md): número da
// chamada em r0, argumentos em r1..r4, "trap 7"; o resultado volta em r0.
// Os números seguem a numeração usada no enunciado do trabalho.
#define SO_LE           1   // r0 = caractere lido (ou -1 em caso de erro)
#define SO_ESCREVE      2   // r1 = caractere; r0 = 0 (ok) ou -1
#define SO_CRIA_PROC    7   // r1 = nome do programa, r2 = argumento; r0 = pid do novo processo ou -1
#define SO_MATA_PROC    8   // r1 = pid (0 = o próprio processo); r0 = 0 ou -1
#define SO_ESPERA_PROC  9   // r1 = pid; r0 = 0 quando o processo morrer, -1 se não existe / é o próprio

// --------------------------------------------------------- escalonadores
#define ESC_SIMPLES      1  // Parte I: continua o atual se estiver pronto; senão o 1º pronto da tabela
#define ESC_ROUND_ROBIN  2  // Parte III: fila circular com quantum (preemptivo)
#define ESC_PRIORIDADE   3  // Parte III: maior prioridade (menor valor) entre os prontos, com quantum

// ------------------------------------------------------------- cenários
// (argumento que o núcleo passa para o processo "init")
#define CEN_TESTES   1      // testes funcionais de todas as chamadas e casos de erro
#define CEN_MISTO    2      // carga mista: 2 CPU-bound, 1 limitado por E/S, 1 "escritor"
#define CEN_CPU      3      // só processos CPU-bound, de tamanhos diferentes

#endif
