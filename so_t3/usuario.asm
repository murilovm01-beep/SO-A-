; usuario.asm -- chamadas de sistema do lado do processo
; (número em r0, argumentos em r1 e r2, trap 7, resposta volta em r0)

_f_so_le:
        ld      r0, 1
        trap    7
        ret

_f_so_escreve:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 2
        trap    7
        ld      sp, bp
        pop     bp
        ret

_f_so_cria_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r2, (bp+6)
        ld      r0, 7
        trap    7
        ld      sp, bp
        pop     bp
        ret

_f_so_mata_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 8
        trap    7
        ld      sp, bp
        pop     bp
        ret

_f_so_espera_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 9
        trap    7
        ld      sp, bp
        pop     bp
        ret

; este arquivo é montado por último, então isso marca o fim do programa
_fim_programa:
