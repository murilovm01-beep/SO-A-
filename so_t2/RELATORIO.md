# Relatório 
## 1. Carga mista, quantum 5

| escalonador | tempo total | preempções | ret cpu(40) | ret cpu(20) | eco: vezes bloqueado | eco: pronto | eco: resp |
|---|---:|---:|---:|---:|---:|---:|---:|
| simples | 115 | 0 | 58 | 90 | 0 | 89 | - |
| round-robin | 119 | 19 | 111 | 76 | 4 | 38 | 6,7 |
| prioridade | 120 | 20 | 112 | 81 | 4 | 19 | 3,2 |

## 2. Mudando o quantum (carga mista)

| escalonador | quantum | tempo total | preempções | eco: pronto | eco: resp |
|---|---:|---:|---:|---:|---:|
| round-robin | 2 | 124 | 61 | 38 | 3,0 |
| round-robin | 5 | 119 | 19 | 38 | 6,7 |
| round-robin | 10 | 117 | 9 | 55 | 11,6 |
| prioridade | 2 | 126 | 60 | 1 | 0,2 |
| prioridade | 5 | 120 | 20 | 19 | 3,2 |
| prioridade | 10 | 118 | 9 | 33 | 4,3 |

## 3. Só CPU, quantum 5

| escalonador | cpu(40) | cpu(30) | cpu(20) | cpu(10) | ret médio | tempo total |
|---|---:|---:|---:|---:|---:|---:|
| simples | 58 | 102 | 134 | 155 | 112,3 | 164 |
| round-robin | 161 | 145 | 126 | 97 | 132,3 | 169 |
| prioridade | 163 | 146 | 121 | 92 | 130,5 | 171 |

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
