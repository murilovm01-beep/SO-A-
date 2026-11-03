# Resumo dos experimentos (gerado por ferramentas/resume.py)

Tempos em instruções. "núcleo" e "ocioso" em porcentagem do tempo total.

### A. Carga mista, os três escalonadores (quantum 5 x 4000)

| execução | esc. | quantum | intervalo | tempo total | núcleo | ocioso | preempções | trocas de contexto | int. relógio | chamadas |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `A_misto_simples` | simples | 5 | 4000 | 658.497 | 37.0% | 0.0% | 0 | 9 | 164 | 406 |
| `A_misto_rr` | RR | 5 | 4000 | 698.861 | 40.3% | 0.0% | 30 | 41 | 174 | 406 |
| `A_misto_prio` | prioridade | 5 | 4000 | 706.635 | 40.9% | 0.0% | 30 | 40 | 176 | 406 |

### A. Tempo de retorno por processo

| execução | cpu(40) | eco(4) | cpu(20) | escritor(12) | média |
|---|---:|---:|---:|---:|---:|
| `A_misto_simples` | 216.375 | 408.841 | 385.334 | 627.495 | 409.511 |
| `A_misto_rr` | 643.784 | 407.325 | 457.278 | 667.779 | 544.041 |
| `A_misto_prio` | 649.635 | 367.836 | 456.184 | 675.642 | 537.324 |

### A. O processo limitado por E/S (eco)

| execução | retorno do eco | resposta média do eco | vezes bloqueado | tempo bloqueado | tempo pronto | tempo executando |
|---|---:|---:|---:|---:|---:|---:|
| `A_misto_simples` | 408.841 | 50.645 | 1 | 72.286 | 268.835 | 67.720 |
| `A_misto_rr` | 407.325 | 49.862 | 4 | 115.792 | 223.512 | 68.021 |
| `A_misto_prio` | 367.836 | 9.911 | 4 | 235.319 | 63.701 | 68.816 |

#### `A_misto_simples` -- simples, quantum 5 x 4000

| pid | programa | retorno | preempções | vezes P/B/E | tempo pronto | tempo bloqueado | tempo executando | resposta média |
|---:|---|---:|---:|---|---:|---:|---:|---:|
| 1 | init(2) | 658.497 | 0 | 4/3/4 | 0 | 616.827 | 41.670 | 0 |
| 2 | cpu(40) | 216.375 | 0 | 1/0/1 | 8.524 | 0 | 207.851 | - |
| 3 | eco(4) | 408.841 | 0 | 2/1/2 | 268.835 | 72.286 | 67.720 | 50.645 |
| 4 | cpu(20) | 385.334 | 0 | 1/0/1 | 262.403 | 0 | 122.931 | - |
| 5 | escritor(12) | 627.495 | 0 | 1/0/1 | 409.170 | 0 | 218.325 | - |

#### `A_misto_rr` -- RR, quantum 5 x 4000

| pid | programa | retorno | preempções | vezes P/B/E | tempo pronto | tempo bloqueado | tempo executando | resposta média |
|---:|---|---:|---:|---|---:|---:|---:|---:|
| 1 | init(2) | 698.861 | 1 | 4/2/4 | 76.344 | 583.277 | 39.240 | 9.102 |
| 2 | cpu(40) | 643.784 | 11 | 12/0/12 | 424.258 | 0 | 219.526 | - |
| 3 | eco(4) | 407.325 | 0 | 5/4/5 | 223.512 | 115.792 | 68.021 | 49.862 |
| 4 | cpu(20) | 457.278 | 6 | 7/0/7 | 325.820 | 0 | 131.458 | - |
| 5 | escritor(12) | 667.779 | 12 | 13/0/13 | 427.163 | 0 | 240.616 | - |

#### `A_misto_prio` -- prioridade, quantum 5 x 4000

| pid | programa | retorno | preempções | vezes P/B/E | tempo pronto | tempo bloqueado | tempo executando | resposta média |
|---:|---|---:|---:|---|---:|---:|---:|---:|
| 1 | init(2) | 706.635 | 1 | 4/2/4 | 69.669 | 595.992 | 40.974 | 0 |
| 2 | cpu(40) | 649.635 | 11 | 12/0/12 | 426.518 | 0 | 223.117 | - |
| 3 | eco(4) | 367.836 | 0 | 5/4/5 | 63.701 | 235.319 | 68.816 | 9.911 |
| 4 | cpu(20) | 456.184 | 6 | 7/0/7 | 323.401 | 0 | 132.783 | - |
| 5 | escritor(12) | 675.642 | 12 | 13/0/13 | 434.697 | 0 | 240.945 | - |

### B. Variando o quantum (carga mista, intervalo 4000)

| execução | esc. | quantum | intervalo | tempo total | núcleo | ocioso | preempções | trocas de contexto | int. relógio | chamadas |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `B_misto_rr_q1` | RR | 1 | 4000 | 886.844 | 51.9% | 0.0% | 221 | 217 | 221 | 406 |
| `B_misto_rr_q2` | RR | 2 | 4000 | 763.957 | 45.0% | 0.0% | 93 | 100 | 190 | 406 |
| `A_misto_rr` | RR | 5 | 4000 | 698.861 | 40.3% | 0.0% | 30 | 41 | 174 | 406 |
| `B_misto_rr_q10` | RR | 10 | 4000 | 678.075 | 38.6% | 0.0% | 14 | 23 | 169 | 406 |
| `B_misto_rr_q20` | RR | 20 | 4000 | 663.157 | 37.4% | 0.0% | 5 | 12 | 165 | 406 |
| `B_misto_prio_q1` | prioridade | 1 | 4000 | 945.677 | 54.6% | 0.0% | 236 | 211 | 236 | 406 |
| `B_misto_prio_q2` | prioridade | 2 | 4000 | 781.669 | 46.1% | 0.0% | 94 | 94 | 195 | 406 |
| `A_misto_prio` | prioridade | 5 | 4000 | 706.635 | 40.9% | 0.0% | 30 | 40 | 176 | 406 |
| `B_misto_prio_q10` | prioridade | 10 | 4000 | 683.986 | 39.1% | 0.0% | 14 | 23 | 170 | 406 |
| `B_misto_prio_q20` | prioridade | 20 | 4000 | 674.012 | 38.3% | 0.0% | 5 | 15 | 168 | 406 |

### B. Tempo de retorno por processo

| execução | cpu(40) | eco(4) | cpu(20) | escritor(12) | média |
|---|---:|---:|---:|---:|---:|
| `B_misto_rr_q1` | 851.518 | 442.092 | 564.072 | 808.009 | 666.422 |
| `B_misto_rr_q2` | 712.534 | 415.973 | 515.012 | 722.711 | 591.557 |
| `A_misto_rr` | 643.784 | 407.325 | 457.278 | 667.779 | 544.041 |
| `B_misto_rr_q10` | 606.488 | 387.462 | 469.008 | 646.460 | 527.354 |
| `B_misto_rr_q20` | 567.338 | 387.319 | 433.074 | 629.225 | 504.239 |
| `B_misto_prio_q1` | 908.445 | 363.001 | 620.176 | 859.667 | 687.822 |
| `B_misto_prio_q2` | 750.274 | 348.680 | 499.599 | 724.236 | 580.697 |
| `A_misto_prio` | 649.635 | 367.836 | 456.184 | 675.642 | 537.324 |
| `B_misto_prio_q10` | 614.735 | 387.701 | 471.707 | 652.962 | 531.776 |
| `B_misto_prio_q20` | 575.132 | 387.509 | 434.414 | 642.328 | 509.845 |

### B. O eco com quanta diferentes

| execução | retorno do eco | resposta média do eco | vezes bloqueado | tempo bloqueado | tempo pronto | tempo executando |
|---|---:|---:|---:|---:|---:|---:|
| `B_misto_rr_q1` | 442.092 | 9.618 | 4 | 70.622 | 282.725 | 88.745 |
| `B_misto_rr_q2` | 415.973 | 19.689 | 4 | 81.324 | 259.176 | 75.473 |
| `A_misto_rr` | 407.325 | 49.862 | 4 | 115.792 | 223.512 | 68.021 |
| `B_misto_rr_q10` | 387.462 | 99.182 | 2 | 79.122 | 242.414 | 65.926 |
| `B_misto_rr_q20` | 387.319 | 201.334 | 1 | 36.564 | 285.383 | 65.372 |
| `B_misto_prio_q1` | 363.001 | 2.032 | 4 | 234.867 | 36.710 | 91.424 |
| `B_misto_prio_q2` | 348.680 | 5.176 | 4 | 227.556 | 43.179 | 77.945 |
| `A_misto_prio` | 367.836 | 9.911 | 4 | 235.319 | 63.701 | 68.816 |
| `B_misto_prio_q10` | 387.701 | 23.327 | 3 | 206.272 | 114.021 | 67.408 |
| `B_misto_prio_q20` | 387.509 | 36.339 | 3 | 126.870 | 193.058 | 67.581 |

### C. Só processos CPU-bound

| execução | esc. | quantum | intervalo | tempo total | núcleo | ocioso | preempções | trocas de contexto | int. relógio | chamadas |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `C_cpu_simples` | simples | 5 | 4000 | 613.285 | 25.7% | 0.0% | 0 | 9 | 153 | 215 |
| `C_cpu_rr` | RR | 5 | 4000 | 641.946 | 28.8% | 0.0% | 30 | 34 | 160 | 215 |
| `C_cpu_prio` | prioridade | 5 | 4000 | 649.565 | 29.6% | 0.0% | 30 | 34 | 162 | 215 |
| `C_cpu_rr_q1` | RR | 1 | 4000 | 815.079 | 42.9% | 0.0% | 203 | 182 | 203 | 215 |
| `C_cpu_rr_q20` | RR | 20 | 4000 | 616.880 | 26.1% | 0.0% | 5 | 11 | 154 | 215 |

### C. Tempo de retorno por processo

| execução | cpu(40) | cpu(30) | cpu(20) | cpu(10) | média |
|---|---:|---:|---:|---:|---:|
| `C_cpu_simples` | 214.804 | 379.919 | 502.611 | 582.350 | 419.921 |
| `C_cpu_rr` | 613.054 | 559.901 | 488.539 | 381.570 | 510.766 |
| `C_cpu_prio` | 620.763 | 566.507 | 490.467 | 382.461 | 515.049 |
| `C_cpu_rr_q1` | 779.711 | 703.275 | 592.263 | 415.250 | 622.624 |
| `C_cpu_rr_q20` | 580.848 | 587.066 | 520.131 | 318.152 | 501.549 |

### D. Mesma fatia de tempo (20000 instruções), relógio com intervalos diferentes (RR)

| execução | esc. | quantum | intervalo | tempo total | núcleo | ocioso | preempções | trocas de contexto | int. relógio | chamadas |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `D_misto_rr_L2000_q10` | RR | 10 | 2000 | 823.257 | 43.0% | 0.0% | 36 | 46 | 411 | 406 |
| `A_misto_rr` | RR | 5 | 4000 | 698.861 | 40.3% | 0.0% | 30 | 41 | 174 | 406 |
| `D_misto_rr_L10000_q2` | RR | 2 | 10000 | 637.236 | 37.8% | 0.0% | 28 | 39 | 63 | 406 |
| `D_misto_rr_L20000_q1` | RR | 1 | 20000 | 627.131 | 37.7% | 0.0% | 31 | 40 | 31 | 406 |

### D. Tempo de retorno por processo

| execução | cpu(40) | eco(4) | cpu(20) | escritor(12) | média |
|---|---:|---:|---:|---:|---:|
| `D_misto_rr_L2000_q10` | 750.014 | 463.843 | 533.659 | 767.558 | 628.768 |
| `A_misto_rr` | 643.784 | 407.325 | 457.278 | 667.779 | 544.041 |
| `D_misto_rr_L10000_q2` | 585.996 | 413.059 | 432.848 | 608.967 | 510.217 |
| `D_misto_rr_L20000_q1` | 562.972 | 403.273 | 454.568 | 598.448 | 504.815 |

### E. Console lenta (2000 instruções por caractere escrito)

| execução | esc. | quantum | intervalo | tempo total | núcleo | ocioso | preempções | trocas de contexto | int. relógio | chamadas |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `E_lenta_simples` | simples | 5 | 4000 | 1.406.043 | 61.7% | 11.2% | 0 | 161 | 351 | 406 |
| `E_lenta_rr` | RR | 5 | 4000 | 1.546.076 | 60.3% | 20.2% | 17 | 188 | 386 | 406 |
| `E_lenta_prio` | prioridade | 5 | 4000 | 1.433.077 | 63.8% | 8.1% | 16 | 251 | 358 | 406 |

### E. Tempo de retorno por processo

| execução | cpu(40) | eco(4) | cpu(20) | escritor(12) | média |
|---|---:|---:|---:|---:|---:|
| `E_lenta_simples` | 463.375 | 473.925 | 777.802 | 1.318.160 | 758.315 |
| `E_lenta_rr` | 860.516 | 602.046 | 807.877 | 1.442.457 | 928.224 |
| `E_lenta_prio` | 1.063.647 | 425.610 | 751.727 | 1.349.587 | 897.642 |

#### `E_lenta_simples` -- simples, quantum 5 x 4000

| pid | programa | retorno | preempções | vezes P/B/E | tempo pronto | tempo bloqueado | tempo executando | resposta média |
|---:|---|---:|---:|---|---:|---:|---:|---:|
| 1 | init(2) | 1.406.043 | 0 | 24/23/24 | 0 | 1.328.367 | 77.676 | 0 |
| 2 | cpu(40) | 463.375 | 0 | 42/41/42 | 49.956 | 111.922 | 301.497 | 1.009 |
| 3 | eco(4) | 473.925 | 0 | 37/36/37 | 193.108 | 135.387 | 145.430 | 5.091 |
| 4 | cpu(20) | 777.802 | 0 | 42/41/42 | 443.415 | 123.692 | 210.695 | 9.225 |
| 5 | escritor(12) | 1.318.160 | 0 | 156/155/156 | 571.154 | 234.239 | 512.767 | 605 |

#### `E_lenta_rr` -- RR, quantum 5 x 4000

| pid | programa | retorno | preempções | vezes P/B/E | tempo pronto | tempo bloqueado | tempo executando | resposta média |
|---:|---|---:|---:|---|---:|---:|---:|---:|
| 1 | init(2) | 1.546.076 | 0 | 25/24/25 | 0 | 1.474.895 | 71.181 | 0 |
| 2 | cpu(40) | 860.516 | 12 | 55/42/55 | 322.471 | 208.098 | 329.947 | 2.763 |
| 3 | eco(4) | 602.046 | 0 | 27/26/27 | 432.100 | 60.833 | 109.113 | 16.243 |
| 4 | cpu(20) | 807.877 | 5 | 48/42/48 | 396.310 | 187.686 | 223.881 | 6.038 |
| 5 | escritor(12) | 1.442.457 | 0 | 188/187/188 | 466.398 | 475.646 | 500.413 | 2.395 |

#### `E_lenta_prio` -- prioridade, quantum 5 x 4000

| pid | programa | retorno | preempções | vezes P/B/E | tempo pronto | tempo bloqueado | tempo executando | resposta média |
|---:|---|---:|---:|---|---:|---:|---:|---:|
| 1 | init(2) | 1.433.077 | 0 | 21/20/21 | 4.897 | 1.358.474 | 69.706 | 244 |
| 2 | cpu(40) | 1.063.647 | 11 | 54/42/54 | 613.833 | 123.619 | 326.195 | 510 |
| 3 | eco(4) | 425.610 | 0 | 37/36/37 | 171.112 | 103.977 | 150.521 | 4.464 |
| 4 | cpu(20) | 751.727 | 5 | 48/42/48 | 397.976 | 123.095 | 230.656 | 519 |
| 5 | escritor(12) | 1.349.587 | 0 | 169/168/169 | 423.738 | 385.264 | 540.585 | 1.797 |

### Testes funcionais (cenário 1)

| execução | esc. | quantum | intervalo | tempo total | núcleo | ocioso | preempções | trocas de contexto | int. relógio | chamadas |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `testes_simples` | simples | 5 | 4000 | 1.529.724 | 62.7% | 3.2% | 0 | 24 | 382 | 1785 |
| `testes_rr` | RR | 5 | 4000 | 2.391.236 | 47.4% | 2.1% | 108 | 85 | 597 | 1782 |
| `testes_prio` | prioridade | 5 | 4000 | 3.585.865 | 36.8% | 1.4% | 169 | 86 | 896 | 1784 |

### Conferência do relógio do SO

Para cada execução: soma dos tempos "executando" de todos os processos + tempo ocioso (deve dar o tempo total), e diferença entre a instrução real em que o SO começou o relatório final (contada pelo executor) e o tempo total medido pelo SO (deve ser só o boot antes de o SO zerar o relógio mais o resto da última chamada -- quase constante).

| execução | tempo total (SO) | Σ executando + ocioso | instrução real do fim | diferença | maior entrada no núcleo |
|---|---:|---:|---:|---:|---:|
| `A_misto_prio` | 706.635 | 706.635 | 709.247 | 2.612 | 2.706 |
| `A_misto_rr` | 698.861 | 698.861 | 701.473 | 2.612 | 2.680 |
| `A_misto_simples` | 658.497 | 658.497 | 661.109 | 2.612 | 2.736 |
| `B_misto_prio_q1` | 945.677 | 945.677 | 948.289 | 2.612 | 2.536 |
| `B_misto_prio_q10` | 683.986 | 683.986 | 686.598 | 2.612 | 2.681 |
| `B_misto_prio_q2` | 781.669 | 781.669 | 784.320 | 2.651 | 2.561 |
| `B_misto_prio_q20` | 674.012 | 674.012 | 676.624 | 2.612 | 2.745 |
| `B_misto_rr_q1` | 886.844 | 886.844 | 889.456 | 2.612 | 2.507 |
| `B_misto_rr_q10` | 678.075 | 678.075 | 680.687 | 2.612 | 2.641 |
| `B_misto_rr_q2` | 763.957 | 763.957 | 766.608 | 2.651 | 2.730 |
| `B_misto_rr_q20` | 663.157 | 663.157 | 665.794 | 2.637 | 2.492 |
| `C_cpu_prio` | 649.565 | 649.565 | 652.177 | 2.612 | 2.536 |
| `C_cpu_rr` | 641.946 | 641.946 | 644.583 | 2.637 | 2.521 |
| `C_cpu_rr_q1` | 815.079 | 815.079 | 817.691 | 2.612 | 2.507 |
| `C_cpu_rr_q20` | 616.880 | 616.880 | 619.492 | 2.612 | 2.467 |
| `C_cpu_simples` | 613.285 | 613.285 | 615.897 | 2.612 | 2.736 |
| `D_misto_rr_L10000_q2` | 637.236 | 637.236 | 639.912 | 2.676 | 2.641 |
| `D_misto_rr_L20000_q1` | 627.131 | 627.131 | 629.782 | 2.651 | 2.666 |
| `D_misto_rr_L2000_q10` | 823.257 | 823.257 | 829.933 | 6.676 | 1.553 |
| `E_lenta_prio` | 1.433.077 | 1.433.077 | 1.435.689 | 2.612 | 3.418 |
| `E_lenta_rr` | 1.546.076 | 1.546.076 | 1.548.727 | 2.651 | 3.498 |
| `E_lenta_simples` | 1.406.043 | 1.406.043 | 1.408.655 | 2.612 | 3.725 |
| `testes_prio` | 3.585.865 | 3.585.865 | 3.588.727 | 2.862 | 3.230 |
| `testes_rr` | 2.391.236 | 2.391.236 | 2.394.059 | 2.823 | 3.178 |
| `testes_simples` | 1.529.724 | 1.529.724 | 1.532.561 | 2.837 | 3.182 |

