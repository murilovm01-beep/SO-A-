

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
