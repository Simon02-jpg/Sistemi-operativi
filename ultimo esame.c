/*
SPECIFICATION TO BE IMPLEMENTED:

VERSIONE ITALIANA
Scrivere un programma che riceva in input tramite argv[] il nome di un
file F e N (con N >= 1) stringhe S1, ... SN, rappresentanti numeri interi
positivi.
Il file F dovra' essere creato (o troncato se esistente) al lancio
dell'applicazione. Per ognuna delle N stringhe Si, dovra' essere attivato
un thread che indicheremo come Ti.

Ogni thread Ti dovra' indefinitamente leggere da standard input esattamente Si
byte ad ogni sua iterazione di lettura, e dovra' scrivere gli Si byte
letti su un file dal nome "output_i" (in cui l'indice 'i' viene usato come
suffisso). Tale file dovra' essere appositamente creato (o troncato se
esistente) proprio dal thread Ti al suo startup.

Allo stesso tempo, i thread Ti dovranno leggere da standard input (ciascuno i
propri Si byte) seguendo una regola circolare (quindi leggera' prima T1 i suoi
S1 byte, poi T2 i suoi S2 byte e cosi' via fino a ritornare a T1).

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso WinAPI)
in modo tale che quando il processo venga colpito il suo main-thread dovra'
accedere ai vari file "output_i" e dovra', utilizzando i dati presenti in tali
file, ripresentare l'originale flusso ricevuto da standard input sullo standard
output.

In caso non vi sia immissione di dati sullo standard input e non vi siano
segnalazioni, l'applicazione dovra' utilizzare non piu' del 10% della capacita'
di lavoro della CPU.

*/

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

volatile sig_atomic_t sigint_flag = 0;
int N;
int *S;
sem_t *turn_sems;

void handler(int sig) {
    sigint_flag = 1;
}

typedef struct {
    int id;
} thread_args_t;

void* thread_routine(void* arg) {
    thread_args_t *t_args = (thread_args_t*)arg;
    int id = t_args->id;
    int next_id = (id + 1) % N;
    
    char filename[64];
    sprintf(filename, "output_%d", id);
    
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        pthread_exit(NULL);
    }

    char *buf = malloc(S[id]);

    while (1) {
        sem_wait(&turn_sems[id]);
        
        ssize_t total_read = 0;
        while (total_read < S[id]) {
            ssize_t r = read(STDIN_FILENO, buf + total_read, S[id] - total_read);
            if (r == 0) {
                break;
            }
            if (r < 0) {
                if (errno == EINTR) continue;
                break;
            }
            total_read += r;
        }

        if (total_read > 0) {
            write(fd, buf, total_read);
        }

        sem_post(&turn_sems[next_id]);
        
        if (total_read < S[id]) {
            break; 
        }
    }

    close(fd);
    free(buf);
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        exit(EXIT_FAILURE);
    }

    int fd_F = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_F >= 0) {
        close(fd_F);
    }

    N = argc - 2;
    S = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
        S[i] = atoi(argv[i + 2]);
    }

    turn_sems = malloc(N * sizeof(sem_t));
    pthread_t *threads = malloc(N * sizeof(pthread_t));
    thread_args_t *args = malloc(N * sizeof(thread_args_t));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);

    for (int i = 0; i < N; i++) {
        args[i].id = i;
        sem_init(&turn_sems[i], 0, (i == 0) ? 1 : 0);
        pthread_create(&threads[i], NULL, thread_routine, &args[i]);
    }

    while (1) {
        pause(); 
        
        if (sigint_flag) {
            sigint_flag = 0;
            
            write(STDOUT_FILENO, "\n--- FLUSSO ORIGINALE ---\n", 26);
            
            int *read_fds = malloc(N * sizeof(int));
            for(int i = 0; i < N; i++) {
                char name[64];
                sprintf(name, "output_%d", i);
                read_fds[i] = open(name, O_RDONLY);
            }

            int active = 1;
            while (active) {
                active = 0;
                for (int i = 0; i < N; i++) {
                    if (read_fds[i] >= 0) {
                        char *buf = malloc(S[i]);
                        ssize_t r = read(read_fds[i], buf, S[i]);
                        if (r > 0) {
                            write(STDOUT_FILENO, buf, r);
                            active = 1;
                        }
                        free(buf);
                    }
                }
            }
            
            for(int i = 0; i < N; i++) {
                if(read_fds[i] >= 0) close(read_fds[i]);
            }
            free(read_fds);
            
            write(STDOUT_FILENO, "\n------------------------\n", 26);
        }
    }

    return 0;
}