; ponte.asm -- parte do SO em assembly (o resto está em so.c)
; é montado depois do so.asm, o rótulo do final marca onde o SO termina

        .equ    INTERVALO = 20000       ; instruções entre interrupções do relógio

_kernel_inicio:
        ld      r0, _so_int_sistema
        st      r0, (tabela_syscalls+2)     ; 1 SO_LE
        st      r0, (tabela_syscalls+4)     ; 2 SO_ESCREVE
        st      r0, (tabela_syscalls+14)    ; 7 SO_CRIA_PROC
        st      r0, (tabela_syscalls+16)    ; 8 SO_MATA_PROC
        st      r0, (tabela_syscalls+18)    ; 9 SO_ESPERA_PROC

        ld      r0, _so_int_relogio
        st      r0, (retomada_escalonador)
        ld      r0, 0
        st      r0, (contador_tiques)

        ; exceções e falta de página: troco os tratadores da bios (8 bytes
        ; por entrada do vetor, o ip é a primeira palavra)
        ld      r0, _so_int_exc1
        st      r0, (8)
        ld      r0, _so_int_exc2
        st      r0, (16)
        ld      r0, _so_int_exc3
        st      r0, (24)
        ld      r0, _so_int_exc4
        st      r0, (32)
        ld      r0, _so_int_pagina
        st      r0, (40)

        ld      r0, _so_int_console
        st      r0, (64)

        ; relógio e interrupções (igual no multitarefa_c)
        ld      r0, INTERVALO
        ld      r1, r0
        shr     r1, 8
        outb    r1, (0x22)
        outb    r0, (0x23)
        ld      r0, 5
        outb    r0, (0x30)

        ld      r0, pilha_sistema
        add     r0, -32
        push    r0
        call    _f_so_inicia
        add     sp, 2

        ld      sp, pilha_sistema
        add     sp, -32
        rete

; ----------------------------------------------------- tratadores

; a bios chama com "call bp", jogo fora o endereço de retorno
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
_so_int_pagina:
        ld      r0, 5
        jmp     _so_entrada

_so_int_console:
        ld      r0, (_g_n_int_console)
        add     r0, 1
        st      r0, (_g_n_int_console)
        jmp     _bios_trata_console

; r0 = número da interrupção, sp = quadro empilhado (pilha_sistema-32)
_so_entrada:
        ld      r1, sp
        ld      sp, pilha_nucleo
        push    r1
        push    r0
        call    _f_so_trata_interrupcao
        add     sp, 4
        cmp     r0, 0
        jmpc    eq, _so_volta
        cmp     r0, 1
        jmpc    eq, _so_ocioso
        halt

_so_volta:
        ld      sp, pilha_sistema
        add     sp, -32
        rete

_so_ocioso:
        ld      sp, pilha_nucleo
        ei
_so_ocioso_laco:
        jmp     _so_ocioso_laco

; ------------------------------------------- funções chamadas pelo C
; (argumentos em bp+4, bp+6...; a bios recebe em r0, r1...)

_f_copia_quadro:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, (bp+6)
        call    bios_copia_quadro
        ld      sp, bp
        pop     bp
        ret

_f_k_putc:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_putc
        ld      sp, bp
        pop     bp
        ret

_f_k_imprime:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_imprime
        ld      sp, bp
        pop     bp
        ret

_f_k_dec:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_imprime_dec
        ld      sp, bp
        pop     bp
        ret

_f_k_hex:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_imprime_hex
        ld      sp, bp
        pop     bp
        ret

; void le_setor(int face, int trilha, int setor, int *destino)
_f_le_setor:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        ld      r1, (bp+6)
        ld      r2, (bp+8)
        ld      r3, (bp+10)
        call    bios_disco_le_setor
        ld      sp, bp
        pop     bp
        ret

; void zera(int *p, int n) -- em C ficava lento (criar processo zera 2 páginas)
_f_zera:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        ld      r2, (bp+6)
        ld      r1, 0
_zera_laco:
        st      r1, (r0+)
        add     r2, -1
        jmpc    ne, _zera_laco
        ld      sp, bp
        pop     bp
        ret

_f_console_tem_tecla:
        call    bios_console_disponivel
        ret

_f_console_le_tecla:
        call    bios_console_le
        ret

; a bios não tem função pra isso: bit 0 da porta de estado da console
_f_console_pronta:
        in      r0, (2)
        and     r0, 1
        ret

_f_tiques:
        ld      r0, (contador_tiques)
        ret

_f_intervalo_relogio:
        ld      r0, INTERVALO
        ret

; só no fim: o relógio para de chamar o SO e a console volta a receber o E
_f_prepara_relatorio:
        ld      r0, 0
        st      r0, (retomada_escalonador)
        ei
        ret

_f_fim_do_so:
        ld      r0, _fim_do_so
        ret

_fim_do_so:
