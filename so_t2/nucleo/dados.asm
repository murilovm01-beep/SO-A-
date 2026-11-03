; dados.asm -- áreas grandes do núcleo, reservadas com ".ds" (uma linha de
; fonte, nenhum byte gerado) em vez de arrays C: o mcc escreve cada byte de
; um array global como uma linha ".db 0", e o montador tem limite de linhas.
;
; MONTADO POR ÚLTIMO (ver Makefile): _so_fim_imagem marca o fim de tudo, e
; _kernel_inicio confere que esse fim fica abaixo da pilha do núcleo.

        .data

; pilhas dos processos de usuário: MAX_PROC (8) x TAM_PILHA (512 bytes)
; = 4096 bytes = 2048 palavras -- ver so.c (cria_processo)
_so_pilhas_usuario:
        .ds     2048

_so_fim_imagem:
