// config.h -- configuração do SO. muda aqui e roda make de novo.

#define ESCALONADOR   ESC_ROUND_ROBIN   // ESC_SIMPLES, ESC_ROUND_ROBIN ou ESC_PRIORIDADE
#define QUANTUM       5                 // em interrupções do relógio
#define CARGA         CARGA_DEMANDA     // CARGA_TOTAL ou CARGA_DEMANDA
#define SUBSTITUICAO  SUBST_LRU         // SUBST_FIFO ou SUBST_LRU
#define QUADROS       24                // quadros de memória para os processos
#define CENARIO       CEN_TESTES        // CEN_TESTES, CEN_FASES1 ou CEN_FASES3
