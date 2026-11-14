// programas.h -- programas que o SO sabe criar (a tabela está no so.c)
// todo programa é "void nome(int arg)"; quando a função acaba o processo morre

#ifndef PROGRAMAS_H
#define PROGRAMAS_H

void init(int cenario);
void cpu(int n);         // só faz conta (n unidades)
void eco(int n);         // lê n linhas da console e repete
void proibido(int tipo); // tenta fazer coisa proibida
void laco(int x);        // laço infinito
void filho(int n);       // faz conta e termina
void pai(int n);         // cria n filhos e espera eles

#endif
