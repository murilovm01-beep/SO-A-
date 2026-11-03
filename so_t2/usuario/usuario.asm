; usuario.asm -- chamadas de sistema do lado do processo (modo usuário).
;
; Convenção do SO (a mesma da BIOS): número da chamada em r0, argumentos
; em r1..r4, "trap 7", resultado em r0. As funções abaixo seguem a
; convenção do mcc (argumentos empilhados: o 1º em (bp+4)), para serem
; chamadas de C (ver usuario.h).
;
; Depois do "trap 7" a CPU entra no SO; quando o SO devolver a CPU a este
; processo (talvez muito depois, se a chamada bloqueou), o "rete" restaura
; todos os registradores do quadro salvo -- com r0 = valor de retorno, que
; o SO escreveu no quadro.

; int so_le(void)
_f_so_le:
        ld      r0, 1                ; SO_LE
        trap    7
        ret

; int so_escreve(int c)
_f_so_escreve:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 2                ; SO_ESCREVE
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
        ld      r0, 7                ; SO_CRIA_PROC
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_mata_proc(int pid)
_f_so_mata_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 8                ; SO_MATA_PROC
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_espera_proc(int pid)
_f_so_espera_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, 9                ; SO_ESPERA_PROC
        trap    7
        ld      sp, bp
        pop     bp
        ret

; Endereço de retorno da função principal de todo processo: o SO monta a
; pilha inicial de cada processo como se essa função tivesse sido chamada
; daqui. Quando ela retorna, o processo pede para morrer.
_f_proc_retorno:
        ld      r0, 8                ; SO_MATA_PROC
        ld      r1, 0                ; 0 = o próprio processo
        trap    7
        jmp     _f_proc_retorno      ; (não volta)

; void tenta_es_direta(void) -- I=0 no sr do processo: causa a interrupção
; 2 (instrução privilegiada) e o SO mata o processo
_f_tenta_es_direta:
        ld      r0, 33               ; '!'
        outb    r0, (1)
        ret

; void tenta_halt(void) -- S=0: interrupção 2 também
_f_tenta_halt:
        halt
        ret
