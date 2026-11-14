# T2 — Processos

SO que roda em cima da BIOS do Mancha (`bios_mancha`), com processos em modo
usuário, as 5 chamadas de sistema, bloqueio por E/S e por espera de outro
processo, três escalonadores e as métricas. Feito em C (mcc) com uma parte em
assembly, seguindo o exemplo `bios_mancha/exemplos/multitarefa_c`.

## Como rodar

Na raiz do projeto, `make` (compila o simulador, o montador, o mcc e a bios).
Depois, aqui na pasta:

```
make
make rodar        (é o mesmo que ../simulador_completo/bin/simulador so.mob)
```

No simulador: `D0` e `C`. Quando o `eco` pedir uma linha, digite com
`E<texto>`. Quando o init termina, o SO mostra as métricas; como a janela da
console só mostra 6 linhas, elas saem de 5 em 5 e é só mandar `E` pra ver a
próxima parte.

O escalonador, o quantum e o cenário ficam no `config.h` (muda e roda `make`
de novo). O intervalo do relógio fica no `ponte.asm` (4000 instruções).

## Arquivos

- `so.c` — o SO: tabela de processos, chamadas de sistema, pendências, escalonadores, métricas
- `ponte.asm` — o que não dá pra fazer em C: instala os tratadores, programa o relógio, primeiro `rete`, laço ocioso e funções pro C chamar a BIOS
- `usuario.asm` — as chamadas de sistema do lado do processo (`trap 7`)
- `biblioteca.c` — `putchar`, `puts`, `print_int`... do `compilador_c`, trocando as portas pelas chamadas
- `programas.c` — o init e os programas de teste
- `RELATORIO.md` — as execuções

## Como funciona

- **Interrupções**: as chamadas usam o `trap 7` e a `tabela_syscalls` da BIOS
  e o relógio usa o `retomada_escalonador`. No vetor eu troquei só as exceções
  1 a 4 (o tratador da BIOS para a máquina; o do SO mata só o processo) e a
  console (só pra contar a interrupção; depois quem trata é a BIOS mesmo).
- Tudo cai em `_so_entrada` (ponte.asm), que troca pra `pilha_nucleo` e chama
  `so_trata_interrupcao()`: salva o quadro do processo na tabela, trata a
  interrupção, vê as pendências, escalona e copia o quadro do escolhido de
  volta pra `pilha_sistema-32`, de onde o `rete` lê. O quadro tem que ser
  copiado porque toda interrupção empilha no mesmo lugar.
- **Processos** rodam com S=0 e I=0, então `in`/`out`/`halt` dão interrupção
  2 e o SO mata o processo. O pid é um contador (não é o índice da tabela).
  Quando cria um processo, o SO deixa na pilha o endereço de `proc_retorno`,
  que chama `SO_MATA_PROC`: quando a função do programa acaba, o processo
  morre sozinho. Sem processo pra rodar, o SO fica num laço ocioso esperando
  o relógio.
- **E/S**: `SO_LE` lê do buffer de teclado da BIOS e bloqueia se não tem
  tecla. `SO_ESCREVE` bloqueia se a console não está pronta (no simulador
  ela sempre está). As pendências são vistas a cada interrupção que passa
  pelo SO. `SO_ESPERA_PROC` bloqueia até o outro processo morrer.
- **Escalonadores**: simples (primeiro pronto da tabela, sem preempção),
  round-robin (fila de prontos + quantum) e prioridade
  (`prio = (prio + t_exec/t_quantum) / 2`, guardada em milésimos porque o mcc
  não tem float; escolhe a menor).
- **Tempo**: tudo em tiques do relógio (o `contador_tiques` da BIOS).
  1 tique = 4000 instruções.

## Chamadas

| nº | chamada | argumentos | retorno |
|---|---|---|---|
| 1 | `SO_LE` | — | caractere |
| 2 | `SO_ESCREVE` | r1 = caractere | 0 |
| 7 | `SO_CRIA_PROC` | r1 = nome do programa, r2 = argumento | pid ou -1 |
| 8 | `SO_MATA_PROC` | r1 = pid (0 = ele mesmo) | 0 ou -1 |
| 9 | `SO_ESPERA_PROC` | r1 = pid | 0, ou -1 se for ele mesmo ou o pid não existe |

## Observações

- `SO_ESPERA_PROC` de um processo que já morreu devolve 0 sem bloquear (antes
  eu devolvia -1, mas aí dependia de o filho terminar antes ou depois do pai
  chamar a espera).
- Não tem proteção de memória: todos os processos usam a mesma segmentação.
- O intervalo do relógio não pode ser pequeno demais, porque o SO roda na
  mesma CPU dos processos: uma chamada simples gasta umas 800 instruções e
  criar ou matar processo umas 2000. Se uma interrupção demorar mais que o
  intervalo, o relógio perde tiques.
