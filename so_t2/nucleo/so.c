// so.c -- núcleo do sistema operacional do T2 (processos), escrito no
// subconjunto de C do mcc e executado no processador Mancha completo.
//
// Partes do enunciado e onde estão aqui:
//
//   Parte I   struct processo, tabela "tab", variável "atual" (-1 = nenhum
//             processo em execução), salvamento/recuperação do quadro de
//             16 palavras (so_trata_interrupcao + copia_quadro), escalonador
//             simples (ESC_SIMPLES), criação e morte de processos, pid
//             diferente da entrada na tabela (prox_pid nunca é reutilizado).
//   Parte II  bloqueio em SO_LE/SO_ESCREVE quando a console não está
//             pronta, trata_pendencias() (verifica a console e desbloqueia),
//             SO_ESPERA_PROC (com os casos de erro) e o desbloqueio de quem
//             espera quando um processo morre (mata_processo).
//   Parte III fila de prontos, round-robin com quantum e preempção
//             (ESC_ROUND_ROBIN), escalonador por prioridade (ESC_PRIORIDADE)
//             e as métricas, impressas por relatorio_final() quando o init
//             morre.
//
// O caminho de toda interrupção (ver ponte.asm):
//
//   CPU empilha o quadro em pilha_sistema-32 -> stub -> _so_entrada (troca
//   para pilha_nucleo) -> so_trata_interrupcao(irq, quadro):
//       1. atualiza o relógio do SO (tempo em instruções)
//       2. salva o quadro no processo corrente
//       3. trata a interrupção (chamada de sistema, relógio, exceção, console)
//       4. trata as pendências (dispositivos que causaram bloqueio)
//       5. escalona
//       6. copia o quadro do processo escolhido para pilha_sistema-32
//   -> "rete" (ou laço ocioso, se não há ninguém pronto; ou "halt").
//
// Restrições do mcc que moldam o código: int de 16 bits com sinal (por isso
// o tipo "struct tempo" para contar instruções), sem switch, sem static,
// sem unsigned, sem ponto flutuante (prioridade em milésimos).

#include "comum.h"
#include "config.h"
#include "programas.h"

// ------------------------------------------ funções de ponte.asm (núcleo)
int  porta_le(int porta);
void porta_escreve(int porta, int valor);
int  le_contador_relogio(void);
void copia_quadro(int *destino, int *origem);
void k_putc(int c);
void k_imprime(char *s);
void k_dec(int v);
void k_hex(int v);
int  area_pilhas_usuario(void);
void zera_palavras(int *p, int n);

// de usuario/usuario.asm: endereço para onde a função principal de todo
// processo "retorna" -- faz SO_MATA_PROC(0), terminando o processo
void proc_retorno(void);

// ------------------------------------------------------------ constantes
#define MAX_PROC    8      // entradas na tabela de processos
#define TAM_PILHA   512    // bytes de pilha por processo (área em dados.asm)
#define MAX_HIST    24     // processos cujas métricas são guardadas

#define PORTA_CONSOLE_DADOS      1
#define PORTA_CONSOLE_ESTADO     2     // bit0: pronta p/ enviar; bit1: há caractere
#define PORTA_RELOGIO_CONT_ALTO  0x20
#define PORTA_RELOGIO_CONT_BAIXO 0x21
#define PORTA_RELOGIO_LIM_ALTO   0x22
#define PORTA_RELOGIO_LIM_BAIXO  0x23
#define PORTA_CTRL_INTERRUPCOES  0x30

#define IRQ_SISTEMA 7
#define IRQ_CONSOLE 8
#define IRQ_RELOGIO 10

// estados de um processo (EST_LIVRE = entrada da tabela sem processo)
#define EST_LIVRE      0
#define EST_PRONTO     1
#define EST_BLOQUEADO  2
#define EST_EXECUTANDO 3

// motivo de bloqueio
#define BLOQ_NADA    0
#define BLOQ_LE      1
#define BLOQ_ESCREVE 2
#define BLOQ_ESPERA  3

// posições no quadro de 16 palavras (bios_mancha/README.md)
#define Q_R0 0
#define Q_R1 1
#define Q_R2 2
#define Q_SP 6
#define Q_IP 7
#define Q_SR 8
#define SR_S 0x8000

// resposta de so_trata_interrupcao() para ponte.asm
#define RET_DESPACHA 0
#define RET_OCIOSO   1
#define RET_DESLIGA  2

#define PRIO_INICIAL 500   // prioridade em milésimos: 0,5

// ======================================================= tempo (instruções)
//
// int tem 16 bits: não conta além de 32767 instruções. "struct tempo"
// guarda alto*10000 + baixo (0 <= baixo < 10000), até ~327 milhões de
// instruções, só com somas/subtrações de int.

struct tempo {
    int alto;
    int baixo;
};

void t_zera(struct tempo *t)
{
    t->alto = 0;
    t->baixo = 0;
}

// t += d, com 0 <= d <= 22767
void t_soma_int(struct tempo *t, int d)
{
    t->baixo = t->baixo + d;
    while (t->baixo >= 10000) {
        t->baixo = t->baixo - 10000;
        t->alto = t->alto + 1;
    }
}

// t += a
void t_soma(struct tempo *t, struct tempo *a)
{
    t->alto = t->alto + a->alto;
    t_soma_int(t, a->baixo);
}

// r = a - b (a >= b)
void t_sub(struct tempo *r, struct tempo *a, struct tempo *b)
{
    r->alto = a->alto - b->alto;
    r->baixo = a->baixo - b->baixo;
    if (r->baixo < 0) {
        r->baixo = r->baixo + 10000;
        r->alto = r->alto - 1;
    }
}

// r = a / n (divisão longa, dígito a dígito; 1 <= n <= 3276)
void t_divide(struct tempo *r, struct tempo *a, int n)
{
    int resto;
    int q;
    int b;
    int d;

    r->alto = a->alto / n;
    resto = a->alto % n;
    q = 0;
    b = a->baixo;
    d = 1000;
    while (d > 0) {
        resto = resto * 10 + b / d;
        b = b % d;
        q = q * 10 + resto / n;
        resto = resto % n;
        d = d / 10;
    }
    r->baixo = q;
}

// =============================================== impressão (do núcleo)

int int_texto(int v, char *buf)
{
    char tmp[8];
    int i;
    int n;

    n = 0;
    i = 0;
    if (v < 0) {
        buf[n] = '-';
        n = n + 1;
        v = -v;
    }
    if (v == 0) {
        tmp[0] = '0';
        i = 1;
    }
    while (v > 0) {
        tmp[i] = '0' + v % 10;
        i = i + 1;
        v = v / 10;
    }
    while (i > 0) {
        i = i - 1;
        buf[n] = tmp[i];
        n = n + 1;
    }
    buf[n] = 0;
    return n;
}

int t_texto(struct tempo *t, char *buf)
{
    int n;
    int b;
    int d;

    if (t->alto > 0) {
        n = int_texto(t->alto, buf);
        b = t->baixo;
        d = 1000;
        while (d > 0) {
            buf[n] = '0' + b / d;
            b = b % d;
            d = d / 10;
            n = n + 1;
        }
        buf[n] = 0;
        return n;
    }
    return int_texto(t->baixo, buf);
}

void k_espacos(int n)
{
    while (n > 0) {
        k_putc(' ');
        n = n - 1;
    }
}

// inteiro alinhado à direita numa coluna de largura w
void k_dec_col(int v, int w)
{
    char b[10];
    int n;
    n = int_texto(v, b);
    k_espacos(w - n);
    k_imprime(b);
}

// tempo alinhado à direita numa coluna de largura w
void t_col(struct tempo *t, int w)
{
    char b[16];
    int n;
    n = t_texto(t, b);
    k_espacos(w - n);
    k_imprime(b);
}

void t_imprime(struct tempo *t)
{
    char b[16];
    t_texto(t, b);
    k_imprime(b);
}

// texto alinhado à esquerda numa coluna de largura w
void k_str_col(char *s, int w)
{
    int n;
    n = 0;
    while (s[n] != 0) {
        n = n + 1;
    }
    k_imprime(s);
    k_espacos(w - n);
}

// ============================================================ programas
//
// O SO conhece os programas pelo nome (SO_CRIA_PROC recebe o nome). Todos
// são funções C "void programa(int arg)" de usuario/programas.c, montadas
// junto com o núcleo; o SO só usa o endereço delas como ip inicial.

struct programa {
    char *nome;
    void (*entrada)(int);
};

#define N_PROGRAMAS 8
struct programa programas[N_PROGRAMAS] = {
    { "init",     init },
    { "cpu",      cpu },
    { "eco",      eco },
    { "escritor", escritor },
    { "proibido", proibido },
    { "laco",     laco },
    { "filho",    filho },
    { "pai",      pai }
};

// compara no máximo 16 caracteres (o nome vem da memória do processo)
int nome_igual(char *a, char *b)
{
    int i;
    i = 0;
    while (i < 16) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == 0) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

int acha_programa(char *nome)
{
    int i;
    if (nome == 0) {
        return -1;
    }
    for (i = 0; i < N_PROGRAMAS; i++) {
        if (nome_igual(nome, programas[i].nome)) {
            return i;
        }
    }
    return -1;
}

// ===================================================== processos (Parte I)

struct metricas {
    int pid;
    int prog;                   // índice em programas[]
    int arg;
    int terminou;               // 0 = ainda vivo quando o SO desligou
    struct tempo t_criacao;
    struct tempo t_termino;
    struct tempo t_estado[3];   // tempo total em pronto, bloqueado, executando
    int n_estado[3];            // vezes que entrou em pronto, bloqueado, executando
    int n_preempcoes;
    struct tempo t_resposta;    // soma dos tempos desbloqueio -> escalonado
    int n_respostas;
};

struct processo {
    int estado;                 // EST_*
    int pid;
    int quadro[16];             // contexto salvo (r0..ip, sr..dl)
    int motivo;                 // BLOQ_* (se bloqueado)
    int arg_bloqueio;           // pid esperado, ou caractere a escrever
    int ordem_bloqueio;         // para atender bloqueados por ordem de chegada
    int prioridade;             // em milésimos (0..1000); menor = mais prioritário
    int esperando_resposta;     // desbloqueou e ainda não foi escalonado
    struct tempo t_mudanca;     // instante da última mudança de estado
    struct tempo t_desbloqueio;
    struct metricas m;
};

struct processo tab[MAX_PROC];
int atual;                      // índice em tab do processo em execução; -1 = nenhum
int prox_pid;                   // pids não são reaproveitados (nº de série)

// fila de prontos (índices em tab), fila[0] é o primeiro
int fila[MAX_PROC];
int fila_n;

// configuração (de config.h)
int escalonador;
int quantum;                    // em interrupções do relógio
int intervalo;                  // instruções entre interrupções do relógio

int quantum_restante;           // decrementado a cada interrupção do relógio
int seq_bloqueio;
int n_bloq[4];                  // quantos processos bloqueados por motivo (BLOQ_*);
                                // evita varrer a tabela quando não há ninguém
int desligar;
int ultimo_despachado;

// relógio do SO
struct tempo agora;             // instruções desde o boot do SO
struct tempo base_relogio;      // voltas do contador do relógio * intervalo
int ultimo_contador;
int volta_antecipada;
int ocioso;                     // a CPU está no laço ocioso
struct tempo ocioso_desde;

// métricas globais
struct tempo t_ocioso;
struct tempo t_nucleo;          // tempo gasto dentro de so_trata_interrupcao
int max_nucleo;                 // maior duração de uma entrada no núcleo
int n_criados;
int n_preempcoes;
int n_trocas;
int n_ocioso;                   // períodos de ociosidade
int n_interrupcoes[16];
int n_chamadas[10];
struct metricas hist[MAX_HIST]; // métricas dos processos que já terminaram
int n_hist;
int n_hist_perdidos;            // processos que não couberam em hist

char *nomes_irq[16] = {
    "inicio/trap 0", "violacao de segmento", "instrucao privilegiada",
    "divisao por zero", "instrucao ilegal", "ausencia de quadro", "6",
    "chamada de sistema", "console", "disco", "relogio",
    "11", "12", "13", "14", "15"
};

// ---------------------------------------------------------------- relógio
//
// O relógio do Mancha conta instruções e volta a 0 ao atingir o limite
// ("intervalo"), gerando a interrupção 10. O tempo do SO é
//
//     agora = (voltas do contador) * intervalo + contador
//
// "base_relogio" acumula voltas*intervalo: cada interrupção do relógio
// soma uma volta. A sutileza: o núcleo roda com D=1, então se o contador
// der a volta logo depois de entrarmos no núcleo por OUTRO motivo (uma
// chamada de sistema, a console), a interrupção do relógio fica pendente e
// só chega depois do "rete" -- e o contador lido agora já recomeçou de
// perto de 0. Isso aparece como o contador MENOR que na leitura anterior
// (sem interrupção do relógio entre as duas): contamos a volta já
// ("volta_antecipada") e não a contamos de novo quando a interrupção
// pendente chegar. É exato desde que nenhum caminho do núcleo demore mais
// que "intervalo" instruções (senão o próprio hardware junta duas voltas
// numa interrupção só).
void atualiza_relogio(int irq)
{
    int c;
    c = le_contador_relogio();
    if (irq == IRQ_RELOGIO) {
        if (volta_antecipada) {
            volta_antecipada = 0;
        } else {
            t_soma_int(&base_relogio, intervalo);
        }
    } else if (c < ultimo_contador) {
        t_soma_int(&base_relogio, intervalo);
        volta_antecipada = 1;
    }
    ultimo_contador = c;
    agora.alto = base_relogio.alto;
    agora.baixo = base_relogio.baixo + c;
    while (agora.baixo >= 10000) {
        agora.baixo = agora.baixo - 10000;
        agora.alto = agora.alto + 1;
    }
}

// ------------------------------------------------- mudança de estado
//
// Toda mudança de estado passa por aqui: acumula o tempo passado no
// estado anterior e conta a entrada no novo estado.
void muda_estado(struct processo *p, int novo)
{
    struct tempo dt;
    if (p->estado != EST_LIVRE) {
        t_sub(&dt, &agora, &p->t_mudanca);
        t_soma(&p->m.t_estado[p->estado - 1], &dt);
    }
    if (novo != EST_LIVRE) {
        p->m.n_estado[novo - 1] = p->m.n_estado[novo - 1] + 1;
    }
    p->estado = novo;
    p->t_mudanca = agora;
}

// -------------------------------------------------------- fila de prontos

void fila_insere(int i)
{
    fila[fila_n] = i;
    fila_n = fila_n + 1;
}

// remove e devolve o elemento da posição k (mantém a ordem dos demais)
int fila_remove_pos(int k)
{
    int i;
    i = fila[k];
    while (k < fila_n - 1) {
        fila[k] = fila[k + 1];
        k = k + 1;
    }
    fila_n = fila_n - 1;
    return i;
}

// remove o processo i da fila, se estiver nela
void fila_retira(int i)
{
    int k;
    for (k = 0; k < fila_n; k++) {
        if (fila[k] == i) {
            fila_remove_pos(k);
            return;
        }
    }
}

// processo i fica pronto e vai para o FIM da fila
void torna_pronto(int i)
{
    muda_estado(&tab[i], EST_PRONTO);
    fila_insere(i);
}

int acha_pid(int pid)
{
    int i;
    struct processo *p;
    if (pid <= 0) {
        return -1;
    }
    p = tab;
    for (i = 0; i < MAX_PROC; i++) {
        if (p->pid == pid && p->estado != EST_LIVRE) {
            return i;
        }
        p = p + 1;
    }
    return -1;
}

// ------------------------------------------------------ criação/morte

// cria um processo executando programas[prog] com argumento "arg";
// devolve o pid, ou -1 se a tabela estiver cheia
int cria_processo(int prog, int arg)
{
    int i;
    int topo;
    int *pilha;
    struct processo *p;

    i = 0;
    while (i < MAX_PROC && tab[i].estado != EST_LIVRE) {
        i = i + 1;
    }
    if (i >= MAX_PROC) {
        return -1;
    }
    p = &tab[i];

    // pilha do processo: a função principal é chamada "de mentira", como
    // se alguém tivesse feito call: (sp) = endereço de retorno
    // (proc_retorno, que faz SO_MATA_PROC(0)) e (sp+2) = argumento -- ou
    // seja, (bp+4) = arg depois do prólogo, a convenção do mcc
    topo = area_pilhas_usuario() + (i + 1) * TAM_PILHA;
    pilha = (int *) (topo - 4);
    pilha[0] = (int) proc_retorno;
    pilha[1] = arg;

    // quadro inicial
    zera_palavras(p->quadro, 16);
    p->quadro[Q_SP] = topo - 4;
    p->quadro[Q_IP] = (int) programas[prog].entrada;
    // sr = 0: S=0 (modo usuário), I=0 (in/out proibidos), D=0
    // (interrupções habilitadas), P=0. cs=cl=ds=dl=0: segmentação plana
    // (limite 0 = sem limite) -- proteção de memória fica para o T3.
    p->quadro[Q_SR] = 0;

    p->pid = prox_pid;
    prox_pid = prox_pid + 1;
    p->motivo = BLOQ_NADA;
    p->prioridade = PRIO_INICIAL;
    p->esperando_resposta = 0;

    zera_palavras((int *) &p->m, sizeof(struct metricas) / 2);
    p->m.pid = p->pid;
    p->m.prog = prog;
    p->m.arg = arg;
    p->m.t_criacao = agora;

    p->estado = EST_LIVRE;
    torna_pronto(i);
    n_criados = n_criados + 1;

    if (SO_VERBOSO) {
        k_imprime("[SO] criado pid ");
        k_dec(p->pid);
        k_imprime(" (");
        k_imprime(programas[prog].nome);
        k_imprime(")\n");
    }
    return p->pid;
}

void guarda_historico(struct processo *p)
{
    if (n_hist < MAX_HIST) {
        hist[n_hist] = p->m;
        n_hist = n_hist + 1;
    } else {
        n_hist_perdidos = n_hist_perdidos + 1;
    }
}

void desbloqueia(int i, int valor)
{
    struct processo *p;
    p = &tab[i];
    p->quadro[Q_R0] = valor;       // valor de retorno da chamada que bloqueou
    n_bloq[p->motivo] = n_bloq[p->motivo] - 1;
    p->motivo = BLOQ_NADA;
    torna_pronto(i);
    p->esperando_resposta = 1;
    p->t_desbloqueio = agora;
}

void mata_processo(int i)
{
    struct processo *p;
    int pid;
    int j;

    p = &tab[i];
    pid = p->pid;
    if (p->estado == EST_PRONTO) {
        fila_retira(i);
    } else if (p->estado == EST_BLOQUEADO) {
        n_bloq[p->motivo] = n_bloq[p->motivo] - 1;
    }
    p->motivo = BLOQ_NADA;
    muda_estado(p, EST_LIVRE);
    p->m.terminou = 1;
    p->m.t_termino = agora;
    guarda_historico(p);
    if (i == atual) {
        atual = -1;
    }
    if (SO_VERBOSO) {
        k_imprime("[SO] terminou pid ");
        k_dec(pid);
        k_putc('\n');
    }

    // Parte II: acorda quem estava esperando a morte deste processo
    if (n_bloq[BLOQ_ESPERA] > 0) {
        p = tab;
        for (j = 0; j < MAX_PROC; j++) {
            if (p->arg_bloqueio == pid && p->motivo == BLOQ_ESPERA
                && p->estado == EST_BLOQUEADO) {
                desbloqueia(j, 0);
            }
            p = p + 1;
        }
    }

    if (pid == 1) {
        desligar = 1;              // morreu o init: fim da execução do SO
    }
}

// ------------------------------------------------- perda do processador

// prio = (prio + t_exec/t_quantum) / 2, em milésimos; t_exec = quantum
// menos o que sobrou da variável decrementada a cada interrupção
void recalcula_prioridade(struct processo *p)
{
    int t_exec;
    t_exec = quantum - quantum_restante;
    if (t_exec < 0) {
        t_exec = 0;
    }
    p->prioridade = (p->prioridade + (t_exec * 1000) / quantum) / 2;
}

void bloqueia_atual(int motivo, int arg)
{
    struct processo *p;
    p = &tab[atual];
    if (escalonador == ESC_PRIORIDADE) {
        recalcula_prioridade(p);
    }
    p->motivo = motivo;
    n_bloq[motivo] = n_bloq[motivo] + 1;
    p->arg_bloqueio = arg;
    p->ordem_bloqueio = seq_bloqueio;
    seq_bloqueio = seq_bloqueio + 1;
    muda_estado(p, EST_BLOQUEADO);
    atual = -1;
}

// fim do quantum: o processo corrente vai para o fim da fila
void preempta_atual(void)
{
    struct processo *p;
    p = &tab[atual];
    if (escalonador == ESC_PRIORIDADE) {
        recalcula_prioridade(p);
    }
    p->m.n_preempcoes = p->m.n_preempcoes + 1;
    n_preempcoes = n_preempcoes + 1;
    torna_pronto(atual);
    atual = -1;
}

// processo bloqueado pelo motivo dado há mais tempo, ou -1
int bloqueado_mais_antigo(int motivo)
{
    int i;
    int melhor;
    int ordem;
    struct processo *p;
    melhor = -1;
    ordem = 0;
    if (n_bloq[motivo] == 0) {
        return -1;
    }
    p = tab;
    for (i = 0; i < MAX_PROC; i++) {
        if (p->motivo == motivo && p->estado == EST_BLOQUEADO) {
            if (melhor < 0 || p->ordem_bloqueio < ordem) {
                melhor = i;
                ordem = p->ordem_bloqueio;
            }
        }
        p = p + 1;
    }
    return melhor;
}

// ===================================================== chamadas de sistema

void chamada_le(struct processo *p)
{
    // há entrada e ninguém na frente esperando: lê já, sem bloquear
    if (n_bloq[BLOQ_LE] == 0 && (porta_le(PORTA_CONSOLE_ESTADO) & 2) != 0) {
        p->quadro[Q_R0] = porta_le(PORTA_CONSOLE_DADOS);
    } else {
        bloqueia_atual(BLOQ_LE, 0);   // trata_pendencias() termina a leitura
    }
}

void chamada_escreve(struct processo *p)
{
    int c;
    c = p->quadro[Q_R1];
    if (n_bloq[BLOQ_ESCREVE] == 0 && (porta_le(PORTA_CONSOLE_ESTADO) & 1) != 0) {
        porta_escreve(PORTA_CONSOLE_DADOS, c);
        p->quadro[Q_R0] = 0;
    } else {
        bloqueia_atual(BLOQ_ESCREVE, c);
    }
}

void chamada_cria(struct processo *p)
{
    int prog;
    prog = acha_programa((char *) p->quadro[Q_R1]);
    if (prog < 0) {
        p->quadro[Q_R0] = -1;
        return;
    }
    p->quadro[Q_R0] = cria_processo(prog, p->quadro[Q_R2]);
}

void chamada_mata(struct processo *p)
{
    int pid;
    int i;
    pid = p->quadro[Q_R1];
    if (pid == 0) {
        pid = p->pid;
    }
    i = acha_pid(pid);
    if (i < 0) {
        p->quadro[Q_R0] = -1;
        return;
    }
    p->quadro[Q_R0] = 0;           // (irrelevante se matou a si mesmo)
    mata_processo(i);
}

// Nunca bloqueia esperando algo que não vai acontecer:
//   - pid do próprio processo, ou pid que nunca existiu -> -1 (erro);
//   - pid de um processo que JÁ terminou -> 0 na hora (o que se esperava já
//     aconteceu; sem isso, um filho que termina antes de o pai chamar
//     SO_ESPERA_PROC daria "erro" ao pai, dependendo do escalonamento);
//   - processo vivo -> bloqueia; mata_processo() desbloqueia com 0.
void chamada_espera(struct processo *p)
{
    int pid;
    int i;
    pid = p->quadro[Q_R1];
    if (pid == p->pid || pid <= 0 || pid >= prox_pid) {
        p->quadro[Q_R0] = -1;
        return;
    }
    i = acha_pid(pid);
    if (i < 0) {
        p->quadro[Q_R0] = 0;
        return;
    }
    bloqueia_atual(BLOQ_ESPERA, pid);
}

void trata_chamada(void)
{
    struct processo *p;
    int num;
    if (atual < 0) {
        return;
    }
    p = &tab[atual];
    // o despachante da BIOS só chega aqui com os números registrados em
    // tabela_syscalls (1, 2, 7, 8, 9); os outros ele mesmo responde com -1
    num = p->quadro[Q_R0];
    n_chamadas[num] = n_chamadas[num] + 1;
    if (num == SO_ESCREVE) {
        chamada_escreve(p);
    } else if (num == SO_LE) {
        chamada_le(p);
    } else if (num == SO_CRIA_PROC) {
        chamada_cria(p);
    } else if (num == SO_MATA_PROC) {
        chamada_mata(p);
    } else if (num == SO_ESPERA_PROC) {
        chamada_espera(p);
    } else {
        p->quadro[Q_R0] = -1;
    }
}

// ============================================================ interrupções

void trata_relogio(void)
{
    if (atual < 0 || escalonador == ESC_SIMPLES) {
        return;
    }
    quantum_restante = quantum_restante - 1;
    if (quantum_restante <= 0) {
        preempta_atual();
    }
}

// exceção causada por um processo de usuário: o SO mata só esse processo
void trata_excecao(int irq)
{
    struct processo *p;
    p = &tab[atual];
    k_imprime("[SO] pid ");
    k_dec(p->pid);
    k_imprime(" (");
    k_imprime(programas[p->m.prog].nome);
    k_imprime(") morto: ");
    k_imprime(nomes_irq[irq]);
    k_imprime(" em ip=");
    k_hex(p->quadro[Q_IP]);
    k_putc('\n');
    mata_processo(atual);
}

// exceção no próprio núcleo: não há o que fazer
void panico(int irq, int *quadro)
{
    k_imprime("\n[SO] PANICO: ");
    k_imprime(nomes_irq[irq]);
    k_imprime(" no nucleo em ip=");
    k_hex(quadro[Q_IP]);
    k_putc('\n');
}

// Parte II: verifica os dispositivos que causaram bloqueio, faz as
// operações pendentes e desbloqueia os processos (por ordem de chegada)
void trata_pendencias(void)
{
    int i;
    // (testa primeiro o contador e o dispositivo, que são baratos, e só
    // então procura o processo na tabela)
    while (n_bloq[BLOQ_LE] > 0 && (porta_le(PORTA_CONSOLE_ESTADO) & 2) != 0) {
        i = bloqueado_mais_antigo(BLOQ_LE);
        desbloqueia(i, porta_le(PORTA_CONSOLE_DADOS));
    }
    while (n_bloq[BLOQ_ESCREVE] > 0 && (porta_le(PORTA_CONSOLE_ESTADO) & 1) != 0) {
        i = bloqueado_mais_antigo(BLOQ_ESCREVE);
        porta_escreve(PORTA_CONSOLE_DADOS, tab[i].arg_bloqueio);
        desbloqueia(i, 0);
    }
}

// ============================================================ escalonador

void despacha(int i)
{
    struct processo *p;
    struct tempo dt;
    p = &tab[i];
    if (p->esperando_resposta) {
        t_sub(&dt, &agora, &p->t_desbloqueio);
        t_soma(&p->m.t_resposta, &dt);
        p->m.n_respostas = p->m.n_respostas + 1;
        p->esperando_resposta = 0;
    }
    muda_estado(p, EST_EXECUTANDO);
    atual = i;
    quantum_restante = quantum;
    if (i != ultimo_despachado) {
        n_trocas = n_trocas + 1;
        ultimo_despachado = i;
    }
}

// Escolhe o próximo processo (altera "atual"). Se o processo corrente
// ainda pode executar (não bloqueou, não morreu, não perdeu o quantum),
// ele continua -- nos três escalonadores.
void escalona(void)
{
    int i;
    int k;
    int melhor;

    if (atual >= 0) {
        return;
    }
    if (fila_n == 0) {
        return;                     // ninguém pronto: CPU ociosa
    }
    if (escalonador == ESC_SIMPLES) {
        // Parte I: o primeiro processo pronto, na ordem da tabela
        i = 0;
        while (tab[i].estado != EST_PRONTO) {
            i = i + 1;
        }
        fila_retira(i);
    } else if (escalonador == ESC_ROUND_ROBIN) {
        // Parte III: o primeiro da fila
        i = fila_remove_pos(0);
    } else {
        // Parte III: menor valor de prioridade; empate -> o que está há
        // mais tempo na fila
        melhor = 0;
        for (k = 1; k < fila_n; k++) {
            if (tab[fila[k]].prioridade < tab[fila[melhor]].prioridade) {
                melhor = k;
            }
        }
        i = fila_remove_pos(melhor);
    }
    despacha(i);
}

// ================================================== relatório (métricas)

char *nome_escalonador(void)
{
    if (escalonador == ESC_SIMPLES) {
        return "simples";
    }
    if (escalonador == ESC_ROUND_ROBIN) {
        return "round-robin";
    }
    return "prioridade";
}

struct metricas *hist_por_pid(int pid)
{
    int i;
    for (i = 0; i < n_hist; i++) {
        if (hist[i].pid == pid) {
            return &hist[i];
        }
    }
    return 0;
}

void relatorio_final(void)
{
    int i;
    int pid;
    struct metricas *m;
    struct tempo dt;

    // processos ainda vivos: fecha a contagem de tempo do estado atual
    for (i = 0; i < MAX_PROC; i++) {
        if (tab[i].estado != EST_LIVRE) {
            muda_estado(&tab[i], EST_LIVRE);
            guarda_historico(&tab[i]);
        }
    }

    k_imprime("\n============ SO: metricas da execucao ============\n");
    k_imprime("escalonador: ");
    k_imprime(nome_escalonador());
    k_imprime("   quantum: ");
    k_dec(quantum);
    k_imprime(" interrupcoes   intervalo do relogio: ");
    k_dec(intervalo);
    k_imprime(" instrucoes\n");
    k_imprime("processos criados .............. ");
    k_dec(n_criados);
    k_imprime("\ntempo total de execucao ........ ");
    t_imprime(&agora);
    k_imprime("\ntempo ocioso ................... ");
    t_imprime(&t_ocioso);
    k_imprime("  (");
    k_dec(n_ocioso);
    k_imprime(" vezes)\ntempo no nucleo ................ ");
    t_imprime(&t_nucleo);
    k_imprime("  (maior entrada: ");
    k_dec(max_nucleo);
    k_imprime(")\npreempcoes ..................... ");
    k_dec(n_preempcoes);
    k_imprime("\ntrocas de contexto ............. ");
    k_dec(n_trocas);
    k_imprime("\ninterrupcoes recebidas:\n");
    for (i = 0; i < 16; i++) {
        if (n_interrupcoes[i] > 0 || i == 2 || i == 7 || i == 8 || i == 10) {
            k_dec_col(i, 4);
            k_imprime(" ");
            k_str_col(nomes_irq[i], 23);
            k_dec_col(n_interrupcoes[i], 7);
            k_putc('\n');
        }
    }
    k_imprime("chamadas: le=");
    k_dec(n_chamadas[SO_LE]);
    k_imprime(" escreve=");
    k_dec(n_chamadas[SO_ESCREVE]);
    k_imprime(" cria=");
    k_dec(n_chamadas[SO_CRIA_PROC]);
    k_imprime(" mata=");
    k_dec(n_chamadas[SO_MATA_PROC]);
    k_imprime(" espera=");
    k_dec(n_chamadas[SO_ESPERA_PROC]);
    k_putc('\n');

    k_imprime("\npor processo (tempos em instrucoes;");
    k_imprime(" P=pronto B=bloqueado E=executando)\n");
    k_imprime("pid programa  arg  retorno preemp");
    k_imprime("  nP  nB  nE  resp.media\n");
    for (pid = 1; pid < prox_pid; pid++) {
        m = hist_por_pid(pid);
        if (m == 0) {
            continue;
        }
        k_dec_col(pid, 3);
        k_putc(' ');
        k_str_col(programas[m->prog].nome, 9);
        k_dec_col(m->arg, 4);
        if (m->terminou) {
            t_sub(&dt, &m->t_termino, &m->t_criacao);
            t_col(&dt, 9);
        } else {
            k_imprime("  (vivo)");
        }
        k_dec_col(m->n_preempcoes, 7);
        k_dec_col(m->n_estado[0], 4);
        k_dec_col(m->n_estado[1], 4);
        k_dec_col(m->n_estado[2], 4);
        if (m->n_respostas > 0) {
            t_divide(&dt, &m->t_resposta, m->n_respostas);
            t_col(&dt, 12);
        } else {
            k_imprime("           -");
        }
        k_putc('\n');
    }
    k_imprime("pid programa   t.pronto    t.bloq    t.exec");
    k_imprime("   criacao   termino\n");
    for (pid = 1; pid < prox_pid; pid++) {
        m = hist_por_pid(pid);
        if (m == 0) {
            continue;
        }
        k_dec_col(pid, 3);
        k_putc(' ');
        k_str_col(programas[m->prog].nome, 9);
        t_col(&m->t_estado[0], 10);
        t_col(&m->t_estado[1], 10);
        t_col(&m->t_estado[2], 10);
        t_col(&m->t_criacao, 10);
        if (m->terminou) {
            t_col(&m->t_termino, 10);
        } else {
            k_imprime("         -");
        }
        k_putc('\n');
    }
    if (n_hist_perdidos > 0) {
        k_imprime("(metricas de ");
        k_dec(n_hist_perdidos);
        k_imprime(" processos nao couberam na tabela)\n");
    }
    k_imprime("==================================================\n");
}

// ========================================================= pontos de entrada

// Parte I: salva o quadro de 16 palavras empilhado pela CPU na entrada da
// tabela do processo em execução...
void salva_contexto(int *quadro)
{
    if (atual >= 0) {
        copia_quadro(tab[atual].quadro, quadro);
    }
}

// ... e recupera o do processo escolhido para o lugar de onde o "rete"
// vai desempilhar
void recupera_contexto(int *quadro)
{
    copia_quadro(quadro, tab[atual].quadro);
}

// mede quanto durou esta entrada no núcleo (desde a leitura do relógio em
// atualiza_relogio): é o custo do SO, que roda na MESMA CPU simulada que
// os processos. Tem que ficar abaixo do intervalo do relógio (ver
// atualiza_relogio).
void mede_nucleo(void)
{
    int d;
    d = le_contador_relogio() - ultimo_contador;
    if (d < 0) {
        d = d + intervalo;
    }
    t_soma_int(&t_nucleo, d);
    if (d > max_nucleo) {
        max_nucleo = d;
    }
}

// Chamada por ponte.asm a cada interrupção. "quadro" é o endereço
// (pilha_sistema-32) onde a CPU empilhou o contexto interrompido e de onde
// o "rete" vai desempilhar o contexto do processo escolhido.
int so_trata_interrupcao(int irq, int *quadro)
{
    struct tempo dt;
    int estava_ocioso;

    // tempo ocioso = período em que não há processo pronto nem executando
    // (todos bloqueados); inclui os tiques do relógio atendidos nele
    atualiza_relogio(irq);
    estava_ocioso = ocioso;
    if (ocioso) {
        t_sub(&dt, &agora, &ocioso_desde);
        t_soma(&t_ocioso, &dt);
        ocioso = 0;
    }
    n_interrupcoes[irq] = n_interrupcoes[irq] + 1;

    // exceção da CPU (1..5) dentro do próprio núcleo (ou do laço ocioso):
    // pânico
    if (irq < IRQ_SISTEMA) {
        if (atual < 0 || (quadro[Q_SR] & SR_S) != 0) {
            panico(irq, quadro);
            return RET_DESLIGA;
        }
    }

    salva_contexto(quadro);

    if (irq == IRQ_SISTEMA) {
        trata_chamada();
    } else if (irq == IRQ_RELOGIO) {
        trata_relogio();
    } else if (irq != IRQ_CONSOLE) {
        trata_excecao(irq);
    }
    // IRQ_CONSOLE: nada específico -- trata_pendencias() olha a console

    if (desligar) {
        relatorio_final();
        return RET_DESLIGA;
    }

    trata_pendencias();
    escalona();

    mede_nucleo();
    if (atual < 0) {
        ocioso = 1;
        ocioso_desde = agora;
        if (!estava_ocioso) {
            n_ocioso = n_ocioso + 1;
        }
        return RET_OCIOSO;
    }
    recupera_contexto(quadro);
    return RET_DESPACHA;
}

// Chamada uma vez, por _kernel_inicio (ponte.asm), com interrupções ainda
// desligadas. Zera todo o estado (o boot pode se repetir: comando "R" do
// simulador), programa o hardware, cria o init e deixa o quadro dele em
// "quadro" para o primeiro "rete".
int so_inicia(int *quadro)
{
    int i;
    int prog;

    for (i = 0; i < MAX_PROC; i++) {
        tab[i].estado = EST_LIVRE;
        tab[i].pid = 0;
        fila[i] = 0;
    }
    for (i = 0; i < 16; i++) {
        n_interrupcoes[i] = 0;
    }
    for (i = 0; i < 10; i++) {
        n_chamadas[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        n_bloq[i] = 0;
    }
    atual = -1;
    prox_pid = 1;
    fila_n = 0;
    quantum_restante = 0;
    seq_bloqueio = 0;
    desligar = 0;
    ultimo_despachado = -1;
    t_zera(&agora);
    t_zera(&base_relogio);
    volta_antecipada = 0;
    t_zera(&ocioso_desde);
    t_zera(&t_ocioso);
    t_zera(&t_nucleo);
    max_nucleo = 0;
    ocioso = 0;
    n_criados = 0;
    n_preempcoes = 0;
    n_trocas = 0;
    n_ocioso = 0;
    n_hist = 0;
    n_hist_perdidos = 0;

    escalonador = ESCALONADOR;
    quantum = QUANTUM;
    intervalo = INTERVALO_RELOGIO;
    if (quantum < 1) {
        quantum = 1;
    }

    // relógio: contador zerado, interrupção a cada "intervalo" instruções
    porta_escreve(PORTA_RELOGIO_CONT_ALTO, 0);
    porta_escreve(PORTA_RELOGIO_CONT_BAIXO, 0);
    porta_escreve(PORTA_RELOGIO_LIM_ALTO, intervalo >> 8);
    porta_escreve(PORTA_RELOGIO_LIM_BAIXO, intervalo & 255);
    ultimo_contador = 0;
    // controlador de interrupções: console (bit 0) e relógio (bit 2)
    porta_escreve(PORTA_CTRL_INTERRUPCOES, 5);

    k_imprime("[SO] T2 -- escalonador ");
    k_imprime(nome_escalonador());
    k_imprime(", quantum ");
    k_dec(quantum);
    k_imprime(" x ");
    k_dec(intervalo);
    k_imprime(" instrucoes\n");

    prog = acha_programa("init");
    if (prog < 0 || cria_processo(prog, CENARIO) < 0) {
        k_imprime("[SO] nao foi possivel criar o init\n");
        return RET_DESLIGA;
    }
    escalona();
    recupera_contexto(quadro);
    return RET_DESPACHA;
}
