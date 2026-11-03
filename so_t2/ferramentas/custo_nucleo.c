// custo_nucleo.c -- mede quantas instruções custa cada entrada no núcleo do
// SO, por tipo (chamada de sistema, relógio, console, exceção), rodando o
// so.mob no mesmo núcleo de simulação do executor. É a tabela "Custo do
// núcleo" do RELATORIO.md.
//
// uso: custo_nucleo so.mob [-e instante:texto]...
//
// Uma "entrada" vai do passo em que a CPU sai do modo usuário (ou do laço
// ocioso do SO) para atender uma interrupção/trap até voltar ao modo usuário
// (depois do "rete") ou ao laço ocioso -- inclui o despachante da BIOS, os
// stubs de ponte.asm e o código C do núcleo. O tipo é lido em _so_entrada:
// r0 = número da interrupção e, se for 7, o número da chamada no r0 do
// quadro empilhado. Para no início do relatório final (não é uma entrada
// "normal"). Precisa dos símbolos públicos _so_entrada, _so_ocioso_laco e
// _f_relatorio_final (ponte.asm).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memoria.h"
#include "dispositivos.h"
#include "cpu.h"
#include "objeto.h"

#define MAX_EVENTOS 64

static const char *nome_tipo(int t)
{
  switch (t) {
    case 1: return "excecao: violacao de segmento";
    case 2: return "excecao: instrucao privilegiada";
    case 3: return "excecao: divisao por zero";
    case 4: return "excecao: instrucao ilegal";
    case 8: return "interrupcao da console";
    case 10: return "interrupcao do relogio";
    case 16 + 1: return "SO_LE";
    case 16 + 2: return "SO_ESCREVE";
    case 16 + 7: return "SO_CRIA_PROC";
    case 16 + 8: return "SO_MATA_PROC";
    case 16 + 9: return "SO_ESPERA_PROC";
    default: return "?";
  }
}

int main(int argc, char *argv[])
{
  const char *arquivo = NULL;
  long ev_t[MAX_EVENTOS];
  char *ev_txt[MAX_EVENTOS];
  int ev_feito[MAX_EVENTOS] = {0};
  int n_ev = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-e") == 0 && i + 1 < argc && n_ev < MAX_EVENTOS) {
      char *arg = argv[++i];
      char *dp = strchr(arg, ':');
      if (dp == NULL) { fprintf(stderr, "evento invalido: %s\n", arg); return 1; }
      *dp = '\0';
      ev_t[n_ev] = atol(arg);
      ev_txt[n_ev] = dp + 1;
      n_ev++;
    } else {
      arquivo = argv[i];
    }
  }
  if (arquivo == NULL) {
    fprintf(stderr, "uso: %s so.mob [-e instante:texto]...\n", argv[0]);
    return 1;
  }

  mem_t *mem = mem_cria();
  disp_t *disp = disp_cria(mem);
  cpu_t *cpu = cpu_cria(mem, disp);
  simbolo_t *simbolos = NULL;
  char erro[256];
  if (!obj_carrega(arquivo, mem, &simbolos, erro, sizeof(erro))) {
    fprintf(stderr, "erro: %s\n", erro);
    return 1;
  }
  bool a1, a2, a3;
  uint16_t entrada = obj_busca_simbolo(simbolos, "_so_entrada", &a1);
  uint16_t ocioso = obj_busca_simbolo(simbolos, "_so_ocioso_laco", &a2);
  uint16_t relatorio = obj_busca_simbolo(simbolos, "_f_relatorio_final", &a3);
  obj_libera_simbolos(simbolos);
  if (!a1 || !a2 || !a3) {
    fprintf(stderr, "o .mob precisa dos simbolos publicos _so_entrada, _so_ocioso_laco e _f_relatorio_final\n");
    return 1;
  }

  long n[32] = {0}, soma[32] = {0}, maximo[32] = {0};
  long inicio = -1;
  int tipo = -1;

  cpu_liga(cpu);
  while (!cpu_parada(cpu)) {
    long agora = cpu_num_instrucoes(cpu);
    for (int i = 0; i < n_ev; i++) {
      if (!ev_feito[i] && ev_t[i] <= agora) {
        for (char *p = ev_txt[i]; *p != '\0'; p++) console_poe_entrada(disp, *p);
        console_poe_entrada(disp, '\n');
        ev_feito[i] = 1;
      }
    }

    uint16_t ip = cpu_r(cpu, 7);
    if (ip == relatorio) break;
    if (ip == entrada) {
      tipo = cpu_r(cpu, 0);
      if (tipo == 7) tipo = 16 + mem_le_palavra(mem, cpu_r(cpu, 6));   // r0 do quadro
      if (tipo < 0 || tipo >= 32) tipo = 0;
    }
    bool fora = !cpu_modo_supervisor(cpu) || ip == ocioso;

    cpu_executa_1(cpu);
    int tam;
    console_saida(disp, &tam);
    console_limpa_saida(disp);

    uint16_t ip2 = cpu_r(cpu, 7);
    bool fora2 = !cpu_modo_supervisor(cpu) || ip2 == ocioso;
    if (fora && !fora2 && agora > 0) {
      inicio = agora;                    // começou uma entrada no núcleo
    } else if (!fora && fora2 && inicio >= 0) {
      long k = cpu_num_instrucoes(cpu) - inicio;
      if (tipo >= 0) {
        n[tipo]++;
        soma[tipo] += k;
        if (k > maximo[tipo]) maximo[tipo] = k;
      }
      inicio = -1;
      tipo = -1;
    }
  }

  printf("| entrada | n | media | maximo |\n|---|---:|---:|---:|\n");
  int ordem[] = {16 + 2, 16 + 1, 10, 8, 16 + 7, 16 + 9, 16 + 8, 1, 2, 3, 4};
  for (unsigned i = 0; i < sizeof(ordem) / sizeof(ordem[0]); i++) {
    int t = ordem[i];
    if (n[t] > 0)
      printf("| %s | %ld | %.0f | %ld |\n", nome_tipo(t), n[t], (double)soma[t] / n[t], maximo[t]);
  }

  cpu_destroi(cpu);
  disp_destroi(disp);
  mem_destroi(mem);
  return 0;
}
