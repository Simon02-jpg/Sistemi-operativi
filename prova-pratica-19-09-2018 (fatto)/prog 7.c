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
for the evaluation of your exam. Moifications made to prog.c which are
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
Implementare un programma che riceva in input tramite argv[] i pathname
associati ad N file (F1 ... FN), con N maggiore o uguale ad 1.
Per ognuno di questi file generi un thread che gestira' il contenuto del file.
Dopo aver creato gli N file ed i rispettivi N thread, il main thread dovra'
leggere indefinitamente la sequenza di byte provenienti dallo standard-input.
Ogni 5 nuovi byte letti, questi dovranno essere scritti da uno degli N thread
nel rispettivo file. La consegna dei 5 byte da parte del main thread
dovra' avvenire secondo uno schema round-robin, per cui i primi 5 byte
dovranno essere consegnati al thread in carico di gestire F1, i secondi 5
byte al thread in carico di gestire il F2 e cosi' via secondo uno schema
circolare.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra',
a partire dai dati correntemente memorizzati nei file F1 ... FN, ripresentare
sullo standard-output la medesima sequenza di byte di input originariamente
letta dal main thread dallo standard-input.

Qualora non vi sia immissione di input, l'applicazione dovra' utilizzare
non piu' del 5% della capacita' di lavoro della CPU.

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
char **filenames;
int *fds;
sem_t *sem_main;
sem_t *sem_threads;
char **buffers;
int *bytes_to_write;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void reconstruct() {
    int *read_fds = malloc(N * sizeof(int));
    if (!read_fds) return;

    for (int i = 0; i < N; i++) {
        read_fds[i] = open(filenames[i], O_RDONLY);
        if (read_fds[i] < 0) {
            for (int j = 0; j < i; j++) close(read_fds[j]);
            free(read_fds);
            return;
        }
    }

    write(STDOUT_FILENO, "\n", 1);

    int t = 0;
    char buf[5];
    while (1) {
        ssize_t bytes = read(read_fds[t], buf, 5);
        if (bytes <= 0) break;
        write(STDOUT_FILENO, buf, bytes);
        if (bytes < 5) break;
        t = (t + 1) % N;
    }

    write(STDOUT_FILENO, "\n", 1);

    for (int i = 0; i < N; i++) {
        close(read_fds[i]);
    }
    free(read_fds);
}

void* worker(void* arg) {
    long id = (long)arg;
    while (1) {
        sem_wait(&sem_threads[id]);
        if (bytes_to_write[id] > 0) {
            if (write(fds[id], buffers[id], bytes_to_write[id]) < 0) {
                exit(EXIT_FAILURE);
            }
        }
        sem_post(&sem_main[id]);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) exit(EXIT_FAILURE);

    N = argc - 1;
    filenames = argv + 1;

    fds = malloc(N * sizeof(int));
    sem_main = malloc(N * sizeof(sem_t));
    sem_threads = malloc(N * sizeof(sem_t));
    buffers = malloc(N * sizeof(char*));
    bytes_to_write = malloc(N * sizeof(int));

    if (!fds || !sem_main || !sem_threads || !buffers || !bytes_to_write) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; i++) {
        fds[i] = open(filenames[i], O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0666);
        if (fds[i] < 0) exit(EXIT_FAILURE);
        
        buffers[i] = malloc(5);
        if (!buffers[i]) exit(EXIT_FAILURE);

        if (sem_init(&sem_main[i], 0, 1) < 0) exit(EXIT_FAILURE);
        if (sem_init(&sem_threads[i], 0, 0) < 0) exit(EXIT_FAILURE);

        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, (void*)(long)i) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) exit(EXIT_FAILURE);

    int turn = 0;

    while (1) {
        sem_wait(&sem_main[turn]);

        if (sigint_flag) {
            reconstruct();
            sigint_flag = 0;
        }

        int read_total = 0;
        while (read_total < 5) {
            ssize_t res = read(STDIN_FILENO, buffers[turn] + read_total, 5 - read_total);
            if (res < 0) {
                if (errno == EINTR) {
                    if (sigint_flag) {
                        reconstruct();
                        sigint_flag = 0;
                    }
                    continue;
                }
                exit(EXIT_FAILURE);
            }
            if (res == 0) break;
            read_total += res;
        }

        if (read_total == 0) {
            sem_post(&sem_main[turn]);
            break;
        }

        bytes_to_write[turn] = read_total;
        sem_post(&sem_threads[turn]);
        turn = (turn + 1) % N;
    }

    return 0;
}