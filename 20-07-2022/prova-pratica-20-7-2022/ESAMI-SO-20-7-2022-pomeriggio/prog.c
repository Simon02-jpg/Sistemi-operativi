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


SPECIFICATION TO BE IMPLEMENTED: (20-07-2022)
Scrivere un programma che riceva in input tramite argv[] i nomi di N file,
con N magiore o uguale a 1. L'applicazione dovrà controllare che i nomi dei 
file siano diversi tra di loro.
I file dovranno essere creati oppure troncati se esistenti.
Per ogniuno dei file dovra' essere attivato un nuovo thread, che indicheremo
con Ti, che gestirà il contenuto del file. 
I thread Ti leggeranno linee di caratteri da standard input a turno secondo 
uno schema circolare, e scriveranno la linea letta all'interno del file 
da loro gestito.

L'applicazione dovrà essere in grado di gestire il segnale SIGINT 
(o CTRL_C_EVENT nel caso WinAPI) in modo tale che quando il processo  
verrà colpito riporti su standard output il contenuto corrente (ovvero le linee 
attualmente presenti) di tutti i file che erano stati specificati in argv[], 
seguendo esattamente l'ordine tramite cui le linee sono state inserite al loro interno.
In ogni caso, nessuno dei thread dovrà terminare la sua esecuzione in caso di arrivo
della segnalazione.

In caso non vi sia immissione di dati sullo standard input e non vi siano segnalazioni, 
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STR 4096

volatile sig_atomic_t signal_received = 0;
volatile sig_atomic_t eof_reached = 0;

typedef struct {
    int thread_id;
    char filename[256];
} thread_data_t;

int num_threads_global;
sem_t *turn_sems;

void sigint_handler(int sig) {
    signal_received = 1;
}

void* worker_routine(void* arg) {
    thread_data_t *my_data = (thread_data_t*) arg;
    int id = my_data->thread_id;
    int next_id = (id + 1) % num_threads_global;

    FILE *fd = fopen(my_data->filename, "w");
    if (!fd) {
        pthread_exit(NULL);
    }

    char buffer[MAX_STR];

    while (1) {
        if (sem_wait(&turn_sems[id]) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (eof_reached) {
            sem_post(&turn_sems[next_id]);
            break;
        }

        clearerr(stdin);
        if (fgets(buffer, MAX_STR, stdin) == NULL) {
            if (errno == EINTR) {
                errno = 0;
                sem_post(&turn_sems[id]); 
                continue;
            }
            eof_reached = 1;
            sem_post(&turn_sems[next_id]);
            break;
        }

        fprintf(fd, "%s", buffer);
        fflush(fd);

        sem_post(&turn_sems[next_id]);
    }

    fclose(fd);
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    int N = argc - 1;

    for (int i = 1; i < argc; i++) {
        for (int j = i + 1; j < argc; j++) {
            if (strcmp(argv[i], argv[j]) == 0) {
                exit(EXIT_FAILURE);
            }
        }
    }

    num_threads_global = N;
    
    pthread_t *threads = malloc(N * sizeof(pthread_t));
    thread_data_t *t_args = malloc(N * sizeof(thread_data_t));
    turn_sems = malloc(N * sizeof(sem_t));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    for (int i = 0; i < N; i++) {
        t_args[i].thread_id = i;
        strcpy(t_args[i].filename, argv[i+1]);
        
        if (i == 0) {
            sem_init(&turn_sems[i], 0, 1);
        } else {
            sem_init(&turn_sems[i], 0, 0);
        }

        if (pthread_create(&threads[i], NULL, worker_routine, &t_args[i]) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    while (!eof_reached) {
        sleep(1);

        if (signal_received) {
            signal_received = 0;
            
            write(STDOUT_FILENO, "\n--- CONTENUTO FILE ---\n", 24);
            
            FILE **fds = malloc(N * sizeof(FILE*));
            for (int i = 0; i < N; i++) {
                fds[i] = fopen(argv[i+1], "r");
            }
            
            int read_something = 1;
            char buf[MAX_STR];
            
            while (read_something) {
                read_something = 0;
                for (int i = 0; i < N; i++) {
                    if (fds[i] && fgets(buf, MAX_STR, fds[i]) != NULL) {
                        write(STDOUT_FILENO, buf, strlen(buf));
                        read_something = 1;
                    }
                }
            }
            
            for (int i = 0; i < N; i++) {
                if (fds[i]) fclose(fds[i]);
            }
            free(fds);
            write(STDOUT_FILENO, "----------------------\n", 23);
        }
    }

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
        sem_destroy(&turn_sems[i]);
    }

    free(threads);
    free(t_args);
    free(turn_sems);
    return 0;
}