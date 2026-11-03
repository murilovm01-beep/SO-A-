#!/usr/bin/env python3
# resume.py -- lê as saídas de experimentos.sh (resultados/*.txt) e escreve,
# em Markdown, as tabelas usadas no RELATORIO.md.
#
# uso: python3 ferramentas/resume.py resultados > resultados/resumo.md
#
# Só reorganiza números que o próprio SO imprimiu no relatório final (e a
# contagem real de instruções impressa pelo executor); não calcula nenhuma
# métrica nova além de médias e porcentagens.

import os
import re
import sys

NOMES_ESC = {"simples": "simples", "round-robin": "RR", "prioridade": "prioridade"}


def num(s):
    return int(s.strip())


def le(caminho):
    texto = open(caminho, encoding="utf-8", errors="replace").read()
    r = {"arquivo": os.path.basename(caminho), "procs": {}}

    m = re.search(r"escalonador: (\S+)\s+quantum: (\d+) interrupcoes\s+intervalo do relogio: (\d+)", texto)
    if not m:
        return None
    r["esc"] = NOMES_ESC.get(m.group(1), m.group(1))
    r["quantum"] = int(m.group(2))
    r["intervalo"] = int(m.group(3))

    def campo(rotulo):
        mm = re.search(rotulo + r" \.+ (\d+)", texto)
        return int(mm.group(1)) if mm else None

    r["criados"] = campo("processos criados")
    r["total"] = campo("tempo total de execucao")
    r["ocioso"] = campo("tempo ocioso")
    r["nucleo"] = campo("tempo no nucleo")
    r["preempcoes"] = campo("preempcoes")
    r["trocas"] = campo("trocas de contexto")
    mm = re.search(r"tempo ocioso \.+ \d+\s+\((\d+) vezes\)", texto)
    r["n_ocioso"] = int(mm.group(1)) if mm else None
    mm = re.search(r"maior entrada: (\d+)", texto)
    r["maior_entrada"] = int(mm.group(1)) if mm else None

    r["irq"] = {}
    bloco = texto[texto.find("interrupcoes recebidas:"):texto.find("chamadas: le=")]
    for mm in re.finditer(r"^\s+(\d+) ([a-z /0-9]+?)\s+(\d+)$", bloco, re.M):
        r["irq"][int(mm.group(1))] = int(mm.group(3))

    mm = re.search(r"chamadas: le=(\d+) escreve=(\d+) cria=(\d+) mata=(\d+) espera=(\d+)", texto)
    if mm:
        r["chamadas"] = dict(zip(["le", "escreve", "cria", "mata", "espera"], map(int, mm.groups())))

    # tabela 1: pid programa arg retorno preemp nP nB nE resp
    for mm in re.finditer(r"^\s*(\d+) (\S+)\s+(-?\d+)\s+(\d+|\(vivo\))\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+|-)\s*$", texto, re.M):
        pid = int(mm.group(1))
        r["procs"][pid] = {
            "prog": mm.group(2), "arg": int(mm.group(3)),
            "retorno": None if mm.group(4) == "(vivo)" else int(mm.group(4)),
            "preemp": int(mm.group(5)), "nP": int(mm.group(6)), "nB": int(mm.group(7)),
            "nE": int(mm.group(8)), "resp": None if mm.group(9) == "-" else int(mm.group(9)),
        }
    # tabela 2: pid programa tP tB tE criacao termino
    for mm in re.finditer(r"^\s*(\d+) (\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+|-)\s*$", texto, re.M):
        pid = int(mm.group(1))
        if pid in r["procs"]:
            p = r["procs"][pid]
            p["tP"], p["tB"], p["tE"] = int(mm.group(3)), int(mm.group(4)), int(mm.group(5))
            p["criacao"] = int(mm.group(6))
            p["termino"] = None if mm.group(7) == "-" else int(mm.group(7))

    mm = re.search(r"\[executor\] (.*?) apos (\d+) instrucoes \((\d+) em modo supervisor, das quais (\d+) no laco ocioso\)", texto)
    if mm:
        r["halt"] = mm.group(1).startswith("CPU parada")
        r["real"] = int(mm.group(2))
    mm = re.search(r"relatorio final na instrucao (\d+)", texto)
    r["real_fim"] = int(mm.group(1)) if mm else None
    return r


def pct(a, b):
    return "%.1f%%" % (100.0 * a / b) if b else "-"


def milhares(v):
    return "-" if v is None else "{:,}".format(v).replace(",", ".")


def rotulo_proc(p):
    return "%s(%d)" % (p["prog"], p["arg"])


def tabela_geral(rs, titulo):
    print("### " + titulo + "\n")
    print("| execução | esc. | quantum | intervalo | tempo total | núcleo | ocioso | preempções | trocas de contexto | int. relógio | chamadas |")
    print("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for r in rs:
        print("| `%s` | %s | %d | %d | %s | %s | %s | %d | %d | %d | %d |" % (
            r["arquivo"][:-4], r["esc"], r["quantum"], r["intervalo"], milhares(r["total"]),
            pct(r["nucleo"], r["total"]), pct(r["ocioso"], r["total"]),
            r["preempcoes"], r["trocas"], r["irq"].get(10, 0), r["irq"].get(7, 0)))
    print()


def tabela_processos(rs, titulo, so_usuario=True):
    # colunas = processos (sem o init), linhas = execuções; célula = tempo de retorno
    print("### " + titulo + "\n")
    primeiro = rs[0]
    pids = [pid for pid in sorted(primeiro["procs"]) if not (so_usuario and pid == 1)]
    cab = [rotulo_proc(primeiro["procs"][pid]) for pid in pids]
    print("| execução | " + " | ".join(cab) + " | média |")
    print("|---|" + "---:|" * (len(cab) + 1))
    for r in rs:
        vals = [r["procs"][pid]["retorno"] for pid in pids if pid in r["procs"]]
        media = sum(v for v in vals if v is not None) // max(1, len(vals))
        print("| `%s` | " % r["arquivo"][:-4] + " | ".join(milhares(v) for v in vals) + " | %s |" % milhares(media))
    print()


def tabela_eco(rs, titulo):
    print("### " + titulo + "\n")
    print("| execução | retorno do eco | resposta média do eco | vezes bloqueado | tempo bloqueado | tempo pronto | tempo executando |")
    print("|---|---:|---:|---:|---:|---:|---:|")
    for r in rs:
        eco = [p for p in r["procs"].values() if p["prog"] == "eco"]
        if not eco:
            continue
        p = eco[0]
        print("| `%s` | %s | %s | %d | %s | %s | %s |" % (
            r["arquivo"][:-4], milhares(p["retorno"]), milhares(p["resp"]), p["nB"],
            milhares(p.get("tB")), milhares(p.get("tP")), milhares(p.get("tE"))))
    print()


def tabela_detalhe(r):
    print("#### `%s` -- %s, quantum %d x %d\n" % (r["arquivo"][:-4], r["esc"], r["quantum"], r["intervalo"]))
    print("| pid | programa | retorno | preempções | vezes P/B/E | tempo pronto | tempo bloqueado | tempo executando | resposta média |")
    print("|---:|---|---:|---:|---|---:|---:|---:|---:|")
    for pid in sorted(r["procs"]):
        p = r["procs"][pid]
        print("| %d | %s | %s | %d | %d/%d/%d | %s | %s | %s | %s |" % (
            pid, rotulo_proc(p), milhares(p["retorno"]), p["preemp"], p["nP"], p["nB"], p["nE"],
            milhares(p.get("tP")), milhares(p.get("tB")), milhares(p.get("tE")), milhares(p["resp"])))
    print()


def verificacao(rs):
    print("### Conferência do relógio do SO\n")
    print("Para cada execução: soma dos tempos \"executando\" de todos os processos + tempo ocioso (deve dar o tempo total), e "
          "diferença entre a instrução real em que o SO começou o relatório final (contada pelo executor) e o tempo total "
          "medido pelo SO (deve ser só o boot antes de o SO zerar o relógio mais o resto da última chamada -- quase constante).\n")
    print("| execução | tempo total (SO) | Σ executando + ocioso | instrução real do fim | diferença | maior entrada no núcleo |")
    print("|---|---:|---:|---:|---:|---:|")
    for r in rs:
        soma = sum(p.get("tE", 0) for p in r["procs"].values()) + (r["ocioso"] or 0)
        dif = (r["real_fim"] - r["total"]) if r["real_fim"] is not None else None
        print("| `%s` | %s | %s | %s | %s | %s |" % (
            r["arquivo"][:-4], milhares(r["total"]), milhares(soma), milhares(r["real_fim"]),
            milhares(dif), milhares(r["maior_entrada"])))
    print()


def main():
    pasta = sys.argv[1] if len(sys.argv) > 1 else "resultados"
    rs = {}
    for nome in sorted(os.listdir(pasta)):
        if nome.endswith(".txt"):
            r = le(os.path.join(pasta, nome))
            if r is not None:
                rs[nome[:-4]] = r

    def grupo(*nomes):
        return [rs[n] for n in nomes if n in rs]

    print("# Resumo dos experimentos (gerado por ferramentas/resume.py)\n")
    print("Tempos em instruções. \"núcleo\" e \"ocioso\" em porcentagem do tempo total.\n")

    A = grupo("A_misto_simples", "A_misto_rr", "A_misto_prio")
    tabela_geral(A, "A. Carga mista, os três escalonadores (quantum 5 x 4000)")
    tabela_processos(A, "A. Tempo de retorno por processo")
    tabela_eco(A, "A. O processo limitado por E/S (eco)")
    for r in A:
        tabela_detalhe(r)

    B = grupo("B_misto_rr_q1", "B_misto_rr_q2", "A_misto_rr", "B_misto_rr_q10", "B_misto_rr_q20",
              "B_misto_prio_q1", "B_misto_prio_q2", "A_misto_prio", "B_misto_prio_q10", "B_misto_prio_q20")
    tabela_geral(B, "B. Variando o quantum (carga mista, intervalo 4000)")
    tabela_processos(B, "B. Tempo de retorno por processo")
    tabela_eco(B, "B. O eco com quanta diferentes")

    C = grupo("C_cpu_simples", "C_cpu_rr", "C_cpu_prio", "C_cpu_rr_q1", "C_cpu_rr_q20")
    tabela_geral(C, "C. Só processos CPU-bound")
    tabela_processos(C, "C. Tempo de retorno por processo")

    D = grupo("D_misto_rr_L2000_q10", "A_misto_rr", "D_misto_rr_L10000_q2", "D_misto_rr_L20000_q1")
    tabela_geral(D, "D. Mesma fatia de tempo (20000 instruções), relógio com intervalos diferentes (RR)")
    tabela_processos(D, "D. Tempo de retorno por processo")

    E = grupo("E_lenta_simples", "E_lenta_rr", "E_lenta_prio")
    tabela_geral(E, "E. Console lenta (2000 instruções por caractere escrito)")
    tabela_processos(E, "E. Tempo de retorno por processo")
    for r in E:
        tabela_detalhe(r)

    T = grupo("testes_simples", "testes_rr", "testes_prio")
    tabela_geral(T, "Testes funcionais (cenário 1)")

    verificacao([rs[k] for k in sorted(rs)])


if __name__ == "__main__":
    main()
