# Makefile do T2 -- SO com processos para o Mancha completo (ver README.md)
#
#   make                         monta so.mob com a configuração padrão
#   make ESC=3 QUANTUM=2         outra configuração (regrava config.h)
#   make testes                  roda o cenário de testes no executor em lote
#   make experimentos            roda todas as configurações do relatório
#   make custo                   mede o custo de cada entrada no núcleo
#
# Variáveis de configuração (viram config.h):
#   ESC        1 = simples (Parte I), 2 = round-robin, 3 = prioridade
#   QUANTUM    quantum, em interrupções do relógio
#   INTERVALO  instruções entre interrupções do relógio
#   CENARIO    1 = testes, 2 = misto, 3 = só CPU (argumento do init)
#   VERBOSO    1 = o SO avisa na console quando cria/termina processos

MANCHA   ?= ..
MCC      = $(MANCHA)/compilador_c/bin/mcc
MONTADOR = $(MANCHA)/simulador_completo/bin/montador
SIMSRC   = $(MANCHA)/simulador_completo/src
BIOS     = $(MANCHA)/bios_mancha/src/bios.asm

ESC       ?= 2
QUANTUM   ?= 5
INTERVALO ?= 4000
CENARIO   ?= 2
VERBOSO   ?= 1

MCCFLAGS = -I. -Iusuario

# ordem de montagem importa: a BIOS primeiro (vetor em 0, código em 0x80),
# dados.asm por último (marca o fim da imagem)
FONTES = $(BIOS) nucleo/ponte.asm gerado/so.asm usuario/usuario.asm \
         gerado/biblioteca.asm gerado/programas.asm nucleo/dados.asm

all: so.mob bin/executor

so.mob: $(FONTES)
	$(MONTADOR) $(FONTES) -o $@

gerado/so.asm: nucleo/so.c comum.h config.h usuario/programas.h | gerado
	$(MCC) $(MCCFLAGS) nucleo/so.c -o $@

gerado/biblioteca.asm: usuario/biblioteca.c usuario/usuario.h comum.h | gerado
	$(MCC) $(MCCFLAGS) usuario/biblioteca.c -o $@

gerado/programas.asm: usuario/programas.c usuario/programas.h usuario/usuario.h comum.h | gerado
	$(MCC) $(MCCFLAGS) usuario/programas.c -o $@

gerado bin:
	mkdir -p $@

# config.h só é regravado quando a configuração muda (para não recompilar
# à toa)
config.h: FORCE
	@printf '// config.h -- GERADO pelo Makefile (make ESC=.. QUANTUM=.. INTERVALO=.. CENARIO=.. VERBOSO=..)\n#define ESCALONADOR       $(ESC)\n#define QUANTUM           $(QUANTUM)\n#define INTERVALO_RELOGIO $(INTERVALO)\n#define CENARIO           $(CENARIO)\n#define SO_VERBOSO        $(VERBOSO)\n' > config.h.novo
	@if cmp -s config.h.novo config.h; then rm -f config.h.novo; \
	 else mv config.h.novo config.h; echo "config.h: ESC=$(ESC) QUANTUM=$(QUANTUM) INTERVALO=$(INTERVALO) CENARIO=$(CENARIO) VERBOSO=$(VERBOSO)"; fi

# executor em lote: o mesmo núcleo de simulação, sem a tela curses
bin/executor: ferramentas/executor.c $(SIMSRC)/cpu.c $(SIMSRC)/dispositivos.c \
              $(SIMSRC)/memoria.c $(SIMSRC)/objeto.c $(SIMSRC)/instrucao.c | bin
	gcc -std=gnu11 -O2 -Wall -Wextra -I$(SIMSRC) -o $@ ferramentas/executor.c \
	    -Wl,--wrap=disp_le_byte,--wrap=disp_escreve_byte,--wrap=disp_tick \
	    $(SIMSRC)/cpu.c $(SIMSRC)/dispositivos.c $(SIMSRC)/memoria.c \
	    $(SIMSRC)/objeto.c $(SIMSRC)/instrucao.c

# cenário de testes funcionais; as linhas do passo [7] são "digitadas"
# depois que o sistema passa 20 mil instruções ocioso, esperando por elas
testes: bin/executor
	$(MAKE) so.mob CENARIO=1 VERBOSO=1
	./bin/executor so.mob -o "20000:primeira linha digitada" -o "20000:segunda linha digitada"

experimentos: bin/executor
	sh ferramentas/experimentos.sh

# custo de cada entrada no núcleo, por tipo (tabela da seção 6 do relatório)
bin/custo_nucleo: ferramentas/custo_nucleo.c $(SIMSRC)/cpu.c $(SIMSRC)/dispositivos.c \
                  $(SIMSRC)/memoria.c $(SIMSRC)/objeto.c $(SIMSRC)/instrucao.c | bin
	gcc -std=gnu11 -O2 -Wall -Wextra -I$(SIMSRC) -o $@ ferramentas/custo_nucleo.c \
	    $(SIMSRC)/cpu.c $(SIMSRC)/dispositivos.c $(SIMSRC)/memoria.c \
	    $(SIMSRC)/objeto.c $(SIMSRC)/instrucao.c

custo: bin/custo_nucleo
	$(MAKE) so.mob ESC=2 QUANTUM=5 INTERVALO=4000 CENARIO=2 VERBOSO=0
	./bin/custo_nucleo so.mob -e 51111:um -e 152222:dois -e 253333:tres -e 354444:quatro

clean:
	rm -rf gerado bin so.mob config.h config.h.novo

.PHONY: all testes experimentos custo clean FORCE
