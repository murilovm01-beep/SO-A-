// programas.c -- programas de usuário do T2. Rodam em modo usuário, sem
// acesso a portas: toda E/S é feita com as chamadas de sistema (usuario.h).
//
// Todos compartilham a mesma console (o Mancha só tem uma), então as
// saídas dos processos aparecem intercaladas -- inclusive no meio de uma
// linha, quando a preempção acontece entre duas chamadas SO_ESCREVE.

#include "usuario.h"
#include "programas.h"

// ------------------------------------------------------------ utilidades

// "trabalho de CPU": cada unidade são 100 passos de um gerador
// congruencial (~4000 instruções com o código do mcc; os coeficientes
// cabem em 16 bits: 1020*31+7 < 32767)
int trabalha(int unidades)
{
    int u;
    int i;
    int x;

    x = 1;
    for (u = 0; u < unidades; u++) {
        for (i = 0; i < 100; i++) {
            x = (x * 31 + 7) % 1021;
        }
    }
    return x;
}

void mostra(char *oque, int r)
{
    print_str("  ");
    print_str(oque);
    print_str(" -> ");
    print_int(r);
    putchar(10);
}

// ------------------------------------------------------------- programas

void cpu(int n)
{
    int r;
    print_str("cpu(");
    print_int(n);
    puts("): inicio");
    r = trabalha(n);
    print_str("cpu(");
    print_int(n);
    print_str("): fim, resultado ");
    print_int(r);
    putchar(10);
}

void eco(int n)
{
    char linha[40];
    int i;
    for (i = 0; i < n; i++) {
        le_linha(linha, 40);           // bloqueia até chegar uma linha
        trabalha(1);
        print_str("eco: ");
        puts(linha);
    }
}

void escritor(int n)
{
    int i;
    for (i = 1; i <= n; i++) {
        print_str("escritor: linha ");
        print_int(i);
        putchar(10);
        trabalha(1);
    }
}

void proibido(int tipo)
{
    int zero;
    int x;
    zero = 0;
    if (tipo == 0) {
        puts("proibido: vou fazer outb direto na console...");
        tenta_es_direta();
    } else if (tipo == 1) {
        puts("proibido: vou executar halt...");
        tenta_halt();
    } else {
        puts("proibido: vou dividir por zero...");
        x = 10 / zero;
    }
    puts("proibido: ERRO, o SO deixou passar!");
}

void laco(int x)
{
    for (;;) {
        x = trabalha(1);
    }
}

void filho(int n)
{
    print_str("filho(");
    print_int(n);
    puts("): calculando");
    trabalha(n);
    print_str("filho(");
    print_int(n);
    puts("): terminando (retorno de main)");
}

void pai(int n)
{
    int pids[4];
    int i;
    if (n > 4) {
        n = 4;
    }
    for (i = 0; i < n; i++) {
        pids[i] = so_cria_proc("filho", i + 1);
        print_str("pai: criei o filho de pid ");
        print_int(pids[i]);
        putchar(10);
    }
    for (i = 0; i < n; i++) {
        so_espera_proc(pids[i]);
        print_str("pai: o filho de pid ");
        print_int(pids[i]);
        puts(" morreu");
    }
}

// ------------------------------------------------------------- cenários

void cenario_testes(void)
{
    int p;
    int pids[8];
    int i;
    int n;

    puts("init: [1] erros nas chamadas");
    mostra("espera_proc(1), o proprio init", so_espera_proc(1));
    mostra("espera_proc(99), nao existe", so_espera_proc(99));
    mostra("mata_proc(99), nao existe", so_mata_proc(99));
    mostra("cria_proc(xyz), programa inexistente", so_cria_proc("xyz", 0));

    puts("init: [2] protecao (S=0, I=0): o SO mata quem tenta");
    for (i = 0; i < 3; i++) {
        p = so_cria_proc("proibido", i);
        mostra("espera_proc(proibido)", so_espera_proc(p));
    }

    puts("init: [3] fim retornando de main + espera");
    p = so_cria_proc("filho", 3);
    mostra("cria_proc(filho)", p);
    mostra("espera_proc(filho)", so_espera_proc(p));

    puts("init: [4] matar outro processo");
    p = so_cria_proc("laco", 0);
    mostra("cria_proc(laco)", p);
    mostra("mata_proc(laco)", so_mata_proc(p));
    mostra("espera_proc(laco, ja terminou)", so_espera_proc(p));

    puts("init: [5] processo que cria processos");
    p = so_cria_proc("pai", 2);
    mostra("espera_proc(pai)", so_espera_proc(p));

    puts("init: [6] tabela cheia e pid != entrada da tabela");
    n = 0;
    for (i = 0; i < 8; i++) {
        pids[i] = so_cria_proc("laco", 0);
        print_str("  cria_proc(laco) -> ");
        print_int(pids[i]);
        putchar(10);
    }
    for (i = 0; i < 8; i++) {
        if (pids[i] > 0) {
            so_mata_proc(pids[i]);
            n = n + 1;
        }
    }
    mostra("processos laco mortos", n);

    puts("init: [7] leitura bloqueante: 2 leitores na mesma console");
    puts("  (cada SO_LE pega 1 caractere; digite 2 linhas)");
    pids[0] = so_cria_proc("eco", 1);
    pids[1] = so_cria_proc("eco", 1);
    mostra("espera_proc(eco 1)", so_espera_proc(pids[0]));
    mostra("espera_proc(eco 2)", so_espera_proc(pids[1]));

    puts("init: [8] matar processo bloqueado na leitura");
    pids[0] = so_cria_proc("eco", 1);
    p = so_cria_proc("filho", 1);
    so_espera_proc(p);              // enquanto isso o eco roda e bloqueia
    mostra("mata_proc(eco bloqueado)", so_mata_proc(pids[0]));
    mostra("espera_proc(eco, ja terminou)", so_espera_proc(pids[0]));
}

void cenario_misto(void)
{
    int pids[4];
    int i;
    pids[0] = so_cria_proc("cpu", 40);
    pids[1] = so_cria_proc("eco", 4);
    pids[2] = so_cria_proc("cpu", 20);
    pids[3] = so_cria_proc("escritor", 12);
    for (i = 0; i < 4; i++) {
        so_espera_proc(pids[i]);
    }
}

void cenario_cpu(void)
{
    int pids[4];
    int i;
    pids[0] = so_cria_proc("cpu", 40);
    pids[1] = so_cria_proc("cpu", 30);
    pids[2] = so_cria_proc("cpu", 20);
    pids[3] = so_cria_proc("cpu", 10);
    for (i = 0; i < 4; i++) {
        so_espera_proc(pids[i]);
    }
}

// o primeiro processo: o SO o cria no boot e desliga quando ele morre
void init(int cenario)
{
    print_str("init: cenario ");
    print_int(cenario);
    putchar(10);
    if (cenario == CEN_TESTES) {
        cenario_testes();
    } else if (cenario == CEN_MISTO) {
        cenario_misto();
    } else {
        cenario_cpu();
    }
    puts("init: fim");
}
