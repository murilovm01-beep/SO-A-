# Makefile do T2
# antes: make na raiz do projeto (pra ter o mcc, o montador e o simulador)

MCC = ../compilador_c/bin/mcc
MONTADOR = ../simulador_completo/bin/montador
SIMULADOR = ../simulador_completo/bin/simulador
BIOS = ../bios_mancha/src/bios.asm

all: so.mob

so.asm: so.c comum.h config.h programas.h
	$(MCC) so.c -o so.asm

biblioteca.asm: biblioteca.c usuario.h comum.h
	$(MCC) biblioteca.c -o biblioteca.asm

programas.asm: programas.c programas.h usuario.h comum.h
	$(MCC) programas.c -o programas.asm

# a bios tem que vir primeiro (o vetor de interrupções fica no endereço 0)
so.mob: $(BIOS) ponte.asm so.asm usuario.asm biblioteca.asm programas.asm
	$(MONTADOR) $(BIOS) ponte.asm so.asm usuario.asm biblioteca.asm programas.asm -o so.mob

rodar: so.mob
	$(SIMULADOR) so.mob

clean:
	rm -f so.mob so.asm biblioteca.asm programas.asm

.PHONY: all rodar clean
