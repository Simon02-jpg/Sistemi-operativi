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
Posix or WinAPI compilation) are already included in the head of the
prog.c file you are editing. 

At the end of the examination, the last saved snapshot of this file
will be automatically stored by the system and will be then considered
for the evaluation of your exam. Modifications made to prog.c which are
not saved by you via the editor will not appear in the stored version
of the prog.c file. 
In other words, unsaved changes will not be tracked, so please save 
this file when you think you have finished software development.
You can also modify the Makefile if requesed, since this file will also
be automatically stored together with your program and will be part
of the final data to be evaluated for your exam.

PLEASE BE CAREFUL THAT THE LAST SAVED VERSION OF THE prog.c FILE (and of
the Makfile) WILL BE AUTOMATICALLY STORED WHEN YOU CLOSE YOUR EXAMINATION 
VIA THE CLOSURE CODE YOU RECEIVED, OR WHEN THE TIME YOU HAVE BEEN GRANTED
TO DEVELOP YOUR PROGRAM EXPIRES. 


SPECIFICATION TO BE IMPLEMENTED:
Implementare una programma che riceva in input, tramite argv[], 
N differenti stringhe S1 ... SN, con N maggiore o uguale a 1.
Per ognuna delle stringhe dovra' essere attivato un nuovo thread per gestirla
(indichiamo quindi con T1 ... TN i thread che dovranno essere attivati).
Il main thread  dovra' leggere stringhe dallo standard input, e dovra'
rendere disponibile ogni stringa letta a T1. T1 dovra' verificare se la stringa ricevuta
e' uguale alla stringa S1 da lui gestita, e dovra' incrementare un contatore
in caso positivo. Altrimenti, in caso negativo, dovra' rendere la stringa ricevuta dal 
main disponibile al thread T2 che fara' lo stesso controllo, e cosi' via fino a TN.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando sia colpita  
essa dovra' riportare su standard output il valore dei contatori che indicano
quante volte le stringhe S1 ... SN sono state trovate uguali alla stringhe che 
il main thread aveva letto da standard input.

In caso non vi sia immissione di dati sullo standard input, e non vi siano segnalazioni,
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX 4096
#define PE(fmt, ...) do{fprintf(stderr, fmt " Error: %s\n", ##__VA_ARGS__, strerror(errno)); exit(EXIT_FAILURE);}while(0)

volatile sig_atomic_t sigint_flag = 0;
int N;
char **strings;
int *counters;

sem_t *sem_ready;
sem_t *sem_copied;
char **buffers;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void* thread(void* arg)
{
    int id = (int)(intptr_t)arg;
    char local_buf[MAX];

    while(1)
    {
        sem_wait(&sem_ready[id]);
        
        strcpy(local_buf, buffers[id]);
        
        sem_post(&sem_copied[id]);

        if(strcmp(local_buf, strings[id]) == 0)
        {
            counters[id]++;
        }
        else if (id < N - 1) 
        {
            strcpy(buffers[id + 1], local_buf);
            sem_post(&sem_ready[id + 1]);
            sem_wait(&sem_copied[id + 1]);
        }
    }
    return NULL;
}

int main(int argc, char** argv)
{
    if(argc < 2) {
        printf("Uso: %s str1 str2 ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    N = argc - 1;
    strings = &argv[1];

    counters = calloc(N, sizeof(int));
    sem_ready = malloc(N * sizeof(sem_t));
    sem_copied = malloc(N * sizeof(sem_t));
    buffers = malloc(N * sizeof(char*));
    pthread_t *threads = malloc(N * sizeof(pthread_t));

    for(int i = 0; i < N; i++) {
        buffers[i] = malloc(MAX);
        sem_init(&sem_ready[i], 0, 0); 
        sem_init(&sem_copied[i], 0, 0);

        if(pthread_create(&threads[i], NULL, thread, (void*)(intptr_t)i) != 0) {
            PE("pthread_create");
        }
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = handler;
    if(sigaction(SIGINT, &act, NULL) < 0) {
        PE("sigaction");
    }

    char main_buf[MAX];
    printf("Inserisci le stringhe (CTRL+C per stampare i contatori, CTRL+D per uscire):\n");

    while(1)
    {
        if (sigint_flag) {
            printf("\n--- RISULTATI ---\n");
            for(int i = 0; i < N; i++) {
                printf("Stringa '%s' trovata %d volte.\n", strings[i], counters[i]);
            }
            printf("-----------------\n");
            sigint_flag = 0;
        }

        if (fgets(main_buf, sizeof(main_buf), stdin) == NULL) {
            if (sigint_flag) continue;
            
            if (feof(stdin)) {
                printf("\nUscita pulita.\n");
                break;
            }
            continue;
        }

        main_buf[strcspn(main_buf, "\n")] = '\0';

        strcpy(buffers[0], main_buf);
        sem_post(&sem_ready[0]);
        sem_wait(&sem_copied[0]);
    }

    return 0;
}