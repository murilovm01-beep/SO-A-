# Relatório de execuções — T2 (processos)

Execuções do SO de `so_t2/` (ver [`README.md`](README.md)) em diferentes
configurações de escalonador, quantum, intervalo do relógio, carga e
velocidade da console. Todos os números saem do relatório de métricas que o
próprio SO imprime quando o init morre; as tabelas completas estão em
[`resultados/resumo.md`](resultados/resumo.md) e a saída integral de cada
execução em `resultados/<nome>.txt`.

## 1. Como as medições foram feitas

- **Unidade de tempo: instruções**, medidas pelo relógio do Mancha
  (`agora = voltas × intervalo + contador`, ver README). Inclui as
  instruções do próprio núcleo, que roda na mesma CPU simulada.
- **Execução em lote** com `bin/executor` (o mesmo núcleo de simulação do
  simulador interativo, sem a tela). O simulador não tem aleatoriedade e a
  entrada da console é injetada em instantes fixos, então cada execução é
  **determinística**: `make experimentos` reproduz exatamente estes números
  (25 execuções, ~4 s).
- **Cargas** (escolhidas pelo argumento do init):
  - **misto**: `cpu(40)` e `cpu(20)` (limitados por CPU, 40 e 20 unidades de
    ~4000 instruções), `eco(4)` (limitado por E/S: lê 4 linhas da console) e
    `escritor(12)` (12 linhas com 1 unidade de cálculo entre elas). O
    "usuário" digita as 4 linhas do `eco` nos instantes 51.111, 152.222,
    253.333 e 354.444 (ver a observação abaixo).
  - **cpu**: `cpu(40)`, `cpu(30)`, `cpu(20)`, `cpu(10)`, criados nessa ordem.
- **Configuração base**: intervalo do relógio 4000 instruções, quantum 5
  interrupções (fatia de 20000 instruções), `VERBOSO=0`.
- **Tempo de resposta** é medido como o enunciado define: do desbloqueio até
  o processo ser escalonado. Processos que nunca bloqueiam aparecem com "-".
- **Tempo no núcleo** é medido pelo próprio SO, da leitura do relógio na
  entrada até o fim do tratamento; não inclui as ~100 instruções do caminho
  em assembly (BIOS + ponte), então é uma estimativa por baixo.

Fórmulas usadas nas tabelas:

```
retorno(p)        = término(p) − criação(p)
resposta(p)       = Σ (escalonado − desbloqueado) / nº de desbloqueios
núcleo %          = tempo no núcleo / tempo total
ocioso %          = tempo sem nenhum processo pronto ou executando / tempo total
prio (prioridade) = (prio + t_exec/t_quantum) / 2,  t_exec = quantum − quantum_restante
```

**Observação sobre os instantes da entrada.** A primeira versão dos
experimentos digitava as linhas em 50.000, 150.000, 250.000 e 350.000. Como
100.000 é múltiplo de 4000, todas chegavam na **mesma fase** do relógio:
logo depois de um tique, com o núcleo ainda tratando a interrupção do
relógio (D=1). A entrada era vista pelas pendências dessa mesma interrupção,
e quando o tique também era uma preempção o `eco` era escolhido na hora — com
prioridade e quantum 1, a resposta média saía 337 instruções em vez de ~2000.
Com instantes "quebrados", as linhas chegam em fases diferentes do relógio e
o viés some. (O SO estava certo nos dois casos — a entrada que chega durante
uma interrupção é tratada nas pendências dela; o problema era o experimento
sempre cair no mesmo caso.)

## 2. Conferência das medições

Antes dos resultados, duas verificações em **todas** as 25 execuções
(tabela completa no fim de `resultados/resumo.md`):

1. **Σ (tempo executando de cada processo) + tempo ocioso = tempo total**,
   exatamente, em todas. O tempo de núcleo fica contabilizado no processo
   que estava executando (ou no tempo ocioso).
2. **Relógio do SO × contagem real.** O executor anota em que instrução real
   o SO começou o relatório final. A diferença para o tempo total que o SO
   mediu é o boot (antes de o SO zerar o relógio) mais o resto da última
   chamada: **2.612 a 2.862 instruções em 24 das 25 execuções** — o relógio
   do SO não perde nada ao longo de execuções de até 3,6 milhões de
   instruções.

   A exceção é `D_misto_rr_L2000_q10` (intervalo 2000): diferença de 6.676,
   ou seja, **duas voltas do relógio (2 × 2000 instruções) perdidas**. Ver a
   seção 7.

## 3. Os três escalonadores, carga mista (grupo A)

Quantum 5 × 4000.

| escalonador | tempo total | núcleo | preempções | trocas de contexto |
|---|---:|---:|---:|---:|
| simples | 658.497 | 37,0% | 0 | 9 |
| round-robin | 698.861 | 40,3% | 30 | 41 |
| prioridade | 706.635 | 40,9% | 30 | 40 |

Tempo de retorno:

| escalonador | cpu(40) | eco(4) | cpu(20) | escritor(12) | média |
|---|---:|---:|---:|---:|---:|
| simples | 216.375 | 408.841 | 385.334 | 627.495 | 409.511 |
| round-robin | 643.784 | 407.325 | 457.278 | 667.779 | 544.041 |
| prioridade | 649.635 | 367.836 | 456.184 | 675.642 | 537.324 |

O processo limitado por E/S:

| escalonador | resposta média do eco | vezes bloqueado | tempo bloqueado | tempo pronto |
|---|---:|---:|---:|---:|
| simples | 50.645 | 1 | 72.286 | 268.835 |
| round-robin | 49.862 | 4 | 115.792 | 223.512 |
| prioridade | **9.911** | 4 | 235.319 | **63.701** |

**Simples (Parte I).** Sem preempção, quem pega a CPU fica com ela até
bloquear ou terminar. `cpu(40)` roda inteiro primeiro e termina em 216 mil
instruções; o `eco` passa 269 mil instruções **pronto, sem executar**. As
linhas digitadas se acumulam no buffer da console, e quando o `eco`
finalmente roda ele lê duas de uma vez sem bloquear (bloqueou 1 vez só,
contra 4 nos outros). É o menor tempo total e o melhor retorno médio (menos
trocas de contexto, menos núcleo), e o pior para interatividade.

**Round-robin.** Divide a CPU por igual: os processos CPU-bound terminam
bem mais tarde (`cpu(40)`: 644 mil contra 216 mil) e o retorno médio piora
33%. O `eco` desbloqueado vai para o fim da fila e espera, em média, o resto
da fatia do processo corrente (meia fatia) mais as fatias inteiras dos dois
que estão na fila: 2,5 × 20000 = 50 mil instruções, exatamente a resposta
média medida (49.862).

**Prioridade.** O `eco` bloqueia logo depois de ganhar a CPU, então
`t_exec/t_quantum ≈ 0` e sua prioridade cai a cada bloqueio (fica com valor
menor = mais prioritário); os processos CPU-bound gastam o quantum inteiro e
sobem para perto de 1. Resultado: ao desbloquear, o `eco` passa na frente —
**resposta média 5× menor que no round-robin**, tempo pronto 3,5× menor, e o
`eco` termina 39 mil instruções antes. Os processos CPU-bound quase não
sentem diferença (retornos a no máximo 1,2% dos do round-robin).

## 4. Variando o quantum (grupo B)

Carga mista, intervalo 4000.

| escalonador | quantum | tempo total | núcleo | preempções | retorno médio | resposta média do eco |
|---|---:|---:|---:|---:|---:|---:|
| round-robin | 1 | 886.844 | 51,9% | 221 | 666.422 | 9.618 |
| round-robin | 2 | 763.957 | 45,0% | 93 | 591.557 | 19.689 |
| round-robin | 5 | 698.861 | 40,3% | 30 | 544.041 | 49.862 |
| round-robin | 10 | 678.075 | 38,6% | 14 | 527.354 | 99.182 |
| round-robin | 20 | 663.157 | 37,4% | 5 | 504.239 | 201.334 |
| prioridade | 1 | 945.677 | 54,6% | 236 | 687.822 | 2.032 |
| prioridade | 2 | 781.669 | 46,1% | 94 | 580.697 | 5.176 |
| prioridade | 5 | 706.635 | 40,9% | 30 | 537.324 | 9.911 |
| prioridade | 10 | 683.986 | 39,1% | 14 | 531.776 | 23.327 |
| prioridade | 20 | 674.012 | 38,3% | 5 | 509.845 | 36.339 |

O compromisso clássico aparece com nitidez:

- **Quantum pequeno** → muitas preempções e trocas de contexto, mais tempo no
  núcleo (52% com quantum 1) e execução 34% mais longa que com quantum 20; em
  troca, o `eco` responde rápido. No round-robin a resposta é praticamente
  **proporcional ao quantum**: ~10 mil instruções por interrupção de quantum
  (9,6 mil com quantum 1 → 201 mil com quantum 20), as mesmas 2,5 fatias de
  espera da seção 3.
- **Quantum grande** → o round-robin se aproxima do escalonador simples: menos
  custo, melhor retorno médio, interatividade ruim.
- O escalonador por **prioridade quebra esse compromisso** para o processo de
  E/S: com quantum 20, a resposta do `eco` (36,3 mil) é 5,5× melhor que a do
  round-robin com o mesmo quantum e ainda melhor que a do round-robin com
  quantum 5 (49,9 mil). Com quantum 1 ela cai a ~2 mil instruções: em média
  meio intervalo do relógio — o tempo até o próximo tique, quando o processo
  corrente perde a CPU e o `eco`, mais prioritário, é o escolhido.

## 5. Só processos limitados por CPU (grupo C)

`cpu(40)`, `cpu(30)`, `cpu(20)`, `cpu(10)`, nessa ordem; quantum 5 salvo
indicação.

| execução | cpu(40) | cpu(30) | cpu(20) | cpu(10) | retorno médio | tempo total | núcleo |
|---|---:|---:|---:|---:|---:|---:|---:|
| simples | 214.804 | 379.919 | 502.611 | 582.350 | 419.921 | 613.285 | 25,7% |
| round-robin | 613.054 | 559.901 | 488.539 | 381.570 | 510.766 | 641.946 | 28,8% |
| prioridade | 620.763 | 566.507 | 490.467 | 382.461 | 515.049 | 649.565 | 29,6% |
| round-robin, quantum 1 | 779.711 | 703.275 | 592.263 | 415.250 | 622.624 | 815.079 | 42,9% |
| round-robin, quantum 20 | 580.848 | 587.066 | 520.131 | 318.152 | 501.549 | 616.880 | 26,1% |

- O escalonador simples executa na ordem de criação, o **maior primeiro** — a
  pior ordem possível para o retorno médio sem preempção. Mesmo assim o
  round-robin não ganha: com trabalhos de 40, 30, 20 e 10 unidades, a conta
  ideal (sem custo de troca) dá retorno médio **igual** nos dois:

  ```
  ordem de criação:  (40 + 70 + 90 + 100) / 4 = 75 unidades
  round-robin:       cpu(10) acaba em 4×10 = 40, cpu(20) em 40+3×10 = 70,
                     cpu(30) em 70+2×10 = 90, cpu(40) em 100 → 75 unidades
  ```

  O que sobra é o custo das preempções e trocas de contexto, que faz o
  round-robin perder (511 mil × 420 mil). O que o round-robin muda é **quem**
  espera: inverte a ordem de término, e o trabalho curto `cpu(10)` termina
  em 382 mil em vez de 582 mil.
- **Prioridade ≈ round-robin** quando todos são CPU-bound: todos gastam o
  quantum inteiro, as prioridades convergem para o mesmo valor (perto de 1)
  e o desempate é a ordem na fila — o próprio round-robin.

## 6. Custo do núcleo

O núcleo é C compilado pelo `mcc`, que não otimiza nada (um `i < 600` vira
~12 instruções), e roda na mesma CPU simulada que os processos. Custo de cada
entrada no núcleo, de quando a CPU sai do modo usuário até o `rete` (inclui o
despachante da BIOS e os stubs), medido com `make custo`
(`ferramentas/custo_nucleo.c`) na carga mista, round-robin, quantum 5 × 4000:

| entrada | nº | média | máximo |
|---|---:|---:|---:|
| `SO_ESCREVE` | 373 | 611 | 1.545 |
| `SO_LE` | 20 | 769 | 1.464 |
| interrupção do relógio | 174 | 698 | 1.677 |
| interrupção da console | 4 | 898 | 1.361 |
| `SO_CRIA_PROC` | 4 | 1.862 | 2.332 |
| `SO_ESPERA_PROC` | 4 | 1.318 | 1.612 |
| `SO_MATA_PROC` | 4 | 2.535 | 2.873 |

(Os máximos das entradas comuns incluem as vezes em que, na mesma entrada, o
SO também preemptou, desbloqueou ou trocou de processo.)

Como cada caractere escrito é uma chamada de sistema, na configuração base a
carga mista passa 37–41% do tempo no núcleo (só CPU: 26–30%). Duas
otimizações do desenvolvimento valeram a pena registrar:

- a varredura da tabela atrás de processos bloqueados a cada interrupção
  chegou a ocupar **44% de toda a execução**; contadores de bloqueados por
  motivo (`n_bloq[]`) zeraram esse custo quando não há ninguém esperando;
- a cópia do quadro de 16 palavras, feita 2 vezes por interrupção, caiu de
  ~115 instruções (laço de `bios_copia_quadro`) para ~40 com o laço
  desenrolado em `ponte.asm`.

## 7. Intervalo do relógio (grupo D)

Round-robin, carga mista, **mesma fatia de 20000 instruções** com relógios
de frequências diferentes:

| intervalo × quantum | interrupções do relógio | tempo total | núcleo | retorno médio |
|---|---:|---:|---:|---:|
| 2000 × 10 | 411 | 823.257 * | 43,0% | 628.768 |
| 4000 × 5 | 174 | 698.861 | 40,3% | 544.041 |
| 10000 × 2 | 63 | 637.236 | 37,8% | 510.217 |
| 20000 × 1 | 31 | 627.131 | 37,7% | 504.815 |

\* medição com erro: o relógio do SO perdeu 2 voltas (ver abaixo).

- Para a mesma fatia, um relógio mais frequente só traz custo: cada
  interrupção do relógio a mais aumentou o tempo total em ~500 instruções
  (de 4000 para 2000: +237 interrupções, +124 mil instruções). Um intervalo
  maior deixa as medições do quantum mais grossas (o quantum só pode ser
  múltiplo dele), mas aqui isso não teve efeito visível no escalonamento.
- **Intervalo 2000 é menor que a entrada mais longa do núcleo** (~2.700 a
  2.900 instruções, em `SO_MATA_PROC`). Quando uma entrada dessas começa com
  o contador já adiantado, o contador dá duas voltas com interrupções
  desligadas, o hardware junta as duas numa interrupção só e o SO perde uma
  volta. Aconteceu duas vezes nesta execução (diferença de 6.676 na
  conferência da seção 2, contra 2.612–2.862 nas outras); a "maior entrada"
  impressa (1.553) também está errada, porque é medida módulo o intervalo.
  Por isso o padrão do SO é 4000: **o intervalo do relógio tem que ser maior
  que o caminho mais longo do núcleo**.

## 8. Console lenta: bloqueio na escrita (grupo E)

A console do Mancha está sempre pronta para escrever, então nas execuções
anteriores `SO_ESCREVE` nunca bloqueia. Aqui o executor simula uma console
que leva **2000 instruções por caractere** (`-l 2000`); carga mista, quantum
5 × 4000.

| escalonador | tempo total | núcleo | ocioso | trocas de contexto | retorno médio | retorno do eco |
|---|---:|---:|---:|---:|---:|---:|
| simples | 1.406.043 | 61,7% | 11,2% | 161 | 758.315 | 473.925 |
| round-robin | 1.546.076 | 60,3% | 20,2% | 188 | 928.224 | 602.046 |
| prioridade | 1.433.077 | 63,8% | 8,1% | 251 | 897.642 | 425.610 |

- Agora **todo** processo vira parcialmente limitado por E/S: o `escritor`
  bloqueou 155–187 vezes, cada `cpu` 41–42 vezes (só para escrever suas duas
  linhas), e aparece **tempo ocioso** — momentos em que todos estão
  esperando a console.
- A console é o gargalo: são 373 caracteres × 2000 = 746 mil instruções de
  console ocupada, em série. O melhor escalonamento é o que mantém a console
  sempre ocupada e usa o tempo de espera dela para calcular. O escalonador
  por prioridade faz isso naturalmente (quem escreve bloqueia cedo, ganha
  prioridade e volta logo a escrever, enquanto os CPU-bound preenchem os
  intervalos): menor ociosidade (8,1%), e o `eco` é o primeiro a terminar. O
  round-robin faz o contrário: os CPU-bound terminam o cálculo intercalados
  com os demais, e no fim só sobra o `escritor` — 20% de CPU ociosa.
- **Latência das pendências.** A console do Mancha não gera interrupção
  quando fica pronta para enviar (só quando chega entrada). Então um
  escritor bloqueado só é atendido na próxima entrada no núcleo — no pior
  caso, a próxima interrupção do relógio, até 4000 instruções depois, embora
  a console tenha ficado pronta após 2000. É o custo de tratar pendências
  por consulta (`trata_pendencias()` a cada interrupção). Isso também infla
  o tempo no núcleo (60–64%): cada caractere passa por pelo menos duas
  entradas (a chamada que bloqueia e a pendência que o escreve).

## 9. Testes funcionais

O cenário 1 (`make testes`) passou nos três escalonadores (saídas em
`resultados/testes_*.txt`):

| passo | o que verifica | resultado |
|---|---|---|
| [1] | `espera_proc` do próprio processo e de pid inexistente, `mata_proc` de pid inexistente, `cria_proc` de programa inexistente | todos devolvem -1, sem bloquear |
| [2] | processo tenta `outb`, `halt`, divisão por zero | o SO mata só o processo (interrupções 2, 2 e 3) e o init segue |
| [3] | processo termina retornando de `main` | `proc_retorno` faz `SO_MATA_PROC(0)`; `espera_proc` → 0 |
| [4] | matar outro processo (laço infinito) | `mata_proc` → 0; `espera_proc` dele depois → 0 sem bloquear |
| [5] | processo que cria processos e espera por eles | pai cria 2 filhos e espera os dois |
| [6] | tabela cheia; pid ≠ entrada | 7 processos recebem os pids 10–16 nas entradas livres; o 8º → -1 |
| [7] | leitura bloqueante com 2 leitores | os dois bloqueiam; ao chegar entrada, cada caractere vai para o leitor bloqueado há mais tempo |
| [8] | matar processo bloqueado na leitura | `mata_proc` → 0, contadores de bloqueados ficam consistentes |

Um efeito visível do escalonador por prioridade nos testes: no passo [6] o
init cria 7 processos `laco` e logo depois os mata. Os `laco` começam com
prioridade 0,5; o init, que vinha gastando quanta inteiros escrevendo na
console, está perto de 1. Então os `laco` rodam primeiro, cada um até a
prioridade subir acima da do init, e só então o init volta e os mata — a
execução dos testes leva 3,6 milhões de instruções com prioridade contra
2,4 milhões no round-robin e 1,5 milhão no simples (em que os `laco` nem
chegam a executar).

## 10. Conclusões

1. **Não há escalonador melhor em tudo.** O simples (sem preempção) teve o
   menor tempo total e o melhor retorno médio nas cargas com a console
   normal, e a pior interatividade. O round-robin é justo, mas paga em
   trocas de contexto e atrasa os trabalhos longos.
2. **O quantum é o botão principal do round-robin**: entre 1 e 20
   interrupções, a resposta do processo de E/S variou 21× (proporcional ao
   quantum) e o tempo no núcleo foi de 52% a 37%.
3. **O escalonador por prioridade** (média do quanto do quantum o processo
   usa) separa sozinho os processos de E/S dos de CPU: respostas 3,8 a 5,5×
   menores para o `eco` com o mesmo quantum, quase sem custo para os
   CPU-bound, e menor ociosidade quando a E/S é o gargalo. O preço aparece
   quando um processo que "usou muito" o quantum precisa voltar logo (o init
   no passo [6] dos testes): não há envelhecimento, então ele espera os
   outros piorarem.
4. **O custo do próprio SO importa** quando ele roda na CPU que mede: 26–64%
   do tempo no núcleo, e o intervalo do relógio tem que ser maior que o
   caminho mais longo do núcleo, senão o relógio perde interrupções.
