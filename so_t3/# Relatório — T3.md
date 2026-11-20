# Relatório — T3

## 1. Testes (partes I e II)

Cenário `CEN_TESTES`, 24 quadros:

| processo | o que ele faz | o que aconteceu |
|---|---|---|
| proibido 0 | escreve no endereço 0 (o próprio código) | morto: escrita em página só de leitura (interrupção 1) |
| proibido 1 | lê o endereço 0x9000, depois da área de dados | morto: acesso a dado fora da área (interrupção 5) |
| proibido 2 | chama o endereço 0x3000, página de código que o programa não tem | morto: código fora do programa (interrupção 5) |
| proibido 3 | recursão sem fim, a pilha passa de 0x8000 | morto: acesso a dado fora da área (interrupção 5) |
| conta 0, 1 e 2 | o mesmo binário em 3 processos; cada um grava o seu número em 0x8000 e imprime a letra do que lê de lá | `a0 b0 c0 a1 a2 b1 b2 c1 c2 ...`: cada um lê o seu |

## 2. Substituição de páginas (parte III)

O programa `fases` tem 6 contas diferentes (as fases), espalhadas pelas
páginas 1 a 5. Todas chamam `mistura()`, que fica na página 0 junto com o
`main`, então a página 0 é usada o tempo todo. Ele usa 6 páginas de código e
1 de dados. O init roda o mesmo trabalho (12 rodadas das 6 fases) de dois
jeitos: 1 processo (`CEN_FASES1`) ou 3 processos de 4 rodadas ao mesmo tempo
(`CEN_FASES3`). Para cada um mudei `QUADROS` e `SUBSTITUICAO`.

Nas tabelas, **faltas** é o total de faltas atendidas (contando as primeiras,
de quando a página nunca tinha sido carregada) e **tempo** é o tempo total.

### 1 processo

Sem substituição precisaria de 11 quadros (init 3 + 1, fases 6 + 1).

| quadros | FIFO: faltas | FIFO: tempo | LRU: faltas | LRU: tempo |
|---:|---:|---:|---:|---:|
| 12 | 9 | 168 | 9 | 175 |
| 7 | 79 | 176 | 57 | 179 |
| 6 | 82 | 176 | 67 | 179 |
| 5 | 87 | 176 | 99 | 181 |
| 4 | 17297 | 1761 | 11643 | 1291 |
| 3 | não termina | | não termina | |

### 3 processos

Sem substituição precisaria de 25 quadros (init 4, cada fases 7).

| quadros | FIFO: faltas | FIFO: tempo | LRU: faltas | LRU: tempo |
|---:|---:|---:|---:|---:|
| 24 | 22 | 171 | 22 | 184 |
| 16 | 84 | 181 | 65 | 188 |
| 12 | 153 | 189 | 144 | 195 |
| 8 | 154 | 186 | 157 | 191 |
| 7 | 167 | 186 | 184 | 193 |
| 6 | 17767 | 1915 | 12227 | 1440 |
| 5 | não termina | | não termina | |

### Limite mínimo

O mínimo é a área de dados de todos os processos (1 quadro cada, ela nunca
sai) mais 2 quadros para o código: 4 quadros com 1 processo (init e fases)
e 6 com 3 processos.


## 3. A primeira versão do LRU

Primeiro eu fiz o LRU escolhendo só pela idade, como está no enunciado.
Quando faltava quadro ficou bem pior que o FIFO:

| caso | FIFO | LRU só com a idade | LRU final |
|---|---:|---:|---:|
| 1 processo, 5 quadros | 87 faltas, 176 tiques | 472 faltas, 218 tiques | 99 faltas, 181 tiques |
| 3 processos, 8 quadros | 154 faltas, 186 tiques | 418 faltas, 220 tiques | 157 faltas, 191 tiques |

Imprimindo cada substituição deu pra ver o problema. A idade só muda no
tique (20000 instruções) e dentro de um tique o programa passa por várias
fases. Uma página que tinha sido muito usada até pouco tempo antes estava
com idade alta, e a que acabou de ser carregada, que era a que estava sendo
usada, tinha idade 128. O LRU tirava a nova, o programa precisava dela na
hora, e ficava indo e voltando entre duas páginas até o próximo tique.
