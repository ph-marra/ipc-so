// Pedro Henrique Marra Araújo - 12011BCC008

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <semaphore.h>
#include <time.h>
#include "fila.h"

#define TRUE 1
#define FALSE 0
#define PROCS_NUMBER 6 // Qtd de processos a serem criados.
#define THREADS_NUMBER 10
#define MAX_N 1000 // Maior número sortiável: [1, MAX_N].
#define N_NUMBER 10000 // Quantidade de nos sorteados.

// Abaixo, etiqueta para cada processo/thread e das filas.
#define P5_NUMBER 10
#define P6_NUMBER 11
#define P7_NUMBER 12
#define P7_T2_NUMBER 13
#define P7_T3_NUMBER 14
#define F1_NUMBER 5678
#define F2_NUMBER 5679

void process_123();
void process_4();
void* threads_4(void* pipe);
void process_56(int p_number, int pipe[2], int* counter);
void process_7();
void* threads_7(void* t_number);
void relatorio();

/*
Struct da F1: temos o semáforo, o PID de P4 (para os processos
P1, P2 e P3 enviarem um sinal para o P4 para esvaziar F1), a
fila em si e o estado da F1 em relação a troca de processos
produtores (P1, P2 e P3) e o P4 (processo consumidor). Esse esta-
do de pronta diz que F1 está pronta para ser esvaziada (é
necessária, pois é pedido que se encha por completo F1 e que se
esvazie por completo F1, sem que seja retirado ou colocado, respec-
tivamente, elementos durante esses procedimentos) (como não foi es-
pecificado como essa comunicação de P4 para P1, P2 e P3 deveria a-
contecer, preferi - pois mais fácil - fazer assim).
*/
struct sem_fila{
    sem_t sem;

    pid_t p4;
    int pronta;
    struct fila fila;
};
typedef struct sem_fila* Sem_Fila;

int shmid_f1;
Sem_Fila f1;

/*
Struct da F2: Busy-wait (por turn), a fila F2, um vetor
contador de frequência dos n(os) sorteados e três contadores
da quantidade de n(os) processados por cada processo, n_p5,
n_p6 e n_p7 por P5, P6 e P7, respectivamente.
*/
struct bw_fila{
    int turn;

    struct fila fila;
    int freq[MAX_N];
    int n_p5, n_p6, n_p7;
};
typedef struct bw_fila* Bw_Fila;

int shmid_f2;
Bw_Fila f2;

struct timespec ti;
int pipe_1[2], pipe_2[2];
pid_t pids[PROCS_NUMBER];

int main(){
    clock_gettime(CLOCK_REALTIME, &ti);   

    /* Criação da memória compartilhada da F1.
    
       Nela, haverá o semáforo (compartilhado pelos
       processos P1, P2 e P3 para escrever em F1), o
       pid do P4 para que possa ser dado o sinal para
       P4 começar a consumir os elementos da fila
       quando ela estiver cheia), a fila F1 e uma
       variável que diz que a F1 está pronta pra
       ser esvaziada (pelo P4).
    */

    key_t f1_number = F1_NUMBER;
    shmid_f1 = shmget(f1_number, sizeof(struct sem_fila), 0666 | IPC_CREAT);
    if(shmid_f1 == -1) exit(EXIT_FAILURE);

    void* shm_f1 = shmat(shmid_f1, (void*) 0, 0);
    if(shm_f1 == (void*) -1) exit(EXIT_FAILURE);

    f1 = (Sem_Fila) shm_f1;

    f1->p4 = 0; f1->pronta = 0;

    if(sem_init(&f1->sem, 1, 1)) exit(EXIT_FAILURE);

    cria_fila(&f1->fila);

    /* Fim da criação da memória compartilhada da F1. */

    /* Criação da memória compartilhada da F2.
    
       Nela, terá o turn para a exclusão mútua com
       busy-wait (inicia com P5), a quantidade de valores
       processador por cada processo (exemplo, n_p5 para
       quantidade processada pelo processo P5), um vetor
       contador de frequência de cada número e a própria
       fila F2.
    */

    shmid_f2 = shmget((key_t)F2_NUMBER, sizeof(struct bw_fila), 0666 | IPC_CREAT);
    if(shmid_f2 == -1) exit(EXIT_FAILURE);

    void* shm_f2 = shmat(shmid_f2, (void*) 0, 0);
    if(shm_f2 == (void*) -1) exit(EXIT_FAILURE);

    f2 = (Bw_Fila) shm_f2;
    f2->n_p5 = 0; f2->n_p6 = 0; f2->n_p7 = 0; // Nenhum n(o) ainda lido.
    f2->turn = P5_NUMBER; // Começo com turn no P5.

    cria_fila(&f2->fila);

    for(int i = 0; i < MAX_N; i++)
        f2->freq[i] = 0; // Zera a frequência dos n(os).


    /* Fim da criação da memória compartilhada da F2. */

    /* Criação do pipe1 e pipe2 */

    // Se pelo menos um retorno é != 0, algum pipe não
    // foi criado com sucesso, logo falha.
    if(pipe(pipe_1) == -1) exit(EXIT_FAILURE);
    if(pipe(pipe_2) == -1) exit(EXIT_FAILURE);

    /* Fim da criação dos pipes. */

    /* Criação de um grupo de processos e dos
       processos e a delegação de suas
       atribuições (distribuição dos processos). */

    int processo = -1;
    srand(time(NULL));

    // Criação de um grupo de processos para depois dar
    // kill em todos os processos de forma mais fácil.
    // O pai de todos é o líder do grupo.

    for(processo = 1; processo <= PROCS_NUMBER; processo++)
        if((pids[processo-1] = fork()) == 0) break;

    if(processo == 1) process_4();
    else if(processo == 2) process_123();
    else if(processo == 3) process_123();
    else if(processo == 4) process_123();
    // P5 usa o pipe_1.
    else if(processo == 5) process_56(P5_NUMBER, pipe_1, &f2->n_p5);
    // P6 usa o pipe_2.
    else if(processo == 6) process_56(P6_NUMBER, pipe_2, &f2->n_p6);
    else process_7();

    /* Fim da criação e delegação dos processos. */
}

void process_123(){
    int r;

    while(TRUE){
        sem_wait(&f1->sem);

        // Se o P4 não foi instanciado (para passar vez)
        // ou se P4 está pronto pra consumir de F1,
        // (foi instanciado e F1 está cheia), então
        // fechar semáforo desse processo produtor
        if(!f1->p4 || f1->pronta) {
            sem_post(&f1->sem);
            continue;
        }

        r = 1 + (rand() % MAX_N);
        // Se o P4 já foi instanciado (para passar vez)
        // e se a F1 não está pronta pra ser esvaziada,
        // podemos inserir ainda em F1, assim, se inseriu
        // e então a F1 ficou cheia, mandar sinal p/ P4
        if(!f1->pronta && insere_fim(&f1->fila, r) && fila_cheia(&f1->fila)){
            kill(f1->p4, SIGUSR1); 
            continue;
        }

        sem_post(&f1->sem);
    }
}

/*
    Trigger para P4 começar a esvaziar F1.
    Ou seja, P4 recebe o sinal SIGUSR1 do processo
    produtor (P1, P2 ou P3) que encheu a F1 e diz
    que a partir de então P4 é quem mexerá com F1
    (nesse caso, esvaziará por completo).
*/
void trigger_p4(){
    f1->pronta = 1;
    sem_post(&f1->sem);
}

void process_4(){
    sem_wait(&f1->sem);
    f1->p4 = getpid();
    sem_post(&f1->sem);

    signal(SIGUSR1, trigger_p4);

    pthread_t tid2;
    // Criação da segunda thread de P4, que trabalhará
    // com o pipe_2 (essa fornecerá para P5)
    pthread_create(&tid2, NULL, threads_4, pipe_2);

    // A thread principal de P4 trabalhará com o pipe_1
    // (essa fornecerá para P6)
    threads_4(pipe_1);
}

/*
    Trigger para P1, P2 e P3 (produtores) começarem
    a encher por completo a F1.
*/
void trigger_p123(){
    f1->pronta = 0;
    sem_post(&f1->sem);
}

/*
    A função das threads receberam um pipe, ou seja,
    pipe_1 ou pipe_2, dependendo de qual thread está.
*/
void* threads_4(void* p){
    int elem, *pipe = (int*) p;

    while(TRUE){
        sem_wait(&f1->sem);

            // Se a F1 vazia, P1, P2 e P3 devem encher por
            // completo, logo temos que dizer para esses
            // processos (com um trigger) que eles podem
            // voltar a encher por completo F1 
            if(fila_vazia(&f1->fila)) {
                trigger_p123();
                continue;
            }

            // Se a F1 está pronta para ser esvaziada e
            // é possível remover um elemento da F1, ou
            // seja, F1 não está vazia, então escrever
            // esse elemento retirado no respectivo pipe
            if(f1->pronta && remove_ini(&f1->fila, &elem))
                write(pipe[1], &elem, sizeof(int));

            // Se essa thread acabou de esvaziar F1, então
            // mandar P1, P2 e P3 podem voltar a produzir
            if(fila_vazia(&f1->fila)) {
                trigger_p123();
                continue;
            }

        sem_post(&f1->sem);
    }
}

// ---------------------------------------------------------------------------------------------

/* Escolhe o próximo processo produtor de F2 (P5 ou P6). */
int next_process_producer(){
    int r = rand() % 2;

    if(r == 0) return P5_NUMBER;
    else return P6_NUMBER;  
}

/* Escolhe a próxima thread consumidora de F2 (as threads de P7). */
int next_thread_consumer(){
    int r = rand() % 3;

    if(r == 0) return P7_NUMBER;
    if(r == 1) return P7_T2_NUMBER;
    else       return P7_T3_NUMBER;
}

/* Escolhe a próxima thread que atualizará F2 (tanto produtora, quanto
   consumidora). */
int next_thread(){
    //srand(time(NULL));
    int r = rand() % 5;

    if(r == 0 || r == 1) return next_process_producer();
    else return next_thread_consumer();
}

// ---------------------------------------------------------------------------------------------

/*
    Passa a etiqueta do processo (para distinguir entre P5 e P6),
    o respectivo pipe (pipe_1 para P5 e pipe_2 para P6) e o ponteiro
    do membro do respectivo contator do processo (&n_p5 ou &n_p6).
*/
void process_56(int p_number, int pipe[2], int* counter){
    int elem;

    while(TRUE){
        // Busy-wait (não é a vez desse processo)
        while(f2->turn != p_number){}

        // Não foi possível ler do pipe,
        // logo escolha outra thread para
        // pegar a vez na exclusão mútua
        if(read(pipe[0], &elem, sizeof(int)) <= 0) f2->turn = next_thread();

        // Se já passaram N_NUMBER por P7, então esse
        // processo aqui não precisa mais produzir elemento
        // na F2 (assim, já chama o relatório)
        if(f2->n_p7 == N_NUMBER) relatorio();
        
        // Se F2 não está cheia, lê do respectivo pipe,
        // insere no fim de F2 e acresce o contator
        if(!fila_cheia(&f2->fila)){
            insere_fim(&f2->fila, elem);
            (*counter)++;
        }
        
        // Escolhi chamar a próxima thread consumidora de P7,
        // para balancear entre produtores (acabou de inserir em F2)
        // e consumidores (para diminuir a chance de F2 ficar muito
        // tempo ou vazia ou cheia)
        f2->turn = next_thread_consumer();
    }
}

void process_7(){
    pthread_t tid7_2, tid7_3;
    int p7_t1 = P7_NUMBER;
    int p7_t2 = P7_T2_NUMBER;
    int p7_t3 = P7_T3_NUMBER;

    // Cria outras duas threads (contando com a principal,
    // três threads para P7) e passa para a função a etiqueta
    // de cada thread para diferenciá-las
    pthread_create(&tid7_2, NULL, threads_7, &p7_t2);
    pthread_create(&tid7_3, NULL, threads_7, &p7_t3);
    threads_7(&p7_t1);
}

/*
    Passa a etiqueta do processo (para distinguir entre as threads).
*/
void* threads_7(void* t_number){
    int elem, t = *(int*)t_number, t_etiqueta;

    switch(t){
        case P7_NUMBER: t_etiqueta = 1; break;
        case P7_T2_NUMBER: t_etiqueta = 2; break;
        case P7_T3_NUMBER: t_etiqueta = 3; break;
    }

    while(TRUE){
        // Busy-wait (não é a vez dessa thread)
        while(f2->turn != t){}

        // Se já passaram N_NUMBER por P7, então essa
        // thread aqui não precisa mais consumir elemento
        // da F2 (assim, já chama o relatório)
        if(f2->n_p7 == N_NUMBER) relatorio();

        // Se a fila está vazia, não há elementos
        // para essa thread consumir, logo passa a vez p/
        // um processo produtor (P5 ou P6).
        if(fila_vazia(&f2->fila)){
            f2->turn = next_process_producer();
            continue;
        }

        // Como a fila não está vazia, remove de F2 elemento
        // acresce sua frequência e acresce o contator dos
        // elementos passados por P7 (passa no máximo N_NUMBER)
        remove_ini(&f2->fila, &elem);
        f2->freq[elem-1]++;
        //printf("P7(T%d): removeu %d de F2 - (%d-ésimo elemento).\n", t_etiqueta, elem, f2->n_p7 + 1);
        f2->n_p7++;

        f2->turn = next_process_producer();
    }
}

void relatorio(){
    int min = -1, max = 1, moda = 1;

    for(int i = 0; i < MAX_N; i++){
        // Acha o número em [1, MAX_N] que mais apareceu.
        if(f2->freq[i] > f2->freq[moda-1]) moda = i+1;
        
        // Acha o último elemento sorteado pelo menos 1 vez
        // (f2->freq[i] != 0), ou seja, o maior n(o) sorteado
        if(f2->freq[i]) max = i + 1;

        // Acha o menor n(o) sorteado, isto é
        if(min == -1 && f2->freq[i] != 0) min = i + 1;
    }

    struct timespec tf;
    clock_gettime(CLOCK_REALTIME, &tf);

    // Diferença em nanosegundos (se tem mais de um segundo, desconsidero os nanosegundos)
    time_t diff_sec = tf.tv_sec - ti.tv_sec;
    // Diferença em segundos
    long diff_nanosec = tf.tv_nsec - ti.tv_nsec;

    printf("\nRelatório:\n\n");

    printf("Tempo = ");
    if(diff_sec > 0) printf("%lds\n", diff_sec); else printf("%ldms\n", diff_nanosec/1000000);
    printf("Valores processador por P5 = %d\n", f2->n_p5);
    printf("Valores processador por P6 = %d\n", f2->n_p6);
    printf("Menor = %d\n", min);
    printf("Maior = %d\n", max);
    printf("Moda = { ");

    // Imprimo todos os elementos com a mesma frequência que
    // o último elemento pertencente à moda (n-modal)
    for(int i = 0; i < MAX_N; i++)
        if(f2->freq[i] == f2->freq[moda-1]) printf("%d ", i+1);
    printf("}\n");
    
    // Como fprintf é uma função bufferizada, logo para descarregar
    // as mensagens acima, é necessário um fflush p/ certificar.
    fflush(stdout);

    // Fechando os pipes
    close(pipe_1[0]); close(pipe_1[1]);
    close(pipe_2[0]); close(pipe_2[1]);

    // Fechando as shared memories
    shmdt(f1); shmdt(f2);
    shmctl(shmid_f1, IPC_RMID, NULL);
    shmctl(shmid_f2, IPC_RMID, NULL);

    // Mata todos os processos criados pelo programa.
    for(int i = 0; i <= PROCS_NUMBER; i++)
        kill(pids[i], SIGKILL);

    // Amém esse trabalho acabou! Quase que ele acaba comigo!
}