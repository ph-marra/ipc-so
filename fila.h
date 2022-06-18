#define MAX 10

struct fila{
    int no[MAX];
    int ini, cont;
};

typedef struct fila* Fila;

int cria_fila(Fila);
int fila_vazia(Fila);
int fila_cheia(Fila);
int insere_fim(Fila, int);
int remove_ini(Fila, int*);
