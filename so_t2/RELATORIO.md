# Relatório — T2

Rodei o SO no simulador do professor (`make rodar`, depois `D0` e `C`) e
anotei as métricas que o SO mostra no fim. Os tempos estão em tiques do
relógio, como o SO imprime (1 tique = 4000 instruções).

## Como rodei

- Para cada configuração: mudei o `config.h`, rodei `make` e o simulador.
- **Carga mista** (`CEN_MISTO`): `cpu(40)` e `cpu(20)` só fazem conta, e o
  `eco(4)` lê 4 linhas da console e repete. Pra ficar igual em todas as
  execuções, eu pausava (`P`), digitava a linha (`E<linha>`) e continuava
  (`C`) quando o `instr=` da tela passava de ~100, 200, 300 e 400 mil.
- **Só CPU** (`CEN_CPU`): `cpu(40)`, `cpu(30)`, `cpu(20)` e `cpu(10)`, criados
  nessa ordem.
- Os testes (`CEN_TESTES`) passaram nos três escalonadores: erros das
  chamadas, processo tentando `outb`, `halt` e dividir por zero (o SO mata só
  ele), processo que termina sozinho, matar outro processo, processo que cria
  processos, tabela cheia e leitura bloqueante.

Nas tabelas: **ret** = tempo de retorno (fim − criação), **resp** = tempo
médio de resposta (de desbloquear até ser escalonado), **pronto** = tempo
total no estado pronto.

## 1. Carga mista, quantum 5

| escalonador | tempo total | preempções | ret cpu(40) | ret cpu(20) | eco: vezes bloqueado | eco: pronto | eco: resp |
|---|---:|---:|---:|---:|---:|---:|---:|
| simples | 115 | 0 | 58 | 90 | 0 | 89 | - |
| round-robin | 119 | 19 | 111 | 76 | 4 | 38 | 6,7 |
| prioridade | 120 | 20 | 112 | 81 | 4 | 19 | 3,2 |

- **Simples**: o `cpu(40)` pega a CPU e só larga quando termina (58 tiques),
  depois vem o `cpu(20)`. O `eco` fica 89 tiques pronto sem rodar; quando
  finalmente roda, as linhas já estão no buffer da BIOS e ele nem chega a
  bloquear. É o que termina tudo mais cedo (menos troca de processo), mas pra
  quem está digitando é o pior.
- **Round-robin**: cada um roda 5 tiques por vez. O `cpu(40)` demora quase o
  dobro pra terminar (111), mas o `eco` responde em ~7 tiques: quando
  desbloqueia ele vai pro fim da fila e espera o resto do quantum de quem
  está rodando e mais o quantum do outro.
- **Prioridade**: o `eco` bloqueia logo depois de rodar, então o t_exec dele
  fica perto de 0 e a prioridade dele cai (menor = melhor), enquanto os `cpu`
  gastam o quantum todo e vão pra perto de 1. Quando desbloqueia, ele é o
  escolhido assim que o quantum de quem está rodando acaba: resposta de 3,2
  tiques (metade da do round-robin) e metade do tempo na fila de prontos (19
  contra 38). Os `cpu` quase não mudam.
- O tempo total sobe um pouco com preempção, porque trocar de processo também
  gasta instrução.

## 2. Mudando o quantum (carga mista)

| escalonador | quantum | tempo total | preempções | eco: pronto | eco: resp |
|---|---:|---:|---:|---:|---:|
| round-robin | 2 | 124 | 61 | 38 | 3,0 |
| round-robin | 5 | 119 | 19 | 38 | 6,7 |
| round-robin | 10 | 117 | 9 | 55 | 11,6 |
| prioridade | 2 | 126 | 60 | 1 | 0,2 |
| prioridade | 5 | 120 | 20 | 19 | 3,2 |
| prioridade | 10 | 118 | 9 | 33 | 4,3 |

- Quantum menor dá mais preempções e o sistema demora mais no total (o SO
  roda mais vezes), mas o `eco` responde mais rápido. No round-robin a
  resposta cresce junto com o quantum (3,0 → 6,7 → 11,6).
- Com prioridade o `eco` responde mais rápido que no round-robin com o mesmo
  quantum, nos três casos. Com quantum 2 ele praticamente não espera.
- Com quantum 10 o round-robin vai ficando parecido com o simples: poucas
  preempções e o `eco` esperando mais.

## 3. Só CPU, quantum 5

| escalonador | cpu(40) | cpu(30) | cpu(20) | cpu(10) | ret médio | tempo total |
|---|---:|---:|---:|---:|---:|---:|
| simples | 58 | 102 | 134 | 155 | 112,3 | 164 |
| round-robin | 161 | 145 | 126 | 97 | 132,3 | 169 |
| prioridade | 163 | 146 | 121 | 92 | 130,5 | 171 |

- No simples eles rodam na ordem em que foram criados, o maior primeiro, e
  mesmo assim a média é melhor que a do round-robin. Fazendo a conta sem o
  custo do SO (40, 30, 20 e 10 unidades de trabalho):
  - em ordem: terminam em 40, 70, 90 e 100 → média 75
  - round-robin: o de 10 termina em 4×10 = 40, o de 20 em 40+3×10 = 70, o de
    30 em 70+2×10 = 90 e o de 40 em 100 → média 75 também

  Ou seja, na teoria dá igual, e a diferença que apareceu é o custo das
  preempções. O que o round-robin muda é quem espera: o `cpu(10)` termina bem
  antes (97 em vez de 155).
- Prioridade fica quase igual ao round-robin: todos gastam o quantum inteiro,
  a prioridade de todos vai pra perto de 1 e aí vale a ordem da fila.

## Conclusões

- O simples termina tudo mais rápido, mas um processo interativo fica
  esperando os outros acabarem.
- No round-robin o quantum é um meio-termo: menor responde melhor, mas o SO
  trabalha mais.
- O de prioridade dá preferência sozinho pra quem faz E/S (o `eco`), quase
  sem atrapalhar os outros.
- O SO roda na mesma CPU que os processos, então ele também conta no tempo:
  cada chamada de sistema gasta umas 800 instruções. Por isso o `eco` imprime
  pouco; na primeira versão ele imprimia mais e gastava quase um quantum
  inteiro por linha, e quase não bloqueava.
