; usuario.asm -- chamadas de sistema do lado do processo
;
; número da chamada em r0, argumentos em r1 e r2, trap 7. quando o SO
; devolve a CPU, o rete restaura os registradores e o r0 já vem com a
; resposta. (convenção do mcc: 1º argumento em bp+4, 2º em bp+6)

; int so_le(void)
_f_so_le:
        ld      r0, 1
        trap    7
        ret

; int so_escreve(int c)
_f_so_escreve:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 2
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_cria_proc(char *nome, int arg)
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

; int so_mata_proc(int pid)
_f_so_mata_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 8
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_espera_proc(int pid)
_f_so_espera_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 9
        trap    7
        ld      sp, bp
        pop     bp
        ret

; quando o main de um processo dá ret, ele cai aqui (o SO coloca esse
; endereço na pilha quando cria o processo) e o processo pede pra morrer
_f_proc_retorno:
        ld      r0, 8
        ld      r1, 0
        trap    7
        jmp     _f_proc_retorno

; com I=0 o outb dá interrupção 2 (instrução privilegiada)
_f_tenta_es_direta:
        ld      r0, 33
        outb    r0, (1)
        ret

; com S=0 o halt também
_f_tenta_halt:
        halt
        ret
