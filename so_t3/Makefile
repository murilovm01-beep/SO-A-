# Makefile do T3
# antes: make na raiz do projeto (pra ter o mcc, o montador e o simulador)

MCC = ../compilador_c/bin/mcc
MONTADOR = ../simulador_completo/bin/montador
MOB2BIN = ../simulador_completo/bin/mob2bin
SIMULADOR = ../simulador_completo/bin/simulador
BIOS = ../bios_mancha/src/bios.asm

# cada programa ocupa 8 setores (4096 bytes) no disco, nessa ordem
# (tem que bater com os PROG_ do comum.h)
BINS = init.bin proibido.bin conta.bin fases.bin

all: so.mob disco.img

so.asm: so.c comum.h config.h
	$(MCC) so.c -o so.asm

# o ponte.asm vai por último porque marca onde o SO termina
so.mob: $(BIOS) so.asm ponte.asm
	$(MONTADOR) $(BIOS) so.asm ponte.asm -o so.mob

biblioteca.asm: biblioteca.c usuario.h comum.h
	$(MCC) biblioteca.c -o biblioteca.asm

# os programas são montados sozinhos, começando no endereço 0
%.asm: %.c usuario.h comum.h
	$(MCC) $< -o $@

%.mob: inicio.asm %.asm biblioteca.asm usuario.asm
	$(MONTADOR) inicio.asm $*.asm biblioteca.asm usuario.asm -o $@

%.bin: %.mob
	$(MOB2BIN) $< 4096 $@

disco.img: $(BINS)
	cat $(BINS) > disco.img

rodar: all
	$(SIMULADOR) so.mob -D disco.img

clean:
	rm -f so.asm so.mob biblioteca.asm init.asm proibido.asm conta.asm fases.asm
	rm -f init.mob proibido.mob conta.mob fases.mob $(BINS) disco.img

# sem isso o make apaga os .asm e .mob dos programas no fim
.SECONDARY:

.PHONY: all rodar clean
