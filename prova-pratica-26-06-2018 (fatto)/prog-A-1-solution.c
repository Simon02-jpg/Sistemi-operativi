/******************************************************************
Welcome to the Operating System examination

You are editing the '/home/esame/prog.c' file. You cannot remove 
this file, just edit it so as to produce your own program according to
the specification listed below.

In the '/home/esame/'directory you can find a Makefile that you can 
use to compile this program to generate an executable named 'prog' 
in the same directory. Typing 'make posix' you will compile for 
Posix, while typing 'make winapi' you will compile for WinAPI just 
depending on the specific technology you selected to implement the
given specification. Most of the required header files (for either 
Posix or WinAPI compilation) are already included at the head of the
prog.c file you are editing. 

At the end of the examination, the last saved snapshot of this file
will be automatically stored by the system and will be then considered
for the evaluation of your exam. Modifications made to prog.c which you
did not save via the editor will not appear in the stored version
of the prog.c file. 
In other words, unsaved changes will not be tracked, so please save 
this file when you think you have finished software development.
You can also modify the Makefile if requested, since this file will also
be automatically stored together with your program and will be part
of the final data to be evaluated for your exam.

PLEASE BE CAREFUL THAT THE LAST SAVED VERSION OF THE prog.c FILE (and of
the Makfile) WILL BE AUTOMATICALLY STORED WHEN YOU CLOSE YOUR EXAMINATION 
VIA THE CLOSURE CODE YOU RECEIVED, OR WHEN THE TIME YOU HAVE BEEN GRANTED
TO DEVELOP YOUR PROGRAM EXPIRES. 


SPECIFICATION TO BE IMPLEMENTED:
Implementare un programma che riceva in input tramite argv[1] un numero
intero N maggiore o uguale ad 1 (espresso come una stringa di cifre 
decimali), e generi N nuovi thread. Ciascuno di questi, a turno, dovra'
inserire in una propria lista basata su memoria dinamica un record
strutturato come segue:

typedef struct _data{
	int val;
	struct _data* next;
} data; 

I record vengono generati e popolati dal main thread, il quale rimane
in attesa indefinita di valori interi da standard input. Ad ogni nuovo
valore letto avverra' la generazione di un nuovo record, che verra'
inserito da uno degli N thread nella sua lista. 
L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra' 
stampare a terminale il contenuto corrente di tutte le liste (ovvero 
i valori interi presenti nei record correntemente registrati nelle liste
di tutti gli N thread). 

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _data {
    int val;
    struct _data* next;
} data;

volatile sig_atomic_t sigint_flag = 0;
int N;
data **lists;
sem_t *sem_threads;
sem_t sem_main;

// <-- MODIFICATO: Adesso condividiamo il puntatore al record già allocato dal main
data *shared_node = NULL; 

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void print_lists() {
    write(STDOUT_FILENO, "\n--- CONTENUTO ATTUALE DELLE LISTE ---\n", 39);
    for (int i = 0; i < N; i++) {
        printf("Lista Thread %d: ", i);
        data *curr = lists[i];
        if (!curr) printf("[vuota]");
        while (curr) {
            printf("%d ", curr->val);
            curr = curr->next;
        }
        printf("\n");
    }
    fflush(stdout);
}

void* worker(void* arg) {
    long id = (long)arg;
    while (1) {
        sem_wait(&sem_threads[id]);
        
        // Condizione di uscita pulita (Sentinella di SHUTDOWN)
        if (shared_node == NULL) {
            break;
        }
        
        // <-- MODIFICATO: Il thread NON fa la malloc. Si limita a prendere il record 
        // generato dal main e ad inserirlo in testa alla sua lista.
        shared_node->next = lists[id];
        lists[id] = shared_node;
        
        sem_post(&sem_main);
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc < 2) exit(EXIT_FAILURE);
    
    N = atoi(argv[1]);
    if (N < 1) exit(EXIT_FAILURE);

    lists = calloc(N, sizeof(data*));
    sem_threads = malloc(N * sizeof(sem_t));
    pthread_t *tids = malloc(N * sizeof(pthread_t));
    if (!lists || !sem_threads || !tids) exit(EXIT_FAILURE);

    sem_init(&sem_main, 0, 0);
    for (int i = 0; i < N; i++) {
        sem_init(&sem_threads[i], 0, 0);
        pthread_create(&tids[i], NULL, worker, (void*)(long)i);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags = 0; // Garantisce che fgets fallisca con EINTR al Ctrl+C
    sigaction(SIGINT, &sa, NULL);

    int turn = 0;
    char buf[256];
    int input_val;

    printf("Main pronto. Inserisci interi (Ctrl+C per stampare, Ctrl+D per terminare):\n");

    while (1) {
        if (sigint_flag) {
            print_lists();
            sigint_flag = 0;
        }

        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            if (errno == EINTR) {
                errno = 0;
                continue; // Torna su a controllare sigint_flag
            }
            break; // Ctrl+D (EOF reale)
        }

        if (sscanf(buf, "%d", &input_val) == 1) {
            // <-- MODIFICATO: Il MAIN THREAD genera e popola il record!
            data *new_record = malloc(sizeof(data));
            if (!new_record) exit(EXIT_FAILURE);
            new_record->val = input_val;
            new_record->next = NULL;

            // Passaggio protetto al worker di turno
            shared_node = new_record;
            
            sem_post(&sem_threads[turn]);
            sem_wait(&sem_main); // Aspetta che il thread lo abbia agganciato
            
            turn = (turn + 1) % N;
        }
    }

    // --- FASE DI SHUTDOWN PULITA ---
    shared_node = NULL; // Sentinella di chiusura
    for (int i = 0; i < N; i++) {
        sem_post(&sem_threads[i]);
        pthread_join(tids[i], NULL);
        sem_destroy(&sem_threads[i]);
    }

    // Liberazione finale della memoria allocata
    for (int i = 0; i < N; i++) {
        data *curr = lists[i];
        while (curr) {
            data *tmp = curr->next;
            free(curr);
            curr = tmp;
        }
    }
    
    sem_destroy(&sem_main);
    free(lists); free(sem_threads); free(tids);
    printf("Programma terminato pulitamente.\n");
    return 0;
}