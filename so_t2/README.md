# T2 — Implementação de processos (SO para o Mancha completo)

Um sistema operacional pequeno, que roda **na própria CPU simulada** do
Mancha, sobre a BIOS de `bios_mancha/`: processos em modo usuário,
multiprogramação, as 5 chamadas de sistema, bloqueio por E/S e por espera de
outro processo, três escalonadores (simples, round-robin e prioridade) e as
métricas pedidas. O núcleo é escrito em C (compilado pelo `mcc`) com uma
"ponte" pequena em assembly, no padrão de `bios_mancha/exemplos/multitarefa_c/`.

O relatório dos experimentos está em [`RELATORIO.md`](RELATORIO.md).

## Sumário

1. [Como compilar e rodar](#como-compilar-e-rodar)
2. [Onde está cada parte do enunciado](#onde-está-cada-parte-do-enunciado)
3. [Arquitetura: o caminho de uma interrupção](#arquitetura-o-caminho-de-uma-interrupção)
4. [Processos](#processos)
5. [Chamadas de sistema](#chamadas-de-sistema)
6. [Bloqueio e pendências (Parte II)](#bloqueio-e-pendências-parte-ii)
7. [Escalonadores (Partes I e III)](#escalonadores-partes-i-e-iii)
8. [O relógio do SO e as métricas](#o-relógio-do-so-e-as-métricas)
9. [Programas de usuário](#programas-de-usuário)
10. [O executor em lote](#o-executor-em-lote)
11. [Decisões de projeto e limitações](#decisões-de-projeto-e-limitações)
12. [Arquivos](#arquivos)

## Como compilar e rodar

A pasta `so_t2/` fica na raiz do projeto, ao lado de `bios_mancha/`,
`compilador_c/` e `simulador_completo/` (o Makefile usa `MANCHA=..`; se
estiver em outro lugar, use `make MANCHA=/caminho/do/mancha`).

```sh
make -C ..            # uma vez: compila simulador, montador, mcc e BIOS
make                  # monta so.mob (configuração padrão) e bin/executor
```

Configuração (cada `make` com valores diferentes regrava `config.h` e recompila):

| variável | valores | padrão |
|---|---|---|
| `ESC` | 1 = simples (Parte I), 2 = round-robin, 3 = prioridade | 2 |
| `QUANTUM` | quantum, em interrupções do relógio | 5 |
| `INTERVALO` | instruções entre interrupções do relógio (ver [restrição](#o-relógio-do-so-e-as-métricas)) | 4000 |
| `CENARIO` | argumento do init: 1 = testes, 2 = carga mista, 3 = só CPU | 2 |
| `VERBOSO` | 1 = o SO avisa na console quando cria/termina processos | 1 |

```sh
make ESC=3 QUANTUM=2 CENARIO=1      # prioridade, quantum 2, cenário de testes
```

**Simulador interativo** (o mesmo de sempre):

```sh
../simulador_completo/bin/simulador so.mob
```

Comandos `D0` e `C` para rodar rápido. Quando um processo lê da console (o
`eco`), o sistema fica ocioso esperando: digite com o comando `E<texto>`.
Quando o init morre, o SO imprime as métricas e executa `halt` (a janela da
console do simulador mostra só as últimas 6 linhas; para ver o relatório
inteiro use o executor em lote).

**Executor em lote** (mesmo núcleo de simulação, sem a tela curses — a saída
da console vai para a saída padrão):

```sh
make testes                         # cenário de testes, com as linhas "digitadas" automaticamente
./bin/executor so.mob -e 51111:um -e 152222:dois -e 253333:tres -e 354444:quatro
make experimentos                   # roda as 25 configurações do relatório (~4 s)
make custo                          # custo de cada entrada no núcleo, por tipo
```

## Onde está cada parte do enunciado

Tudo em `nucleo/so.c`, salvo indicação.

| requisito | onde |
|---|---|
| **Parte I** — estrutura de processo | `struct processo` (estado, pid, quadro de 16 palavras, motivo de bloqueio, prioridade, métricas) |
| tabela de processos | `struct processo tab[MAX_PROC]` (8 entradas) |
| init na primeira entrada | `so_inicia()` → `cria_processo(init, CENARIO)` |
| processo em execução, inclusive "nenhum" | `int atual` (índice em `tab`, **-1 = nenhum**: CPU ociosa) |
| salvar/recuperar o quadro de 16 palavras | `salva_contexto()` / `recupera_contexto()` (cópia em `copia_quadro`, `nucleo/ponte.asm`) |
| escalonador | `escalona()` (+ `despacha()`) |
| criação e morte | `cria_processo()`, `mata_processo()`, chamadas `SO_CRIA_PROC`/`SO_MATA_PROC` |
| pid ≠ entrada da tabela | `prox_pid` só cresce; entradas livres são reaproveitadas, pids não |
| biblioteca C adaptada | `usuario/biblioteca.c` (`putchar`, `getchar`, `puts`, `print_int`… sobre as chamadas) |
| **Parte II** — E/S bloqueante | `chamada_le()`, `chamada_escreve()`: se a console não está pronta, bloqueiam o processo sem fazer a E/S |
| tratamento de pendências | `trata_pendencias()`, chamada em toda interrupção |
| `SO_ESPERA_PROC` | `chamada_espera()`; o desbloqueio está em `mata_processo()` |
| **Parte III** — fila de prontos | `fila[]`, `fila_insere()` (sempre no fim), `fila_remove_pos()` |
| round-robin com quantum e preempção | `ESC_ROUND_ROBIN`; `trata_relogio()` decrementa `quantum_restante`, `preempta_atual()` |
| escalonador por prioridade | `ESC_PRIORIDADE`; `recalcula_prioridade()` |
| métricas | `struct metricas`, `relatorio_final()` (impresso quando o init morre) |
| relatório de execuções | `RELATORIO.md`, gerado a partir de `make experimentos` |

## Arquitetura: o caminho de uma interrupção

```
 processo (S=0, I=0)                   núcleo (S=1, D=1)
 ───────────────────                   ──────────────────────────────────────────────
 trap 7 / relógio / console / exceção
        │  a CPU empilha o quadro de 16 palavras em pilha_sistema-32
        ▼
 BIOS: despachante do trap 7 ─┐   BIOS: relógio (conta o tique) ─┐   vetor: 1..5, 8
        (tabela_syscalls)     │         (retomada_escalonador)   │   (instalados pelo SO)
        ▼                     ▼                                  ▼
 ponte.asm: stub ──► _so_entrada: r0 = nº da interrupção; sp = pilha_nucleo
                         │
                         ▼
 so.c: so_trata_interrupcao(irq, quadro)
        1. atualiza o relógio do SO          5. trata_pendencias()  (Parte II)
        2. salva_contexto(quadro)            6. escalona()
        3. trata: chamada / relógio /        7. recupera_contexto(quadro)
           exceção / console                    (ou: ninguém pronto → ocioso;
        4. o init morreu? → relatório, halt      init morto → halt)
                         │
                         ▼
 ponte.asm: sp = pilha_sistema-32 ; rete   ──►  processo escolhido (talvez outro)
            ou laço ocioso: ei ; jmp $      ──►  (até a próxima interrupção)
```

Pontos importantes:

- **A "pegadinha" da `pilha_sistema`.** Todo quadro do vetor usa o mesmo
  endereço de pilha, então o quadro do contexto interrompido aparece sempre
  em `pilha_sistema-32` e é sobrescrito na próxima interrupção. O núcleo
  copia esse quadro para a entrada do processo na tabela logo na entrada
  e **troca para `pilha_nucleo`** antes de chamar qualquer C; nada do núcleo
  sobrevive em `pilha_sistema` a um `rete`. Antes do `rete`, o quadro do
  processo escolhido é copiado de volta para `pilha_sistema-32`.
- **Uso da BIOS.** As 5 chamadas são registradas em `tabela_syscalls` (a
  convenção da BIOS: número em `r0`, argumentos em `r1..r4`, `trap 7`), todas
  apontando para o mesmo stub. Como o despachante da BIOS chama o handler
  com `call` e faria `rete` para o **mesmo** processo — e uma chamada
  bloqueante precisa passar a CPU para outro —, o stub descarta o endereço
  de retorno (`add sp, 2`) e segue para o caminho comum do núcleo. O
  relógio usa o gancho `retomada_escalonador` (a BIOS conta o tique e
  salta para o SO). O SO instala no vetor tratadores próprios só para as
  **exceções 1–5** (a BIOS pararia a máquina inteira; o SO mata só o
  processo culpado) e para a **console (8)** (a BIOS guardaria o caractere
  num buffer e voltaria sem avisar o SO; aqui a chegada de entrada entra no
  núcleo e desbloqueia leitores na hora).
- **CPU ociosa.** Se ninguém está pronto, `_so_ocioso` liga as interrupções
  e gira num `jmp` para si mesmo, em modo supervisor, na pilha do núcleo.
  A próxima interrupção entra no núcleo normalmente; o quadro do laço ocioso
  é descartado (`atual == -1`, não há processo para guardá-lo).
- **Exceção dentro do próprio núcleo** (ou do laço ocioso) → pânico: o SO
  imprime a exceção e o `ip` e para.

## Processos

```c
struct processo {
    int estado;               // EST_LIVRE, EST_PRONTO, EST_BLOQUEADO, EST_EXECUTANDO
    int pid;
    int quadro[16];           // r0..r4, bp, sp, ip, sr, s1..s3, cs, cl, ds, dl
    int motivo;               // BLOQ_LE, BLOQ_ESCREVE, BLOQ_ESPERA
    int arg_bloqueio;         // pid esperado, ou o caractere a escrever
    int ordem_bloqueio;       // atende bloqueados por ordem de chegada
    int prioridade;           // em milésimos (0..1000), menor = mais prioritário
    ...                       // + instantes e struct metricas
};
```

**Criação** (`cria_processo`). Todo programa é uma função C `void
programa(int arg)`. O SO monta uma pilha inicial "de mentira", como se a
função tivesse sido chamada: `(sp)` = endereço de `proc_retorno`
(`usuario/usuario.asm`) e `(sp+2)` = `arg` — depois do prólogo do `mcc`,
`(bp+4)` é o argumento. Quando a função **retorna**, cai em `proc_retorno`,
que faz `SO_MATA_PROC(0)`: o processo termina sozinho. O quadro inicial tem
`ip` = endereço da função, `sp` = topo da pilha do processo (512 bytes por
processo, reservados em `nucleo/dados.asm`) e **`sr = 0`: S=0 (modo
usuário), I=0 (sem `in`/`out`), D=0 (interrupções ligadas)**, P=0. Assim,
`halt`, `ei`, `di`, `lds`, `rete` e qualquer `in`/`out` num processo causam
a interrupção 2, e o SO mata o processo (ver o passo [2] dos testes).

**Pid × entrada da tabela.** O pid vem de `prox_pid`, que só cresce. Quando
um processo morre, sua entrada volta a `EST_LIVRE` e pode ser reusada por
outro processo, com outro pid — no passo [6] dos testes, 7 processos ocupam
as entradas livres com pids 10 a 16 (a tabela tem 8 entradas).

**Morte** (`mata_processo`). Tira o processo da fila (se pronto), fecha a
contagem de tempo do último estado, guarda as métricas em `hist[]`,
desbloqueia quem estava esperando por ele e, se for o init (pid 1), marca o
desligamento.

## Chamadas de sistema

Número em `r0`, argumentos em `r1..r4`, `trap 7`, resultado em `r0` (o SO
escreve o resultado no `r0` do **quadro salvo** do processo; o `rete` o
restaura). Do lado do processo, `usuario/usuario.asm` tem uma função C para
cada chamada.

| nº | chamada | argumentos | resultado |
|---|---|---|---|
| 1 | `SO_LE` | — | caractere lido. Bloqueia se não há entrada |
| 2 | `SO_ESCREVE` | `r1` = caractere | 0. Bloqueia se a console não está pronta |
| 7 | `SO_CRIA_PROC` | `r1` = nome do programa, `r2` = argumento | pid do novo processo; -1 se o programa não existe ou a tabela está cheia |
| 8 | `SO_MATA_PROC` | `r1` = pid (0 = o próprio) | 0; -1 se não existe processo com esse pid |
| 9 | `SO_ESPERA_PROC` | `r1` = pid | 0 quando o processo morrer; -1 se o pid é o do próprio processo ou nunca existiu; 0 na hora se o processo já terminou |

`SO_CRIA_PROC` recebe o **nome** do programa (`so_cria_proc("eco", 4)`): o
SO procura numa tabela `programas[]` que associa nomes a funções.

## Bloqueio e pendências (Parte II)

- `SO_LE`: se a console tem caractere (bit 1 da porta de estado) **e** não
  há outro leitor esperando na frente, lê e devolve. Senão, o processo vai
  para `EST_BLOQUEADO` com motivo `BLOQ_LE` e a leitura **não** é feita.
- `SO_ESCREVE`: igual, com o bit 0 (pronta para enviar); o caractere fica
  guardado em `arg_bloqueio`.
- `trata_pendencias()`, chamada em **toda** interrupção: enquanto houver
  leitor bloqueado e entrada disponível, lê um caractere e o entrega (no `r0`
  do quadro) ao leitor bloqueado há mais tempo, desbloqueando-o; o mesmo
  para escritores e a console pronta. Contadores `n_bloq[motivo]` evitam
  varrer a tabela quando não há ninguém bloqueado (sem isso, essa varredura
  sozinha consumia ~40% da CPU — o `mcc` não otimiza nada).
- `SO_ESPERA_PROC` bloqueia com motivo `BLOQ_ESPERA`; `mata_processo()`
  desbloqueia (com 0) todos que esperavam o pid que morreu.
- Um processo desbloqueado vai para o **fim** da fila de prontos.

A console do simulador está **sempre** pronta para escrever (`dispositivos.c`
devolve o bit 0 sempre em 1), então em execução normal `SO_ESCREVE` nunca
bloqueia. Para exercitar esse caminho, o executor em lote tem a opção
`-l N` ("console lenta": N instruções por caractere) — usada no grupo E do
relatório.

## Escalonadores (Partes I e III)

Nos três, se o processo corrente ainda pode executar (não bloqueou, não
morreu, não perdeu o quantum), ele continua. Quando precisa escolher outro:

| escalonador | escolha | preempção |
|---|---|---|
| `ESC_SIMPLES` (Parte I) | o primeiro processo pronto **na ordem da tabela** | nenhuma |
| `ESC_ROUND_ROBIN` | o primeiro da fila de prontos | fim do quantum → fim da fila |
| `ESC_PRIORIDADE` | o de **menor** valor de prioridade entre os prontos (empate: o que está há mais tempo na fila) | fim do quantum → fim da fila |

**Quantum.** `quantum_restante` recebe `QUANTUM` sempre que um processo é
escolhido da fila e é decrementado a cada interrupção do relógio; ao chegar
a 0, o processo vai para o fim da fila (`preempta_atual`) e conta uma
preempção.

**Prioridade.** Quando um processo perde o processador (bloqueou ou acabou o
quantum):

```
prio  = (prio + t_exec / t_quantum) / 2          prio inicial = 0,5
t_exec = QUANTUM - quantum_restante               (em interrupções do relógio)
```

Sem ponto flutuante no `mcc`, `prio` é guardada em milésimos:
`prio = (prio + t_exec*1000/QUANTUM) / 2`, com `prio` inicial = 500.
Quem bloqueia cedo (E/S) tem `t_exec` pequeno → `prio` cai → é escolhido
antes. Quem sempre gasta o quantum inteiro converge para 1000.

## O relógio do SO e as métricas

**Tempo em instruções.** O relógio do Mancha conta instruções e volta a 0 ao
atingir o limite (o `INTERVALO`), gerando a interrupção 10. O SO calcula

```
agora = voltas * INTERVALO + contador
```

onde `voltas` é contada pelas interrupções do relógio. Sutileza: o núcleo roda
com D=1; se o contador der a volta logo depois de entrarmos no núcleo por
outro motivo, a interrupção do relógio fica pendente e o contador lido já
recomeçou de perto de 0. O SO detecta isso (contador **menor** que na
leitura anterior, sem interrupção de relógio entre as duas), conta a volta
na hora e não a conta de novo quando a interrupção pendente chegar
(`atualiza_relogio`).

Como `int` tem 16 bits, os tempos são guardados em `struct tempo`
(`alto*10000 + baixo`), com soma, subtração e divisão escritas à mão.

**Restrição: o intervalo do relógio tem que ser maior que o caminho mais
longo do núcleo.** O núcleo roda na mesma CPU simulada que os processos; se
uma entrada no núcleo durar mais que `INTERVALO` instruções, o contador dá
duas voltas com D=1 e o hardware junta as duas numa interrupção só — o SO
perde uma volta (e um tique do quantum). A entrada mais longa medida nas
execuções do relatório foi de 3.648 instruções (console lenta); com a console
normal, ~2.900 (`SO_MATA_PROC`). Por isso o padrão é 4000. O grupo D do
relatório mostra o que acontece com 2000.

**Métricas** (impressas por `relatorio_final()` quando o init morre):

- número de processos criados; tempo total de execução; tempo ocioso (período
  sem nenhum processo pronto ou executando) e quantas vezes ficou ocioso;
- interrupções recebidas de cada tipo (e chamadas de sistema de cada tipo);
- número de preempções (total e por processo) e trocas de contexto;
- por processo: tempo de retorno (término − criação); preempções; quantas
  vezes entrou em pronto, bloqueado e executando; tempo total em cada um;
  tempo médio de resposta (desbloqueio → ser escalonado);
- extra: tempo gasto dentro do núcleo e a maior entrada no núcleo.

## Programas de usuário

`usuario/programas.c` (todos `void nome(int arg)`):

| programa | o que faz |
|---|---|
| `init` | primeiro processo; o argumento escolhe o cenário |
| `cpu(n)` | limitado por CPU: n unidades de cálculo (~4000 instruções cada), imprime só no início e no fim |
| `eco(n)` | limitado por E/S: lê n linhas da console e as repete |
| `escritor(n)` | escreve n linhas, com 1 unidade de cálculo entre elas |
| `proibido(t)` | tenta `outb` direto (t=0), `halt` (t=1) ou divisão por zero (t=2) |
| `laco` | laço infinito, para ser morto por outro processo |
| `filho(n)` | calcula um pouco e termina retornando de `main` |
| `pai(n)` | cria n filhos e espera por eles |

Cenários do init: **1 = testes** (todas as chamadas e os casos de erro, em 8
passos), **2 = misto** (`cpu(40)`, `eco(4)`, `cpu(20)`, `escritor(12)`),
**3 = só CPU** (`cpu(40)`, `cpu(30)`, `cpu(20)`, `cpu(10)`).

Saída de `make testes` (round-robin, trechos):

```
init: [1] erros nas chamadas
  espera_proc(1), o proprio init -> -1
  espera_proc(99), nao existe -> -1
  mata_proc(99), nao existe -> -1
  cria_proc(xyz), programa inexistente -> -1
init: [2] protecao (S=0, I=0): o SO mata quem tenta
proibido: vou fazer outb direto na console...
[SO] pid 2 (proibido) morto: instrucao privilegiada em ip=55AA
  espera_proc(proibido) -> 0
...
init: [6] tabela cheia e pid != entrada da tabela
  cria_proc(laco) -> 10
  ...
  cria_proc(laco) -> 16
  cria_proc(laco) -> -1
```

Todos os processos escrevem na mesma console, então as saídas se misturam
— inclusive no meio de uma linha, quando a preempção cai entre duas
chamadas `SO_ESCREVE`. No passo [7], dois `eco` leem da mesma console ao
mesmo tempo: cada `SO_LE` entrega **um** caractere, ao leitor bloqueado há
mais tempo, então as duas linhas digitadas aparecem repartidas entre os dois.
É o comportamento correto para uma console compartilhada sem disciplina de
linha (o enunciado define `SO_LE` por caractere).

## O executor em lote

`ferramentas/executor.c` liga o mesmo `cpu.c`/`dispositivos.c`/… do
simulador, sem a tela curses:

```
bin/executor arquivo.mob [-n max] [-e T:texto]... [-o E:texto]... [-l N]
  -n max      limite de instruções (padrão 50 milhões)
  -e T:texto  "digita" texto+\n no instante T (em instruções)
  -o E:texto  "digita" texto+\n depois de E instruções de CPU ociosa (o
              usuário responde quando o sistema fica esperando por ele)
  -l N        console lenta: N instruções por caractere escrito
```

No fim, informa em `stderr` o total de instruções, quantas em modo
supervisor, quantas no laço ocioso e em que instrução o SO começou o
relatório final (para conferir o relógio do SO contra a contagem real).
`-l` é implementado sem alterar o simulador: o executor é ligado com
`-Wl,--wrap` para interceptar `disp_le_byte`/`disp_escreve_byte`/`disp_tick`.

## Decisões de projeto e limitações

- **Sem proteção de memória.** Os processos rodam com segmentação plana
  (`cs=ds=0`, limites 0), no mesmo espaço do núcleo — o `mcc` só gera
  desvios absolutos, e todo o código é montado num módulo só. A proteção
  pedida neste trabalho (E/S e instruções privilegiadas) está garantida por
  S=0 e I=0; isolamento de memória é assunto de outro trabalho.
- **`SO_ESPERA_PROC` em processo que já terminou devolve 0** (sem bloquear).
  O enunciado manda devolver erro se o processo "não existe"; aqui isso vale
  para pids que nunca existiram. Devolver -1 também para quem já terminou
  tornaria o resultado dependente do escalonamento: um filho curto pode
  terminar antes de o pai chamar `SO_ESPERA_PROC` (aconteceu nos testes com
  o escalonador de prioridade).
- **Prioridade não preempta na chegada.** Um processo que fica pronto com
  prioridade melhor espera o corrente bloquear ou acabar o quantum ("semelhante
  ao circular", como diz o enunciado).
- **Custo do núcleo.** O núcleo é C sem otimização: ~600 instruções por
  chamada de sistema e ~700 por interrupção do relógio. Com `SO_ESCREVE`
  caractere a caractere, isso dá de 26% a 64% do tempo dentro do SO nas
  cargas do relatório. As partes quentes que não ficam claras em C (cópia de quadro,
  zerar memória, ler o relógio) estão em assembly em `ponte.asm`.
- `trap 0` num processo reinicia o sistema (o quadro 0 da BIOS aponta para
  `_start`); não foi alterado para não quebrar o comando `R` do simulador.
- Limites: 8 processos simultâneos, 24 processos no histórico de métricas,
  intervalo do relógio até ~20000 instruções.

## Arquivos

```
so_t2/
  Makefile               monta so.mob; gera config.h; alvos testes/experimentos
  comum.h                números das chamadas, escalonadores e cenários
  nucleo/so.c            o núcleo (C)
  nucleo/ponte.asm       _kernel_inicio, stubs de interrupção, _so_entrada,
                         laço ocioso, acesso a portas, cópia de quadro
  nucleo/dados.asm       pilhas dos processos (montado por último)
  usuario/usuario.h      interface dos processos com o SO
  usuario/usuario.asm    as chamadas de sistema (trap 7) e proc_retorno
  usuario/biblioteca.c   putchar/getchar/puts/print_int... sobre as chamadas
  usuario/programas.h/.c init e os programas de teste
  ferramentas/executor.c executor em lote
  ferramentas/experimentos.sh  roda as configurações do relatório
  ferramentas/resume.py  gera resultados/resumo.md a partir das saídas
  ferramentas/custo_nucleo.c   mede o custo de cada entrada no núcleo
  resultados/            saída completa de cada execução + resumo.md
  RELATORIO.md           relatório das execuções
```

Ordem de montagem: `bios.asm ponte.asm so.asm usuario.asm biblioteca.asm
programas.asm dados.asm` (a BIOS primeiro, por causa do vetor no endereço 0;
`dados.asm` por último, porque marca o fim da imagem, que `_kernel_inicio`
confere estar abaixo da pilha do núcleo).
