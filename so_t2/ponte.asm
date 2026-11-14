; ponte.asm -- parte do SO em assembly (o resto está em so.c)
;
; fiz em cima do exemplo multitarefa_c da bios. aqui fica só o que não dá
; pra fazer em C: instalar os tratadores, programar o relógio, o primeiro
; rete, o laço ocioso e umas funções pro C chamar as rotinas da bios.

        .equ    INTERVALO = 4000        ; instruções entre interrupções do relógio

_kernel_inicio:
        ; chamadas de sistema: uso o trap 7 da bios. as 5 chamadas apontam
        ; pro mesmo tratador (cada entrada da tabela tem 2 bytes)
        ld      r0, _so_int_sistema
        st      r0, (tabela_syscalls+2)     ; 1 SO_LE
        st      r0, (tabela_syscalls+4)     ; 2 SO_ESCREVE
        st      r0, (tabela_syscalls+14)    ; 7 SO_CRIA_PROC
        st      r0, (tabela_syscalls+16)    ; 8 SO_MATA_PROC
        st      r0, (tabela_syscalls+18)    ; 9 SO_ESPERA_PROC

        ; relógio: a bios conta o tique e depois pula pra cá
        ld      r0, _so_int_relogio
        st      r0, (retomada_escalonador)
        ld      r0, 0
        st      r0, (contador_tiques)

        ; exceções: o tratador da bios para a máquina inteira, então troco
        ; pelos do SO (que matam só o processo). 8 bytes por quadro do vetor
        ld      r0, _so_int_exc1
        st      r0, (8)
        ld      r0, _so_int_exc2
        st      r0, (16)
        ld      r0, _so_int_exc3
        st      r0, (24)
        ld      r0, _so_int_exc4
        st      r0, (32)

        ; console: continua sendo a bios que trata (ela guarda a tecla num
        ; buffer). só passo por aqui antes pra contar a interrupção
        ld      r0, _so_int_console
        st      r0, (64)

        ; programa o relógio e liga as interrupções da console e do relógio
        ; (igual no multitarefa_c)
        ld      r0, INTERVALO
        ld      r1, r0
        shr     r1, 8
        outb    r1, (0x22)
        outb    r0, (0x23)
        ld      r0, 5
        outb    r0, (0x30)

        ; so_inicia cria o init e põe o quadro dele em pilha_sistema-32
        ld      r0, pilha_sistema
        add     r0, -32
        push    r0
        call    _f_so_inicia
        add     sp, 2

        ; primeiro rete: começa o init (o sr dele tem S=0, então vai pro
        ; modo usuário). o sp tem que apontar pro quadro, senão o rete lê lixo
        ld      sp, pilha_sistema
        add     sp, -32
        rete

; ----------------------------------------------------- tratadores

; a bios chama isso com "call bp", então tem um endereço de retorno em
; cima do quadro. jogo ele fora: se voltasse pra bios ela daria rete pro
; mesmo processo, e o processo pode ter bloqueado
_so_int_sistema:
        add     sp, 2
        ld      r0, 7
        jmp     _so_entrada

_so_int_relogio:
        ld      r0, 10
        jmp     _so_entrada

_so_int_exc1:
        ld      r0, 1
        jmp     _so_entrada
_so_int_exc2:
        ld      r0, 2
        jmp     _so_entrada
_so_int_exc3:
        ld      r0, 3
        jmp     _so_entrada
_so_int_exc4:
        ld      r0, 4
        jmp     _so_entrada

_so_int_console:
        ld      r0, (_g_n_int_console)
        add     r0, 1
        st      r0, (_g_n_int_console)
        jmp     _bios_trata_console

; todos os tratadores do SO chegam aqui com r0 = número da interrupção e
; sp apontando pro quadro que a cpu empilhou (pilha_sistema-32).
; o SO roda na pilha_nucleo, porque a pilha_sistema é sobrescrita a cada
; interrupção (isso está no README da bios)
_so_entrada:
        ld      r1, sp
        ld      sp, pilha_nucleo
        push    r1                      ; quadro
        push    r0                      ; número da interrupção
        call    _f_so_trata_interrupcao
        add     sp, 4
        cmp     r0, 0
        jmpc    eq, _so_volta
        cmp     r0, 1
        jmpc    eq, _so_ocioso
        halt                            ; 2: o init morreu, acabou

; o C já copiou o quadro do processo escolhido pra pilha_sistema-32
_so_volta:
        ld      sp, pilha_sistema
        add     sp, -32
        rete

; ninguém pra rodar: liga as interrupções e fica esperando o relógio
_so_ocioso:
        ld      sp, pilha_nucleo
        ei
_so_ocioso_laco:
        jmp     _so_ocioso_laco

; ------------------------------------------- funções chamadas pelo C
; (no mcc o 1º argumento fica em (bp+4), o 2º em (bp+6), e a volta em r0;
;  as rotinas da bios recebem em r0, r1...)

; void copia_quadro(int *destino, int *origem)
_f_copia_quadro:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, (bp+6)
        call    bios_copia_quadro
        ld      sp, bp
        pop     bp
        ret

; void k_putc(int c)
_f_k_putc:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_putc
        ld      sp, bp
        pop     bp
        ret

; void k_imprime(char *s)
_f_k_imprime:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_imprime
        ld      sp, bp
        pop     bp
        ret

; void k_dec(int v)
_f_k_dec:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_imprime_dec
        ld      sp, bp
        pop     bp
        ret

; void k_hex(int v)
_f_k_hex:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_imprime_hex
        ld      sp, bp
        pop     bp
        ret

; int console_tem_tecla(void) -- quantas teclas tem no buffer da bios
_f_console_tem_tecla:
        call    bios_console_disponivel
        ret

; int console_le_tecla(void)
_f_console_le_tecla:
        call    bios_console_le
        ret

; int console_pronta(void) -- a bios não tem função pra saber se dá pra
; escrever, então leio o bit 0 da porta de estado da console
_f_console_pronta:
        in      r0, (2)
        and     r0, 1
        ret

; int tiques(void) -- interrupções do relógio desde o boot (a bios conta)
_f_tiques:
        ld      r0, (contador_tiques)
        ret

; int intervalo_relogio(void)
_f_intervalo_relogio:
        ld      r0, INTERVALO
        ret

; void prepara_relatorio(void) -- usada só no fim. o relógio para de chamar
; o SO e as interrupções voltam, senão a console não recebe o E que o
; relatório espera entre uma página e outra
_f_prepara_relatorio:
        ld      r0, 0
        st      r0, (retomada_escalonador)
        ei
        ret
