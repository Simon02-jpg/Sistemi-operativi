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
for the evaluation of your exam. Moifications made to prog.c which you
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
Implementare un programma che riceva in input tramite argv[] i pathname 
associati ad N file, con N maggiore o uguale ad 1. Per ognuno di questi
file generi un thread (quindi in totale saranno generati N nuovi thread 
concorrenti). 
Successivamente il main-thread acquisira' stringhe da standard input in 
un ciclo indefinito, ed ognuno degli N thread figli dovra' scrivere ogni
stringa acquisita dal main-thread nel file ad esso associato.
L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso 
WinAPI) in modo tale che quando uno qualsiasi dei thread dell'applicazione
venga colpito da esso dovra' stampare a terminale tutte le stringhe gia' 
immesse da standard-input e memorizzate nei file destinazione.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t sigint_flag = 0;
int N;
char **files;
sem_t *sem_ready;
sem_t *sem_done;
char buffer[4096];

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void print_strings() {
    int fd = open(files[0], O_RDONLY);
    if (fd >= 0) {
        char buf[4096];
        ssize_t n;
        write(STDOUT_FILENO, "\n", 1);
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        close(fd);
    }
}

void* worker(void* arg) {
    long id = (long)arg;
    int fd = open(files[id], O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0666);
    if (fd < 0) exit(EXIT_FAILURE);

    while (1) {
        sem_wait(&sem_ready[id]);
        if (write(fd, buffer, strlen(buffer)) < 0) {
            exit(EXIT_FAILURE);
        }
        sem_post(&sem_done[id]);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) exit(EXIT_FAILURE);

    N = argc - 1;
    files = argv + 1;

    sem_ready = malloc(N * sizeof(sem_t));
    sem_done = malloc(N * sizeof(sem_t));
    if (!sem_ready || !sem_done) exit(EXIT_FAILURE);

    for (int i = 0; i < N; i++) {
        if (sem_init(&sem_ready[i], 0, 0) < 0) exit(EXIT_FAILURE);
        if (sem_init(&sem_done[i], 0, 1) < 0) exit(EXIT_FAILURE);
    }

    for (long i = 0; i < N; i++) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, (void*)i) != 0) exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) exit(EXIT_FAILURE);

    int tokens_held = 0;

    while (1) {
        if (!tokens_held) {
            for (int i = 0; i < N; i++) {
                sem_wait(&sem_done[i]);
            }
            tokens_held = 1;
        }

        if (sigint_flag) {
            print_strings();
            sigint_flag = 0;
        }

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < N; i++) {
            sem_post(&sem_ready[i]);
        }
        tokens_held = 0;
    }

    return 0;
}
