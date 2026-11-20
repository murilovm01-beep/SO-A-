; inicio.asm -- vem primeiro em todo programa (o processo começa no endereço 0)

        jmp     _comeco
        .dw     _fim_programa       ; tamanho do programa, o SO lê daqui
_comeco:
        call    _f_main             ; o SO já deixou o argumento na pilha
        ld      r0, 8               ; main voltou: SO_MATA_PROC(0)
        ld      r1, 0
        trap    7
