#include <stdlib.h>
#include "fila.h"

int cria_fila(Fila f){
    if(f == NULL) return 0;

    f->ini = 0;
    f->cont = 0;

    return 1;
}

int fila_vazia(Fila f){
    return(f->cont == 0);
}

int fila_cheia(Fila f){
    return(f->cont == MAX);
}

int insere_fim(Fila f, int elem){
    if(fila_cheia(f))
        return 0;

    f->no[(f->ini + f->cont) % MAX] = elem;
    f->cont++;
    return 1;
}

int remove_ini(Fila f, int *elem){
    if(fila_vazia(f))
        return 0;

    *elem = f->no[f->ini];
    f->ini = (f->ini + 1) % MAX;
    f->cont--;
    return 1;
}