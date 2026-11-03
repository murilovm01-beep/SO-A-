#!/bin/sh
# experimentos.sh -- roda o SO do T2 em várias configurações (escalonador,
# quantum, intervalo do relógio, carga, console lenta) no executor em lote e
# guarda a saída completa de cada execução em resultados/<nome>.txt. Depois
# gera resultados/resumo.md com as tabelas usadas no RELATORIO.md.
#
# uso (de dentro de so_t2/):  sh ferramentas/experimentos.sh
#
# Tudo é determinístico: o simulador não tem nenhuma fonte de aleatoriedade e
# a entrada da console é injetada em instantes fixos (em instruções), então
# rodar de novo dá exatamente os mesmos números.

set -e
cd "$(dirname "$0")/.."
mkdir -p resultados

# carga "misto": o processo eco espera 4 linhas, que o "usuário" digita em
# ~50, 150, 250 e 350 mil instruções. Os instantes exatos são "quebrados" de
# propósito: com 50000, 150000... (múltiplos de 2000 e 4000 a menos do boot)
# toda linha chegava logo depois de um tique do relógio, com o núcleo ainda
# tratando a interrupção -- e o SO a via nas pendências DESSA interrupção,
# o que favorecia artificialmente o eco quando o tique também era uma
# preempção. Estes instantes caem em fases diferentes do relógio.
MISTO="-e 51111:um -e 152222:dois -e 253333:tres -e 354444:quatro"

# roda NOME ESC QUANTUM INTERVALO CENARIO VERBOSO [opções do executor...]
roda() {
    nome=$1; esc=$2; q=$3; l=$4; cen=$5; verb=$6
    shift 6
    make -s so.mob ESC=$esc QUANTUM=$q INTERVALO=$l CENARIO=$cen VERBOSO=$verb > /dev/null
    if ./bin/executor so.mob -n 50000000 "$@" > resultados/$nome.txt 2>&1; then
        echo "  $nome"
    else
        echo "  $nome  (NAO TERMINOU -- ver resultados/$nome.txt)"
    fi
}

echo "testes funcionais (cenario 1):"
roda testes_simples 1 5 4000 1 1 -o "20000:primeira linha digitada" -o "20000:segunda linha digitada"
roda testes_rr      2 5 4000 1 1 -o "20000:primeira linha digitada" -o "20000:segunda linha digitada"
roda testes_prio    3 5 4000 1 1 -o "20000:primeira linha digitada" -o "20000:segunda linha digitada"

echo "A: os tres escalonadores, carga mista, quantum 5 x 4000:"
roda A_misto_simples 1 5 4000 2 0 $MISTO
roda A_misto_rr      2 5 4000 2 0 $MISTO
roda A_misto_prio    3 5 4000 2 0 $MISTO

echo "B: variando o quantum (carga mista, intervalo 4000):"
for q in 1 2 10 20; do
    roda B_misto_rr_q$q   2 $q 4000 2 0 $MISTO
    roda B_misto_prio_q$q 3 $q 4000 2 0 $MISTO
done

echo "C: so processos CPU-bound (cpu 40, 30, 20, 10):"
roda C_cpu_simples 1 5 4000 3 0
roda C_cpu_rr      2 5 4000 3 0
roda C_cpu_prio    3 5 4000 3 0
roda C_cpu_rr_q1   2 1 4000 3 0
roda C_cpu_rr_q20  2 20 4000 3 0

echo "D: mesma fatia (20000 instrucoes), relogio mais ou menos frequente:"
roda D_misto_rr_L2000_q10  2 10 2000 2 0 $MISTO
roda D_misto_rr_L10000_q2  2 2 10000 2 0 $MISTO
roda D_misto_rr_L20000_q1  2 1 20000 2 0 $MISTO

echo "E: console lenta (2000 instrucoes por caractere), carga mista:"
roda E_lenta_simples 1 5 4000 2 0 -l 2000 $MISTO
roda E_lenta_rr      2 5 4000 2 0 -l 2000 $MISTO
roda E_lenta_prio    3 5 4000 2 0 -l 2000 $MISTO

# deixa so.mob na configuração padrão de novo
make -s so.mob > /dev/null

python3 ferramentas/resume.py resultados > resultados/resumo.md
echo "resumo: resultados/resumo.md"
