; ponte.asm -- a parte do núcleo que NÃO dá para escrever no C do mcc.
;
; O núcleo do SO está quase todo em C (so.c). Este arquivo contém só o que
; a linguagem não expõe:
;
;   1. _kernel_inicio: instala os pontos de entrada do SO (na tabela de
;      chamadas da BIOS, no gancho do relógio e no vetor de interrupções)
;      e dá o primeiro "rete" manual, que coloca o processo init para rodar
;      em modo usuário;
;   2. os pontos de entrada de interrupção ("stubs"): cada um só anota QUAL
;      interrupção chegou (em r0) e salta para _so_entrada, o caminho comum;
;   3. _so_entrada: troca para a pilha do núcleo, chama a função C
;      so_trata_interrupcao() e, conforme a resposta, despacha um processo
;      ("rete"), deixa a CPU ociosa esperando interrupção, ou desliga;
;   4. funções de apoio chamadas pelo C (convenção do mcc: argumentos
;      empilhados, o primeiro em (bp+4), resultado em r0): acesso a portas
;      de E/S, leitura do relógio, cópia de quadro e impressão no console
;      (essas duas últimas reaproveitando as rotinas da BIOS).
;
; Montagem (ver Makefile): bios.asm ponte.asm so.asm ... dados.asm
;
; ------------------------------------------------------------------------
; Por que o núcleo NÃO roda em pilha_sistema (a "pegadinha" do enunciado):
; toda interrupção empilha o quadro de 16 palavras sempre no MESMO lugar
; (pilha_sistema-32, campo sp de todos os quadros do vetor na BIOS). O
; núcleo, então, logo na entrada (1) copia esse quadro para a entrada do
; processo interrompido na tabela de processos (feito em C, so.c) e
; (2) passa a usar a sua própria pilha, pilha_nucleo. Nada do núcleo fica
; guardado em pilha_sistema entre uma interrupção e outra; antes do "rete"
; o quadro do processo escolhido é copiado de volta para pilha_sistema-32.
; ------------------------------------------------------------------------

        ; públicos só para ferramentas externas (o executor em lote e
        ; custo_nucleo usam esses endereços para saber quando a CPU está
        ; ociosa, quando o núcleo foi chamado e quando o SO começou o
        ; relatório final; não mudam nada aqui)
        .pub    _so_ocioso_laco, _f_relatorio_final, _so_entrada

; ================================================================ início

_kernel_inicio:
        ; a imagem (código + dados de BIOS, núcleo e programas) tem que
        ; terminar antes da região usada pela pilha do núcleo (2KB abaixo
        ; de pilha_nucleo ficam reservados para ela)
        ld      r0, _so_fim_imagem
        ld      r1, pilha_nucleo-2048
        cmp     r0, r1
        jmpc    hi, _so_imagem_grande

        ; --- chamadas de sistema: usam o despachante da BIOS (trap 7 +
        ; tabela_syscalls). Todas as 5 chamadas apontam para o MESMO stub,
        ; que desvia para o caminho comum do núcleo (ver _so_int_sistema).
        ld      r0, _so_int_sistema
        st      r0, (tabela_syscalls+2)     ; 1 SO_LE
        st      r0, (tabela_syscalls+4)     ; 2 SO_ESCREVE
        st      r0, (tabela_syscalls+14)    ; 7 SO_CRIA_PROC
        st      r0, (tabela_syscalls+16)    ; 8 SO_MATA_PROC
        st      r0, (tabela_syscalls+18)    ; 9 SO_ESPERA_PROC

        ; --- relógio: gancho da BIOS (o handler da BIOS conta o tique em
        ; contador_tiques e salta para cá com o quadro já empilhado)
        ld      r0, _so_int_relogio
        st      r0, (retomada_escalonador)

        ; --- vetor de interrupções: o SO instala tratadores próprios para
        ; as exceções da CPU (a BIOS pararia a máquina inteira; o SO mata
        ; só o processo culpado) e para a console (a BIOS só guardaria o
        ; caractere num buffer e voltaria, sem avisar o SO -- aqui a
        ; chegada de entrada entra no núcleo, que desbloqueia leitores).
        ; Cada quadro do vetor tem 8 bytes: ip, sp, cs, ds -- só o ip muda.
        ld      r0, _so_int_excecao1
        st      r0, (8)                      ; 1 violação de segmento
        ld      r0, _so_int_excecao2
        st      r0, (16)                     ; 2 instrução privilegiada
        ld      r0, _so_int_excecao3
        st      r0, (24)                     ; 3 divisão por zero
        ld      r0, _so_int_excecao4
        st      r0, (32)                     ; 4 instrução ilegal
        ld      r0, _so_int_excecao5
        st      r0, (40)                     ; 5 ausência de quadro (paginação)
        ld      r0, _so_int_console
        st      r0, (64)                     ; 8 console

        ; --- parte em C: programa relógio e controlador de interrupções,
        ; cria o init e escreve o quadro dele em pilha_sistema-32
        ld      r0, pilha_sistema
        add     r0, -32
        push    r0
        call    _f_so_inicia
        add     sp, 2
        cmp     r0, 0
        jmpc    ne, _so_desliga

        ; primeiro "rete" manual: o sr do quadro do init tem S=0, I=0, D=0
        ; -> modo usuário, sem E/S, interrupções ligadas a partir daqui
        ld      sp, pilha_sistema
        add     sp, -32
        rete

_so_imagem_grande:
        ld      r0, _so_msg_imagem
        call    bios_imprime
        halt

; =========================================== pontos de entrada (stubs)
;
; Ao chegar em qualquer um deles: modo supervisor, D=1 (interrupções
; externas bloqueadas), sp = pilha_sistema-32 apontando para o quadro do
; contexto interrompido. Os registradores já estão salvos no quadro, então
; podemos sujar r0 à vontade.

; chamada de sistema: chegamos aqui por "call bp" do despachante da BIOS
; (_bios_chamada_sistema), que empilhou um endereço de retorno logo abaixo
; do quadro. Não vamos "retornar" para a BIOS (ela faria "rete" para o
; MESMO processo, e uma chamada pode bloquear o processo e passar a CPU
; para outro), então descartamos esse endereço: sp volta a apontar o quadro.
_so_int_sistema:
        add     sp, 2
        ld      r0, 7
        jmp     _so_entrada

_so_int_relogio:                             ; via retomada_escalonador (jmp da BIOS)
        ld      r0, 10
        jmp     _so_entrada

_so_int_console:
        ld      r0, 8
        jmp     _so_entrada

_so_int_excecao1:
        ld      r0, 1
        jmp     _so_entrada
_so_int_excecao2:
        ld      r0, 2
        jmp     _so_entrada
_so_int_excecao3:
        ld      r0, 3
        jmp     _so_entrada
_so_int_excecao4:
        ld      r0, 4
        jmp     _so_entrada
_so_int_excecao5:
        ld      r0, 5
        jmp     _so_entrada

; ==================================================== caminho comum

_so_entrada:
        ld      r1, sp                       ; r1 = endereço do quadro empilhado
        ld      sp, pilha_nucleo             ; o núcleo roda na SUA pilha
        push    r1                           ; 2º argumento: quadro
        push    r0                           ; 1º argumento: número da interrupção
        call    _f_so_trata_interrupcao
        add     sp, 4
        cmp     r0, 0
        jmpc    eq, _so_despacha
        cmp     r0, 1
        jmpc    eq, _so_ocioso
_so_desliga:
        halt                                 ; 2: o init morreu (ou pânico) -- fim
        jmp     _so_desliga

; so_trata_interrupcao() já copiou o quadro do processo escolhido para
; pilha_sistema-32; "rete" desempilha a partir de sp, então sp tem que
; apontar para lá.
_so_despacha:
        ld      sp, pilha_sistema
        add     sp, -32
        rete

; nenhum processo pronto: liga as interrupções e espera. A próxima
; interrupção (relógio, console) empilha um quadro deste laço em
; pilha_sistema-32 e entra de novo em _so_entrada -- o quadro do laço é
; simplesmente descartado (não há processo corrente para guardá-lo).
_so_ocioso:
        ld      sp, pilha_nucleo
        ei
_so_ocioso_laco:
        jmp     _so_ocioso_laco

; ============================================ funções de apoio (para o C)

; int porta_le(int porta)
_f_porta_le:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        in      r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; void porta_escreve(int porta, int valor)
_f_porta_escreve:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, (bp+6)
        out     r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; int le_contador_relogio(void) -- contador do relógio (portas 0x20/0x21).
; Os 16 bits ficam em duas portas de 8 bits, lidas em instruções
; diferentes; como o contador anda 1 a cada instrução, relemos a parte
; alta até ela não mudar entre as duas leituras.
_f_le_contador_relogio:
        in      r1, (0x20)
        in      r0, (0x21)
        in      r2, (0x20)
        cmp     r1, r2
        jmpc    ne, _f_le_contador_relogio
        shl     r1, 8
        or      r0, r1
        ret

; void copia_quadro(int *destino, int *origem) -- copia as 16 palavras de
; um quadro de contexto. É o mesmo padrão de bios_copia_quadro, mas com o
; laço desenrolado: a cópia acontece 2 vezes a cada entrada no núcleo, e o
; laço da BIOS custa ~115 instruções por cópia contra ~40 desta versão.
_f_copia_quadro:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)                   ; destino
        ld      r0, (bp+6)                   ; origem
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
        ld      r2, (r0+)
        st      r2, (r1+)
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

; void zera_palavras(int *p, int n)
_f_zera_palavras:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r2, (bp+6)
        ld      r0, 0
_so_zp_laco:
        cmp     r2, 0
        jmpc    le, _so_zp_fim
        st      r0, (r1+)
        add     r2, -1
        jmp     _so_zp_laco
_so_zp_fim:
        ld      sp, bp
        pop     bp
        ret

; int area_pilhas_usuario(void) -- endereço da área reservada (dados.asm)
; para as pilhas dos processos
_f_area_pilhas_usuario:
        ld      r0, _so_pilhas_usuario
        ret

_so_msg_imagem:
        .db     "SO: imagem grande demais (invade a pilha do nucleo)", 10, 0
