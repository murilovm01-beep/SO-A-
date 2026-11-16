
#include "comum.h"
#include "config.h"
#include "programas.h"

// --- do ponte.asm
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
// do usuario.asm: pra onde o main de cada processo "retorna"
void proc_retorno(void);

#define MAX_PROC   8      // tamanho da tabela de processos
#define TAM_PILHA  128    // pilha de cada processo (em int)
#define MAX_PID    32     // só guardo métricas dos pids até aqui

// estados (também são os índices dos vetores de métricas)
#define LIVRE      0
#define PRONTO     1
#define BLOQUEADO  2
#define EXECUTANDO 3

// motivos de bloqueio
#define ESPERA_LE      1
#define ESPERA_ESCREVE 2
#define ESPERA_PROC    3

// posições no quadro de 16 palavras (README da bios)
#define R0 0
#define R1 1
#define R2 2
#define SP 6
#define IP 7
#define SR 8

struct processo {
    int estado;
    int pid;
    int quadro[16];     // os registradores salvos
    int motivo;         // por que está bloqueado
    int dado;           // pid que ele espera, ou o caractere que quer escrever
    int prio;           // prioridade em milésimos (o mcc não tem float). menor = melhor
    int desde;          // tique em que entrou no estado atual
    int desbloqueou;    // tique em que desbloqueou, -1 se não está esperando a CPU
};


struct metricas {
    int prog;
    int arg;
    int criacao;
    int fim;            // -1 enquanto está vivo
    int vezes[4];       // quantas vezes entrou em cada estado
    int tempo[4];       // quanto tempo ficou em cada estado
    int preempcoes;
    int resp_soma;      // soma dos tempos de resposta
    int resp_n;
};

// programas que dá pra criar com SO_CRIA_PROC (ficam em programas.c)
struct programa {
    char *nome;
    void (*codigo)(int);
};

#define N_PROGRAMAS 7
struct programa programas[N_PROGRAMAS] = {
    { "init", init },
    { "cpu", cpu },
    { "eco", eco },
    { "proibido", proibido },
    { "laco", laco },
    { "filho", filho },
    { "pai", pai }
};

struct processo tab[MAX_PROC];
int pilhas[MAX_PROC][TAM_PILHA];
struct metricas met[MAX_PID];

int atual;          // índice na tabela de quem está executando (-1 = ninguém)
int prox_pid;
int fila[MAX_PROC]; // fila de prontos
int n_fila;
int quantum_restante;
int desligar;
int ocioso;
int ocioso_desde;

// métricas do sistema
int n_criados;
int n_preempcoes;
int t_ocioso;
int n_int[16];          // interrupções de cada tipo
int n_cham[10];         // chamadas de sistema de cada tipo
int n_int_console;      // quem soma é o ponte.asm (a console é da bios)

char *nome_exc[5] = { "-", "violacao de segmento", "instrucao privilegiada",
                      "divisao por zero", "instrucao ilegal" };


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

// tira o k-ésimo da fila e devolve
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

// pronto sempre vai pro fim da fila
void fica_pronto(int i)
{
    muda_estado(&tab[i], PRONTO);
    fila_poe(i);
}

// processos

int compara(char *a, char *b)
{
    int i = 0;
    while (a[i] == b[i]) {
        if (a[i] == 0) {
            return 1;
        }
        i++;
    }
    return 0;
}

int acha_programa(char *nome)
{
    int i;
    for (i = 0; i < N_PROGRAMAS; i++) {
        if (compara(nome, programas[i].nome)) {
            return i;
        }
    }
    return -1;
}

// índice na tabela do processo com esse pid, ou -1
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
    int *topo;
    struct processo *p;
    struct metricas *m;

    for (i = 0; i < MAX_PROC; i++) {
        if (tab[i].estado == LIVRE) {
            break;
        }
    }
    if (i == MAX_PROC) {
        return -1;     // tabela cheia
    }
    p = &tab[i];

    topo = &pilhas[i][TAM_PILHA - 2];
    topo[0] = (int) proc_retorno;
    topo[1] = arg;

    for (k = 0; k < 16; k++) {
        p->quadro[k] = 0;
    }
    p->quadro[SP] = (int) topo;
    p->quadro[IP] = (int) programas[prog].codigo;
    p->quadro[SR] = 0;   // S=0 (modo usuário), I=0 (sem in/out), D=0 (interrupções ligadas)

    p->pid = prox_pid;
    prox_pid++;
    p->prio = 500;       // 0,5
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
    }

    fica_pronto(i);
    n_criados++;
    return p->pid;
}

void desbloqueia(int i, int valor)
{
    tab[i].quadro[R0] = valor;     // o que a chamada vai devolver
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
    if (pid < MAX_PID) {
        met[pid].fim = tiques();
    }
    if (i == atual) {
        atual = -1;
    }

    // acorda quem estava esperando ele morrer
    for (j = 0; j < MAX_PROC; j++) {
        if (tab[j].estado == BLOQUEADO && tab[j].motivo == ESPERA_PROC && tab[j].dado == pid) {
            desbloqueia(j, 0);
        }
    }

    if (pid == 1) {
        desligar = 1;    // o init morreu
    }
}

// prio = (prio + t_exec/t_quantum) / 2
// t_exec = quantum - o que sobrou. como é em milésimos fica *1000
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

// acabou o quantum: vai pro fim da fila
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


void chamada_le(struct processo *p)
{
    if (console_tem_tecla()) {
        p->quadro[R0] = console_le_tecla();
    } else {
        bloqueia(ESPERA_LE, 0);     // termina nas pendências
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

void chamada_cria(struct processo *p)
{
    int prog = acha_programa((char *) p->quadro[R1]);
    if (prog < 0) {
        p->quadro[R0] = -1;
    } else {
        p->quadro[R0] = cria_processo(prog, p->quadro[R2]);
    }
}

void chamada_mata(struct processo *p)
{
    int pid = p->quadro[R1];
    int i;
    if (pid == 0) {
        pid = p->pid;           // 0 = ele mesmo
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
        p->quadro[R0] = -1;     // ele mesmo, ou pid que nunca existiu
    } else if (i < 0) {
        // já morreu: devolvo 0 sem bloquear. primeiro eu devolvia -1, mas
        // com prioridade o filho às vezes terminava antes do pai chamar o
        // espera e o pai recebia erro
        p->quadro[R0] = 0;
    } else {
        bloqueia(ESPERA_PROC, pid);   // mata_processo() desbloqueia
    }
}

void trata_chamada(void)
{
    struct processo *p = &tab[atual];
    int num = p->quadro[R0];
    n_cham[num]++;      // a bios só manda pra cá os números da tabela (1 a 9)
    if (num == SO_LE) {
        chamada_le(p);
    } else if (num == SO_ESCREVE) {
        chamada_escreve(p);
    } else if (num == SO_CRIA_PROC) {
        chamada_cria(p);
    } else if (num == SO_MATA_PROC) {
        chamada_mata(p);
    } else if (num == SO_ESPERA_PROC) {
        chamada_espera(p);
    } else {
        p->quadro[R0] = -1;
    }
}


void trata_relogio(void)
{
    if (atual < 0 || ESCALONADOR == ESC_SIMPLES) {
        return;
    }
    quantum_restante--;
    if (quantum_restante <= 0) {
        preempta();
    }
}

void trata_excecao(int irq)
{
    k_imprime("[SO] pid ");
    k_dec(tab[atual].pid);
    k_imprime(" morto: ");
    k_imprime(nome_exc[irq]);
    k_imprime(" (ip=");
    k_hex(tab[atual].quadro[IP]);
    k_imprime(")\n");
    mata_processo(atual);
}

void trata_pendencias(void)
{
    int i;
    struct processo *p;
    for (i = 0; i < MAX_PROC; i++) {
        p = &tab[i];
        if (p->estado != BLOQUEADO) {
            continue;
        }
        if (p->motivo == ESPERA_LE && console_tem_tecla()) {
            desbloqueia(i, console_le_tecla());
        } else if (p->motivo == ESPERA_ESCREVE && console_pronta()) {
            k_putc(p->dado);
            desbloqueia(i, 0);
        }
    }
}


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
        // o primeiro pronto na ordem da tabela
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

// número alinhado à direita
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

char *nome_esc(void)
{
    if (ESCALONADOR == ESC_SIMPLES) {
        return "simples";
    }
    if (ESCALONADOR == ESC_ROUND_ROBIN) {
        return "round-robin";
    }
    return "prioridade";
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

    k_imprime("===== metricas (tempos em tiques do relogio) =====");
    fim_linha();
    k_imprime(nome_esc());
    k_imprime(", quantum ");
    k_dec(QUANTUM);
    k_imprime(", 1 tique = ");
    k_dec(intervalo_relogio());
    k_imprime(" instrucoes");
    fim_linha();
    k_imprime("processos criados ");
    k_dec(n_criados);
    k_imprime(", tempo total ");
    k_dec(agora);
    k_imprime(", ocioso ");
    k_dec(t_ocioso);
    k_imprime(", preempcoes ");
    k_dec(n_preempcoes);
    fim_linha();
    k_imprime("interrupcoes: relogio ");
    k_dec(n_int[10]);
    k_imprime(", console ");
    k_dec(n_int_console);
    k_imprime(", trap 7 ");
    k_dec(n_int[7]);
    k_imprime(", excecoes ");
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

    // n = vezes que entrou no estado, t = tempo no estado
    // P = pronto, B = bloqueado, E = executando
    k_imprime("pid prog     arg  ret pre  nP nB nE  tP  tB  tE resp");
    fim_linha();
    for (pid = 1; pid < prox_pid && pid < MAX_PID; pid++) {
        m = &met[pid];
        num(pid, 3);
        k_putc(' ');
        texto(programas[m->prog].nome, 8);
        num(m->arg, 4);
        if (m->fim >= 0) {
            num(m->fim - m->criacao, 5);
        } else {
            k_imprime("    -");
        }
        num(m->preempcoes, 4);
        num(m->vezes[PRONTO], 4);
        num(m->vezes[BLOQUEADO], 3);
        num(m->vezes[EXECUTANDO], 3);
        num(m->tempo[PRONTO], 4);
        num(m->tempo[BLOQUEADO], 4);
        num(m->tempo[EXECUTANDO], 4);
        if (m->resp_n > 0) {

            x = m->resp_soma * 10 / m->resp_n;
            num(x / 10, 3);
            k_putc('.');
            k_dec(x % 10);
        } else {
            k_imprime("    -");
        }
        fim_linha();
    }
    k_imprime("fim");
}

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
    } else {
        trata_excecao(irq);
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

// chamada uma vez no boot, pelo _kernel_inicio
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

    k_imprime("[SO] escalonador ");
    k_imprime(nome_esc());
    k_imprime(", quantum ");
    k_dec(QUANTUM);
    k_putc('\n');

    cria_processo(acha_programa("init"), CENARIO);
    so_escalona();
    recupera_quadro(quadro);
    return 0;
}
