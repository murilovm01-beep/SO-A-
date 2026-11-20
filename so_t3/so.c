// so.c -- SO do T3: o do T2 com paginação
//
// cada processo tem a sua tabela de páginas (tabpag). os programas ficam
// no disco, 8 setores cada, e vão para quadros livres. código: páginas 0
// a n_pag-1, tipo 1. dados e pilha: página 64 (0x8000), tipo 3, ficam
// sempre na memória.

#include "comum.h"
#include "config.h"

// ponte.asm
void copia_quadro(int *destino, int *origem);
void k_putc(int c);
void k_imprime(char *s);
void k_dec(int v);
void k_hex(int v);
int console_tem_tecla(void);
int console_le_tecla(void);
int console_pronta(void);
int tiques(void);
int intervalo_relogio(void);
void prepara_relatorio(void);
void le_setor(int face, int trilha, int setor, int *destino);
void zera(int *p, int n);
int fim_do_so(void);

#define MAX_PROC     8
#define MAX_PID      32
#define MAX_QUADROS  64
#define TAM_VAGA     8      // setores de cada programa no disco
#define QUADRO_MAX   104    // 0xD000: dali pra cima fica a pilha do SO

#define LIVRE      0
#define PRONTO     1
#define BLOQUEADO  2
#define EXECUTANDO 3

#define ESPERA_LE      1
#define ESPERA_ESCREVE 2
#define ESPERA_PROC    3

// posições no quadro de 16 palavras
#define R0 0
#define R1 1
#define R2 2
#define SP 6
#define IP 7
#define SR 8
#define PT 12

struct processo {
    int estado;
    int pid;
    int quadro[16];
    int motivo;
    int dado;
    int prio;           // em milésimos, menor = melhor
    int desde;
    int desbloqueou;
    int prog;
    int n_pag;          // tamanho do programa em páginas (0 = ainda não sei)
};

// por pid, porque a entrada da tabela é reaproveitada
struct metricas {
    int prog;
    int arg;
    int criacao;
    int fim;
    int vezes[4];
    int tempo[4];
    int preempcoes;
    int resp_soma;
    int resp_n;
    int faltas;
};

// quadros de memória dados aos processos
struct quadro {
    int dono;           // índice na tabela de processos, -1 = livre
    int *entrada;       // entrada da tabela de páginas, se for página de código
    int carga;          // ordem de carga, pro FIFO
    int idade;          // pro LRU (só 8 bits, o mcc não tem unsigned)
};

#define N_PROGRAMAS 4
char *nome_prog[N_PROGRAMAS] = { "init", "proibido", "conta", "fases" };

struct processo tab[MAX_PROC];
int tabpag[MAX_PROC][256];
struct metricas met[MAX_PID];
struct quadro quadros[MAX_QUADROS];

int q_inicio;       // primeiro quadro físico dos processos
int n_quadros;
int n_cargas;

int atual;          // quem está executando (-1 = ninguém)
int prox_pid;
int fila[MAX_PROC];
int n_fila;
int quantum_restante;
int desligar;
int ocioso;
int ocioso_desde;

int n_criados;
int n_preempcoes;
int t_ocioso;
int n_int[16];
int n_cham[10];
int n_int_console;  // quem soma é o ponte.asm
int n_faltas;
int n_substituicoes;

char *nome_exc[5] = { "-", "escrita em pagina so de leitura", "instrucao privilegiada",
                      "divisao por zero", "instrucao ilegal" };

// ------------------------------------------------------------ estados

void muda_estado(struct processo *p, int novo)
{
    int agora = tiques();
    if (p->pid < MAX_PID) {
        met[p->pid].tempo[p->estado] += agora - p->desde;
        met[p->pid].vezes[novo]++;
    }
    p->estado = novo;
    p->desde = agora;
}

void fila_poe(int i)
{
    fila[n_fila] = i;
    n_fila++;
}

int fila_tira(int k)
{
    int i = fila[k];
    while (k < n_fila - 1) {
        fila[k] = fila[k + 1];
        k++;
    }
    n_fila--;
    return i;
}

void fila_remove(int i)
{
    int k;
    for (k = 0; k < n_fila; k++) {
        if (fila[k] == i) {
            fila_tira(k);
            return;
        }
    }
}

void fica_pronto(int i)
{
    muda_estado(&tab[i], PRONTO);
    fila_poe(i);
}

// ------------------------------------------------------------ memória

int *endereco(int f)
{
    return (int *) ((q_inicio + f) * 512);
}

// tira a página do quadro f da tabela do dono. só sai página de código,
// que é tipo 1 e já está no disco, então não precisa escrever de volta
void despeja(int f)
{
    struct quadro *q = &quadros[f];
    q->entrada[0] = 0;
    q->entrada[128] = 0;
    q->dono = -1;
    q->entrada = 0;
    n_substituicoes++;
}

// FIFO: a carregada há mais tempo. LRU: a de menor idade.
// página de dados não entra: se ela saísse, a falta seria de dado e essa
// não tem como tratar
int escolhe_vitima(void)
{
    struct quadro *q = quadros;
    int f;
    int x;
    int v = -1;
    int menor = 0;
    for (f = 0; f < n_quadros; f++) {
        if (q->entrada != 0) {
            if (SUBSTITUICAO == SUBST_FIFO) {
                x = q->carga;
            } else {
                // usada depois do último tique conta como mais recente.
                // passo o bit pra idade e zero ele, pra próxima falta ver
                // o que foi usado desde esta
                x = q->idade;
                if (*q->entrada & 0x2000) {
                    x = x + 256;
                    q->idade = q->idade | 128;
                    *q->entrada = *q->entrada & 0xDFFF;
                }
            }
            if (v < 0 || x < menor) {
                v = f;
                menor = x;
            }
        }
        q++;
    }
    return v;
}

int pega_quadro(int i)
{
    int f;
    for (f = 0; f < n_quadros; f++) {
        if (quadros[f].dono < 0) {
            break;
        }
    }
    if (f == n_quadros) {
        if (CARGA == CARGA_TOTAL) {
            return -1;
        }
        f = escolhe_vitima();
        if (f < 0) {
            return -1;
        }
        despeja(f);
    }
    n_cargas++;
    quadros[f].dono = i;
    quadros[f].entrada = 0;
    quadros[f].carga = n_cargas;
    quadros[f].idade = 128;
    return f;
}

void libera_quadros(int i)
{
    int f;
    for (f = 0; f < n_quadros; f++) {
        if (quadros[f].dono == i) {
            quadros[f].dono = -1;
            quadros[f].entrada = 0;
        }
    }
}

// lê do disco a página pag do programa do processo i
int carrega_pagina(int i, int pag)
{
    int f = pega_quadro(i);
    int s = tab[i].prog * TAM_VAGA + pag;
    int *fis;
    if (f < 0) {
        return 0;
    }
    fis = endereco(f);
    le_setor(s / 720, s / 18 % 40, s % 18, fis);
    // tipo 1 (só leitura) no código e também nos dados, pra o programa
    // poder ler o próprio código mas não escrever nele
    tabpag[i][pag] = 0x4000 | (q_inicio + f);
    tabpag[i][pag + 128] = 0x4000 | (q_inicio + f);
    quadros[f].entrada = &tabpag[i][pag];
    if (pag == 0) {
        tab[i].n_pag = (fis[2] + 511) / 512;    // o inicio.asm guarda o tamanho ali
    }
    return 1;
}

// LRU: a cada tique a idade anda um bit pra direita e ganha o bit 7 se a
// página foi usada (bit de acesso da entrada, que a cpu liga sozinha)
void envelhece(void)
{
    struct quadro *q = quadros;
    int f;
    for (f = 0; f < n_quadros; f++) {
        if (q->entrada != 0) {
            q->idade = q->idade >> 1;
            if (*q->entrada & 0x2000) {
                q->idade = q->idade | 128;
                *q->entrada = *q->entrada & 0xDFFF;
            }
        }
        q++;
    }
}

// instrução de 2 palavras que começa no fim de uma página: se a página
// seguinte não está na memória, a falta é na 2ª palavra e o ip salvo fica
// no meio da instrução. pra saber, volto na página anterior até achar uma
// palavra que não pode ser começo de instrução de 2 palavras (regra do
// instrucao.c do simulador): andou um número par = atravessou
int duas_palavras(int w)
{
    int formato = w >> 14;
    int mod = (w >> 3) & 7;
    if (formato == 3) {
        return 0;
    }
    if (formato == 2 && ((w >> 10) & 15) != 8) {    // no formato 2 só o jmpc tem mod
        return 0;
    }
    return mod >= 4 && mod <= 6;
}

int atravessou(int *pt, int ip)
{
    int pag = ip >> 9;
    int *fim;
    int k = 1;
    if ((ip & 511) != 0 || pag == 0 || pt[pag - 1] == 0) {
        return 0;
    }
    fim = (int *) (((pt[pag - 1] & 0x1FFF) + 1) * 512);
    while (k < 256 && duas_palavras(*(fim - k))) {
        k++;
    }
    return k % 2 == 0;
}

// -------------------------------------------------------- processos

int acha_pid(int pid)
{
    int i;
    for (i = 0; i < MAX_PROC; i++) {
        if (tab[i].estado != LIVRE && tab[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

int cria_processo(int prog, int arg)
{
    int i;
    int k;
    int f;
    int *pt;
    int *fis;
    struct processo *p;
    struct metricas *m;

    if (prog < 0 || prog >= N_PROGRAMAS) {
        return -1;
    }
    for (i = 0; i < MAX_PROC; i++) {
        if (tab[i].estado == LIVRE) {
            break;
        }
    }
    if (i == MAX_PROC) {
        return -1;
    }
    p = &tab[i];
    p->prog = prog;
    p->n_pag = 0;

    pt = tabpag[i];
    zera(pt, 256);

    for (k = 0; k < PAGS_DADOS; k++) {
        f = pega_quadro(i);
        if (f < 0) {
            libera_quadros(i);
            return -1;
        }
        fis = endereco(f);
        zera(fis, 256);
        pt[128 + PAG_DADOS + k] = 0xC000 | (q_inicio + f);
    }
    // o argumento fica no topo da pilha, o main acha ele em bp+4
    fis[255] = arg;

    if (CARGA == CARGA_TOTAL) {
        k = carrega_pagina(i, 0);
        for (f = 1; f < p->n_pag && k; f++) {
            k = carrega_pagina(i, f);
        }
        if (!k) {
            libera_quadros(i);
            return -1;
        }
    }

    for (k = 0; k < 16; k++) {
        p->quadro[k] = 0;
    }
    p->quadro[IP] = 0;
    p->quadro[SP] = (PAG_DADOS + PAGS_DADOS) * 512 - 2;
    p->quadro[SR] = 0x4000;         // S=0, P=1 (paginação), I=0
    p->quadro[PT] = (int) pt;

    p->pid = prox_pid;
    prox_pid++;
    p->prio = 500;
    p->desbloqueou = -1;
    p->estado = LIVRE;

    if (p->pid < MAX_PID) {
        m = &met[p->pid];
        m->prog = prog;
        m->arg = arg;
        m->criacao = tiques();
        m->fim = -1;
        for (k = 0; k < 4; k++) {
            m->vezes[k] = 0;
            m->tempo[k] = 0;
        }
        m->preempcoes = 0;
        m->resp_soma = 0;
        m->resp_n = 0;
        m->faltas = 0;
    }

    fica_pronto(i);
    n_criados++;
    return p->pid;
}

void desbloqueia(int i, int valor)
{
    tab[i].quadro[R0] = valor;
    fica_pronto(i);
    tab[i].desbloqueou = tiques();
}

void mata_processo(int i)
{
    int pid = tab[i].pid;
    int j;

    if (tab[i].estado == PRONTO) {
        fila_remove(i);
    }
    muda_estado(&tab[i], LIVRE);
    libera_quadros(i);
    if (pid < MAX_PID) {
        met[pid].fim = tiques();
    }
    if (i == atual) {
        atual = -1;
    }
    for (j = 0; j < MAX_PROC; j++) {
        if (tab[j].estado == BLOQUEADO && tab[j].motivo == ESPERA_PROC && tab[j].dado == pid) {
            desbloqueia(j, 0);
        }
    }
    if (pid == 1) {
        desligar = 1;
    }
}

// prio = (prio + t_exec/t_quantum) / 2, em milésimos
void recalcula_prio(struct processo *p)
{
    int t_exec = QUANTUM - quantum_restante;
    p->prio = (p->prio + t_exec * 1000 / QUANTUM) / 2;
}

void bloqueia(int motivo, int dado)
{
    struct processo *p = &tab[atual];
    if (ESCALONADOR == ESC_PRIORIDADE) {
        recalcula_prio(p);
    }
    p->motivo = motivo;
    p->dado = dado;
    muda_estado(p, BLOQUEADO);
    atual = -1;
}

void preempta(void)
{
    struct processo *p = &tab[atual];
    if (ESCALONADOR == ESC_PRIORIDADE) {
        recalcula_prio(p);
    }
    if (p->pid < MAX_PID) {
        met[p->pid].preempcoes++;
    }
    n_preempcoes++;
    fica_pronto(atual);
    atual = -1;
}

void morre(char *motivo)
{
    k_imprime("[SO] pid ");
    k_dec(tab[atual].pid);
    k_imprime(" morto: ");
    k_imprime(motivo);
    k_imprime(" (ip=");
    k_hex(tab[atual].quadro[IP]);
    k_imprime(")\n");
    mata_processo(atual);
}

// ------------------------------------------------ chamadas de sistema

void chamada_le(struct processo *p)
{
    if (console_tem_tecla()) {
        p->quadro[R0] = console_le_tecla();
    } else {
        bloqueia(ESPERA_LE, 0);
    }
}

void chamada_escreve(struct processo *p)
{
    if (console_pronta()) {
        k_putc(p->quadro[R1]);
        p->quadro[R0] = 0;
    } else {
        bloqueia(ESPERA_ESCREVE, p->quadro[R1]);
    }
}

void chamada_mata(struct processo *p)
{
    int pid = p->quadro[R1];
    int i;
    if (pid == 0) {
        pid = p->pid;
    }
    i = acha_pid(pid);
    if (i < 0) {
        p->quadro[R0] = -1;
        return;
    }
    p->quadro[R0] = 0;
    mata_processo(i);
}

void chamada_espera(struct processo *p)
{
    int pid = p->quadro[R1];
    int i = acha_pid(pid);
    if (pid == p->pid || pid <= 0 || pid >= prox_pid) {
        p->quadro[R0] = -1;
    } else if (i < 0) {
        p->quadro[R0] = 0;      // já morreu
    } else {
        bloqueia(ESPERA_PROC, pid);
    }
}

void trata_chamada(void)
{
    struct processo *p = &tab[atual];
    int num = p->quadro[R0];
    n_cham[num]++;
    if (num == SO_LE) {
        chamada_le(p);
    } else if (num == SO_ESCREVE) {
        chamada_escreve(p);
    } else if (num == SO_CRIA_PROC) {
        p->quadro[R0] = cria_processo(p->quadro[R1], p->quadro[R2]);
    } else if (num == SO_MATA_PROC) {
        chamada_mata(p);
    } else if (num == SO_ESPERA_PROC) {
        chamada_espera(p);
    } else {
        p->quadro[R0] = -1;
    }
}

// ---------------------------------------------------- interrupções

void trata_relogio(void)
{
    if (SUBSTITUICAO == SUBST_LRU) {
        envelhece();
    }
    if (atual < 0 || ESCALONADOR == ESC_SIMPLES) {
        return;
    }
    quantum_restante--;
    if (quantum_restante <= 0) {
        preempta();
    }
}

// interrupção 5. só a falta na busca de instrução dá pra atender, porque
// aí o ip salvo é o endereço que faltou
void trata_falta(void)
{
    struct processo *p = &tab[atual];
    int *pt = tabpag[atual];
    int ip = p->quadro[IP];
    int pag = ip >> 9;

    if (CARGA == CARGA_TOTAL) {
        morre("falta de pagina");
    } else if (pt[pag] != 0) {
        // a página do ip está na memória, então faltou num acesso a dado
        morre("acesso a dado fora da area do processo");
    } else if (pag >= PAG_DADOS || (p->n_pag > 0 && pag >= p->n_pag)) {
        morre("codigo fora do programa");
    } else {
        if (atravessou(pt, ip)) {
            p->quadro[IP] = ip - 2;
        }
        if (!carrega_pagina(atual, pag)) {
            morre("sem quadro para carregar a pagina");
            return;
        }
        n_faltas++;
        if (p->pid < MAX_PID) {
            met[p->pid].faltas++;
        }
    }
}

void trata_pendencias(void)
{
    struct processo *p = tab;
    int i;
    for (i = 0; i < MAX_PROC; i++) {
        if (p->estado == BLOQUEADO) {
            if (p->motivo == ESPERA_LE && console_tem_tecla()) {
                desbloqueia(i, console_le_tecla());
            } else if (p->motivo == ESPERA_ESCREVE && console_pronta()) {
                k_putc(p->dado);
                desbloqueia(i, 0);
            }
        }
        p++;
    }
}

// ------------------------------------------------------- escalonador

void despacha(int i)
{
    struct processo *p = &tab[i];
    int pid = p->pid;
    if (p->desbloqueou >= 0) {
        if (pid < MAX_PID) {
            met[pid].resp_soma += tiques() - p->desbloqueou;
            met[pid].resp_n++;
        }
        p->desbloqueou = -1;
    }
    muda_estado(p, EXECUTANDO);
    atual = i;
    quantum_restante = QUANTUM;
}

void so_escalona(void)
{
    int k = 0;
    int j;
    if (atual >= 0 || n_fila == 0) {
        return;
    }
    if (ESCALONADOR == ESC_SIMPLES) {
        for (j = 1; j < n_fila; j++) {
            if (fila[j] < fila[k]) {
                k = j;
            }
        }
    } else if (ESCALONADOR == ESC_PRIORIDADE) {
        for (j = 1; j < n_fila; j++) {
            if (tab[fila[j]].prio < tab[fila[k]].prio) {
                k = j;
            }
        }
    }
    despacha(fila_tira(k));
}

// ------------------------------------------------------- relatório
// a console do simulador só mostra 6 linhas: imprimo de 5 em 5 e espero um E

int linhas;

void fim_linha(void)
{
    k_putc('\n');
    linhas++;
    if (linhas == 5) {
        while (console_tem_tecla()) {
            console_le_tecla();
        }
        k_imprime("      (E para continuar)");
        while (!console_tem_tecla()) {
        }
        while (console_tem_tecla()) {
            console_le_tecla();
        }
        k_putc('\n');
        linhas = 0;
    }
}

void num(int v, int largura)
{
    int d = 1;
    int x = v;
    if (x < 0) {
        d++;
        x = -x;
    }
    while (x >= 10) {
        x = x / 10;
        d++;
    }
    while (d < largura) {
        k_putc(' ');
        d++;
    }
    k_dec(v);
}

void texto(char *s, int largura)
{
    int n = 0;
    while (s[n] != 0) {
        n++;
    }
    k_imprime(s);
    while (n < largura) {
        k_putc(' ');
        n++;
    }
}

void mostra_config(void)
{
    if (ESCALONADOR == ESC_SIMPLES) {
        k_imprime("simples");
    } else if (ESCALONADOR == ESC_ROUND_ROBIN) {
        k_imprime("round-robin");
    } else {
        k_imprime("prioridade");
    }
    k_imprime(", quantum ");
    k_dec(QUANTUM);
    if (CARGA == CARGA_TOTAL) {
        k_imprime(", carga total, ");
    } else if (SUBSTITUICAO == SUBST_FIFO) {
        k_imprime(", demanda FIFO, ");
    } else {
        k_imprime(", demanda LRU, ");
    }
    k_dec(n_quadros);
    k_imprime(" quadros");
}

void relatorio(void)
{
    int pid;
    int i;
    int x;
    int agora = tiques();
    struct metricas *m;

    prepara_relatorio();
    linhas = 0;

    for (i = 0; i < MAX_PROC; i++) {
        if (tab[i].estado != LIVRE && tab[i].pid < MAX_PID) {
            met[tab[i].pid].tempo[tab[i].estado] += agora - tab[i].desde;
        }
    }

    k_imprime("===== metricas (tempo em tiques, 1 tique = ");
    k_dec(intervalo_relogio());
    k_imprime(" instrucoes) =====");
    fim_linha();
    mostra_config();
    fim_linha();
    k_imprime("processos ");
    k_dec(n_criados);
    k_imprime(", tempo total ");
    k_dec(agora);
    k_imprime(", ocioso ");
    k_dec(t_ocioso);
    k_imprime(", preempcoes ");
    k_dec(n_preempcoes);
    k_imprime(", faltas ");
    k_dec(n_faltas);
    k_imprime(", substituicoes ");
    k_dec(n_substituicoes);
    fim_linha();
    k_imprime("interrupcoes: relogio ");
    k_dec(n_int[10]);
    k_imprime(", console ");
    k_dec(n_int_console);
    k_imprime(", trap 7 ");
    k_dec(n_int[7]);
    k_imprime(", pagina ");
    k_dec(n_int[5]);
    k_imprime(", outras ");
    k_dec(n_int[1] + n_int[2] + n_int[3] + n_int[4]);
    fim_linha();
    k_imprime("chamadas: le ");
    k_dec(n_cham[SO_LE]);
    k_imprime(", escreve ");
    k_dec(n_cham[SO_ESCREVE]);
    k_imprime(", cria ");
    k_dec(n_cham[SO_CRIA_PROC]);
    k_imprime(", mata ");
    k_dec(n_cham[SO_MATA_PROC]);
    k_imprime(", espera ");
    k_dec(n_cham[SO_ESPERA_PROC]);
    fim_linha();

    // em dois pedaços: string muito grande não cabe numa linha do montador
    k_imprime("pid prog     arg   ret  pre   nP  nB   nE");
    k_imprime("    tP    tB    tE  resp   falt");
    fim_linha();
    for (pid = 1; pid < prox_pid && pid < MAX_PID; pid++) {
        m = &met[pid];
        num(pid, 3);
        k_putc(' ');
        texto(nome_prog[m->prog], 8);
        num(m->arg, 4);
        if (m->fim >= 0) {
            num(m->fim - m->criacao, 6);
        } else {
            k_imprime("     -");
        }
        num(m->preempcoes, 5);
        num(m->vezes[PRONTO], 5);
        num(m->vezes[BLOQUEADO], 4);
        num(m->vezes[EXECUTANDO], 5);
        num(m->tempo[PRONTO], 6);
        num(m->tempo[BLOQUEADO], 6);
        num(m->tempo[EXECUTANDO], 6);
        if (m->resp_n > 0) {
            x = m->resp_soma * 10 / m->resp_n;
            num(x / 10, 4);
            k_putc('.');
            k_dec(x % 10);
        } else {
            k_imprime("     -");
        }
        num(m->faltas, 7);
        fim_linha();
    }
    k_imprime("fim");
}

// ------------------------------------------------- entrada do SO

void salva_quadro(int *quadro)
{
    if (atual >= 0) {
        copia_quadro(tab[atual].quadro, quadro);
    }
}

void recupera_quadro(int *quadro)
{
    copia_quadro(quadro, tab[atual].quadro);
}

// chamada pelo ponte.asm em toda interrupção.
// devolve 0 = rete no quadro, 1 = fica ocioso, 2 = desliga
int so_trata_interrupcao(int irq, int *quadro)
{
    if (ocioso) {
        t_ocioso += tiques() - ocioso_desde;
        ocioso = 0;
    }
    n_int[irq]++;

    if (irq < 7 && (atual < 0 || (quadro[SR] & 0x8000))) {
        k_imprime("[SO] PANICO: excecao dentro do SO\n");
        return 2;
    }

    salva_quadro(quadro);

    if (irq == 7) {
        trata_chamada();
    } else if (irq == 10) {
        trata_relogio();
    } else if (irq == 5) {
        trata_falta();
    } else {
        morre(nome_exc[irq]);
    }

    if (desligar) {
        relatorio();
        return 2;
    }

    trata_pendencias();
    so_escalona();

    if (atual < 0) {
        ocioso = 1;
        ocioso_desde = tiques();
        return 1;
    }
    recupera_quadro(quadro);
    return 0;
}

int so_inicia(int *quadro)
{
    int i;
    for (i = 0; i < MAX_PROC; i++) {
        tab[i].estado = LIVRE;
    }
    for (i = 0; i < 16; i++) {
        n_int[i] = 0;
    }
    for (i = 0; i < 10; i++) {
        n_cham[i] = 0;
    }
    atual = -1;
    prox_pid = 1;
    n_fila = 0;
    desligar = 0;
    ocioso = 0;
    t_ocioso = 0;
    n_criados = 0;
    n_preempcoes = 0;
    n_int_console = 0;
    n_faltas = 0;
    n_substituicoes = 0;
    n_cargas = 0;

    // os quadros dos processos começam logo depois do fim do SO
    q_inicio = (fim_do_so() >> 9) + 1;
    n_quadros = QUADROS;
    if (q_inicio + n_quadros > QUADRO_MAX) {
        n_quadros = QUADRO_MAX - q_inicio;
    }
    for (i = 0; i < n_quadros; i++) {
        quadros[i].dono = -1;
    }

    k_imprime("[SO] ");
    mostra_config();
    k_putc('\n');

    cria_processo(PROG_INIT, CENARIO);
    so_escalona();
    recupera_quadro(quadro);
    return 0;
}
