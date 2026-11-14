// programas.c -- os programas que rodam como processos

#include "usuario.h"
#include "programas.h"

// gasta CPU: cada unidade são 100 voltas desse cálculo
// (escolhi números que não estouram os 16 bits do int)
int trabalha(int n)
{
    int x = 1;
    int u;
    int i;
    for (u = 0; u < n; u++) {
        for (i = 0; i < 100; i++) {
            x = (x * 31 + 7) % 1021;
        }
    }
    return x;
}

void mostra(char *texto, int valor)
{
    print_str("  ");
    print_str(texto);
    print_str(" -> ");
    print_int(valor);
    putchar(10);
}

void cpu(int n)
{
    print_str("cpu(");
    print_int(n);
    puts(") comecou");
    trabalha(n);
    print_str("cpu(");
    print_int(n);
    puts(") terminou");
}

void eco(int n)
{
    char linha[40];
    int i;
    // imprime pouco de propósito: cada caractere é uma chamada de sistema
    print_str("eco: digite ");
    print_int(n);
    putchar(10);
    for (i = 0; i < n; i++) {
        le_linha(linha, 40);       // aqui o processo bloqueia esperando a console
        putchar('>');
        puts(linha);
    }
}

void proibido(int tipo)
{
    int zero = 0;
    int x;
    if (tipo == 0) {
        puts("proibido: vou dar outb direto");
        tenta_es_direta();
    } else if (tipo == 1) {
        puts("proibido: vou dar halt");
        tenta_halt();
    } else {
        puts("proibido: vou dividir por zero");
        x = 10 / zero;
    }
    puts("proibido: ERRO, passou!");
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
    puts(") rodando");
    trabalha(n);
}

void pai(int n)
{
    int pids[4];
    int i;
    for (i = 0; i < n; i++) {
        pids[i] = so_cria_proc("filho", i + 1);
    }
    for (i = 0; i < n; i++) {
        so_espera_proc(pids[i]);
        print_str("pai: filho ");
        print_int(pids[i]);
        puts(" morreu");
    }
}

// ------------------------------------------------------------ cenários

void testes(void)
{
    int p;
    int pids[8];
    int i;

    puts("[1] erros");
    mostra("espera(1) (ele mesmo)", so_espera_proc(1));
    mostra("espera(99)", so_espera_proc(99));
    mostra("mata(99)", so_mata_proc(99));
    mostra("cria(xyz)", so_cria_proc("xyz", 0));

    puts("[2] protecao");
    for (i = 0; i < 3; i++) {
        p = so_cria_proc("proibido", i);
        mostra("espera(proibido)", so_espera_proc(p));
    }

    puts("[3] processo que termina sozinho");
    p = so_cria_proc("filho", 3);
    mostra("cria(filho)", p);
    mostra("espera(filho)", so_espera_proc(p));

    puts("[4] matar outro processo");
    p = so_cria_proc("laco", 0);
    mostra("cria(laco)", p);
    mostra("mata(laco)", so_mata_proc(p));
    mostra("espera(laco)", so_espera_proc(p));

    puts("[5] processo que cria processos");
    p = so_cria_proc("pai", 2);
    mostra("espera(pai)", so_espera_proc(p));

    puts("[6] tabela cheia (pid != entrada)");
    for (i = 0; i < 8; i++) {
        pids[i] = so_cria_proc("laco", 0);
        mostra("cria(laco)", pids[i]);
    }
    for (i = 0; i < 8; i++) {
        if (pids[i] > 0) {
            so_mata_proc(pids[i]);
        }
    }

    puts("[7] leitura (use o comando E)");
    p = so_cria_proc("eco", 1);
    mostra("espera(eco)", so_espera_proc(p));
}

void misto(void)
{
    int pids[3];
    int i;
    pids[0] = so_cria_proc("cpu", 40);
    pids[1] = so_cria_proc("cpu", 20);
    pids[2] = so_cria_proc("eco", 4);
    for (i = 0; i < 3; i++) {
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

void init(int cenario)
{
    print_str("init: cenario ");
    print_int(cenario);
    putchar(10);
    if (cenario == CEN_TESTES) {
        testes();
    } else if (cenario == CEN_MISTO) {
        misto();
    } else {
        cenario_cpu();
    }
    puts("init: fim");
}
