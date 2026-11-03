// executor.c -- executa um .mob do Mancha completo SEM a interface curses
// ("modo lote"), para rodar o SO do T2 de forma automática e repetível
// (testes e experimentos do relatório).
//
// Usa exatamente o mesmo núcleo de simulação do simulador interativo
// (cpu.c, dispositivos.c, memoria.c, objeto.c, instrucao.c de
// ../simulador_completo/src) -- só troca a tela curses por:
//   - a saída da console vai direto para a saída padrão;
//   - a entrada da console (o que o operador digitaria com "E<texto>")
//     é injetada automaticamente, em instantes pré-definidos.
//
// uso: executor arquivo.mob [-n max_instrucoes] [-e instante:texto]...
//                           [-o espera:texto]... [-l N] [-D imagem_disco]
//
//   -n N            para depois de N instruções (padrão: 50 milhões), se a
//                   CPU não tiver parado (halt) antes
//   -e T:texto      no instante T (em instruções), põe "texto\n" na fila de
//                   entrada da console -- o mesmo que o comando "Etexto" do
//                   simulador interativo. Pode ser repetido.
//   -o E:texto      põe "texto\n" na entrada depois que a CPU tiver passado
//                   E instruções no laço ocioso do SO, contadas desde a
//                   injeção anterior (o "usuário" só digita depois que o
//                   sistema fica um tempo esperando por ele). Precisa do
//                   símbolo público _so_ocioso_laco no .mob. Pode ser
//                   repetido (um por vez, na ordem dada).
//   -l N            console "lenta": depois de cada caractere escrito, a
//                   console fica N instruções sem estar pronta para escrever
//                   (bit 0 da porta de estado em 0). A console do simulador
//                   está SEMPRE pronta; esta opção existe para exercitar o
//                   bloqueio de SO_ESCREVE (Parte II). Implementada sem
//                   mexer no simulador: o Makefile liga o executor com
//                   -Wl,--wrap para interceptar disp_le_byte/
//                   disp_escreve_byte/disp_tick.
//   -D arquivo      monta uma imagem de disco na unidade 0 (não usado pelo T2)
//
// Ao terminar, escreve em stderr quantas instruções foram executadas, quantas
// em modo supervisor (e quantas dessas no laço ocioso), se a CPU parou por
// "halt" ou pelo limite, e em que instrução o SO começou o relatório final
// (símbolo público _f_relatorio_final) -- para conferir o relógio do SO
// contra a contagem real. Código de saída: 0 = halt, 2 = limite.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memoria.h"
#include "dispositivos.h"
#include "cpu.h"
#include "objeto.h"

#define MAX_EVENTOS 256

typedef struct {
  long valor;          // instante (-e) ou instruções ociosas seguidas (-o)
  int ocioso;          // 1 = evento "-o"
  char texto[200];
  int feito;
} evento_t;

static evento_t eventos[MAX_EVENTOS];
static int n_eventos = 0;

// ------------------------------------------------- console lenta (-l N)
static long console_atraso = 0;      // N
static long console_ocupada = 0;     // instruções que faltam para ficar pronta

uint8_t __real_disp_le_byte(disp_t *d, uint16_t porta);
void __real_disp_escreve_byte(disp_t *d, uint16_t porta, uint8_t valor);
void __real_disp_tick(disp_t *d);

uint8_t __wrap_disp_le_byte(disp_t *d, uint16_t porta)
{
  uint8_t v = __real_disp_le_byte(d, porta);
  if (porta == PORTA_CONSOLE_ESTADO && console_ocupada > 0) v &= (uint8_t)~0x01;
  return v;
}

void __wrap_disp_escreve_byte(disp_t *d, uint16_t porta, uint8_t valor)
{
  __real_disp_escreve_byte(d, porta, valor);
  if (porta == PORTA_CONSOLE_DADOS) console_ocupada = console_atraso;
}

void __wrap_disp_tick(disp_t *d)
{
  if (console_ocupada > 0) console_ocupada--;
  __real_disp_tick(d);
}

static void descarrega_console(disp_t *disp)
{
  int tam;
  const char *s = console_saida(disp, &tam);
  if (tam > 0) {
    fwrite(s, 1, (size_t)tam, stdout);
    console_limpa_saida(disp);
  }
}

static void injeta(disp_t *disp, evento_t *ev)
{
  for (char *p = ev->texto; *p != '\0'; p++) console_poe_entrada(disp, *p);
  console_poe_entrada(disp, '\n');
  ev->feito = 1;
}

static int adiciona_evento(char *arg, int ocioso)
{
  char *dois_pontos = strchr(arg, ':');
  if (dois_pontos == NULL || n_eventos >= MAX_EVENTOS) {
    fprintf(stderr, "executor: evento invalido '%s' (use numero:texto)\n", arg);
    return 0;
  }
  *dois_pontos = '\0';
  eventos[n_eventos].valor = atol(arg);
  eventos[n_eventos].ocioso = ocioso;
  snprintf(eventos[n_eventos].texto, sizeof(eventos[n_eventos].texto), "%s", dois_pontos + 1);
  eventos[n_eventos].feito = 0;
  n_eventos++;
  return 1;
}

int main(int argc, char *argv[])
{
  const char *arquivo = NULL;
  const char *disco = NULL;
  long max_instr = 50000000L;
  int usa_ocioso = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
      max_instr = atol(argv[++i]);
    } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
      if (!adiciona_evento(argv[++i], 0)) return 1;
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      if (!adiciona_evento(argv[++i], 1)) return 1;
      usa_ocioso = 1;
    } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
      console_atraso = atol(argv[++i]);
    } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
      disco = argv[++i];
    } else if (arquivo == NULL) {
      arquivo = argv[i];
    } else {
      fprintf(stderr, "executor: argumento inesperado '%s'\n", argv[i]);
      return 1;
    }
  }

  if (arquivo == NULL) {
    fprintf(stderr, "uso: %s arquivo.mob [-n max_instrucoes] [-e instante:texto]... "
                    "[-o espera:texto]... [-l atraso_console] [-D disco]\n", argv[0]);
    return 1;
  }

  mem_t *mem = mem_cria();
  disp_t *disp = disp_cria(mem);
  cpu_t *cpu = cpu_cria(mem, disp);

  char erro[256];
  if (disco != NULL && !disp_monta_disco(disp, 0, disco, erro, sizeof(erro))) {
    fprintf(stderr, "executor: erro ao montar disco: %s\n", erro);
    return 1;
  }

  simbolo_t *simbolos = NULL;
  if (!obj_carrega(arquivo, mem, &simbolos, erro, sizeof(erro))) {
    fprintf(stderr, "executor: erro ao carregar '%s': %s\n", arquivo, erro);
    return 1;
  }
  bool achou = false, achou_rel = false;
  uint16_t end_ocioso = obj_busca_simbolo(simbolos, "_so_ocioso_laco", &achou);
  uint16_t end_relatorio = obj_busca_simbolo(simbolos, "_f_relatorio_final", &achou_rel);
  obj_libera_simbolos(simbolos);
  long inicio_relatorio = -1;
  if (usa_ocioso && !achou) {
    fprintf(stderr, "executor: -o precisa do simbolo _so_ocioso_laco no .mob\n");
    return 1;
  }

  cpu_liga(cpu);

  long n_supervisor = 0;   // passos começados em modo supervisor
  long n_ocioso = 0;       // ... dos quais no laço ocioso do SO
  long ocioso_acumulado = 0;   // desde a última injeção "-o"

  while (!cpu_parada(cpu) && cpu_num_instrucoes(cpu) < max_instr) {
    long agora = cpu_num_instrucoes(cpu);
    int no_ocioso = achou && cpu_r(cpu, 7) == end_ocioso && cpu_modo_supervisor(cpu);
    if (no_ocioso) ocioso_acumulado++;

    for (int i = 0; i < n_eventos; i++) {
      if (!eventos[i].feito && !eventos[i].ocioso && eventos[i].valor <= agora)
        injeta(disp, &eventos[i]);
    }
    // eventos "-o": só o primeiro ainda não feito, um de cada vez
    for (int i = 0; i < n_eventos; i++) {
      if (!eventos[i].feito && eventos[i].ocioso) {
        if (ocioso_acumulado >= eventos[i].valor) {
          injeta(disp, &eventos[i]);
          ocioso_acumulado = 0;
        }
        break;
      }
    }

    if (cpu_modo_supervisor(cpu)) n_supervisor++;
    if (no_ocioso) n_ocioso++;
    if (achou_rel && inicio_relatorio < 0 && cpu_r(cpu, 7) == end_relatorio) inicio_relatorio = agora;
    cpu_executa_1(cpu);
    descarrega_console(disp);
  }
  descarrega_console(disp);
  fflush(stdout);

  long total = cpu_num_instrucoes(cpu);
  fprintf(stderr, "[executor] %s apos %ld instrucoes (%ld em modo supervisor, "
                  "das quais %ld no laco ocioso)\n",
          cpu_parada(cpu) ? "CPU parada (halt)" : "limite de instrucoes atingido",
          total, n_supervisor, n_ocioso);
  if (inicio_relatorio >= 0)
    fprintf(stderr, "[executor] o SO comecou o relatorio final na instrucao %ld\n", inicio_relatorio);

  int parou = cpu_parada(cpu);
  cpu_destroi(cpu);
  disp_destroi(disp);
  mem_destroi(mem);
  return parou ? 0 : 2;
}
