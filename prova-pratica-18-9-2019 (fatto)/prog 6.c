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
Implementare una programma che riceva in input, tramite argv[], un insieme di
stringhe S_1 ..... S_n con n maggiore o uguale ad 1. 
Per ogni stringa S_i dovra' essere attivato un thread T_i.
Il main thread dovra' leggere indefinitamente stringhe dallo standard-input.
Ogni stringa letta dovra' essere resa disponibile al thread T_1 che dovra' 
eliminare dalla stringa ogni carattere presente in S_1, sostituendolo con il 
carattere 'spazio'.
Successivamente T_1 rendera' la stringa modificata disponibile a T_2 che dovra' 
eseguire la stessa operazione considerando i caratteri in S_2, e poi la passera' 
a T_3 (che fara' la stessa operazione considerando i caratteri in S_3) e cosi' 
via fino a T_n. 
T_n, una volta completata la sua operazione sulla stringa ricevuta da T_n-1, dovra'
passare la stringa ad un ulteriore thread che chiameremo OUTPUT il quale dovra' 
stampare la stringa ricevuta su un file di output dal nome output.txt.
Si noti che i thread lavorano secondo uno schema pipeline, sono ammesse quindi 
operazioni concorrenti su differenti stringhe lette dal main thread dallo 
standard-input.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra' 
stampare il contenuto corrente del file output.txt su standard-output.

In caso non vi sia immissione di dati sullo standard-input, l'applicazione 
dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

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
char **strings;
sem_t *sem_ready;
sem_t *sem_empty;
char **mbox;
FILE *output_file;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void print_output() {
    int fd = open("output.txt", O_RDONLY);
    if (fd >= 0) {
        char buffer[4096];
        ssize_t bytes;
        write(STDOUT_FILENO, "\n", 1);
        while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, bytes);
        }
        close(fd);
    }
}

void *filter_thread(void *arg) {
    long id = (long)arg;
    char *my_str = strings[id];

    while (1) {
        sem_wait(&sem_ready[id]);
        char *buf = mbox[id];
        sem_post(&sem_empty[id]);

        for (int j = 0; buf[j] != '\0'; j++) {
            for (int k = 0; my_str[k] != '\0'; k++) {
                if (buf[j] == my_str[k]) {
                    buf[j] = ' ';
                    break;
                }
            }
        }

        sem_wait(&sem_empty[id + 1]);
        mbox[id + 1] = buf;
        sem_post(&sem_ready[id + 1]);
    }
    return NULL;
}

void *output_thread(void *arg) {
    (void)arg;
    while (1) {
        sem_wait(&sem_ready[N]);
        char *buf = mbox[N];
        sem_post(&sem_empty[N]);

        fprintf(output_file, "%s\n", buf);
        fflush(output_file);
        free(buf);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    N = argc - 1;
    strings = argv + 1;

    output_file = fopen("output.txt", "w+");
    if (!output_file) {
        exit(EXIT_FAILURE);
    }

    sem_ready = malloc((N + 1) * sizeof(sem_t));
    sem_empty = malloc((N + 1) * sizeof(sem_t));
    mbox = malloc((N + 1) * sizeof(char *));

    if (!sem_ready || !sem_empty || !mbox) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i <= N; i++) {
        if (sem_init(&sem_ready[i], 0, 0) < 0) exit(EXIT_FAILURE);
        if (sem_init(&sem_empty[i], 0, 1) < 0) exit(EXIT_FAILURE);
    }

    for (long i = 0; i < N; i++) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, filter_thread, (void *)i) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    pthread_t tid_out;
    if (pthread_create(&tid_out, NULL, output_thread, NULL) != 0) {
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (sigint_flag) {
            print_output();
            sigint_flag = 0;
        }

        char *buf = malloc(4096);
        if (!buf) exit(EXIT_FAILURE);

        if (fgets(buf, 4096, stdin) == NULL) {
            free(buf);
            if (errno == EINTR) continue;
            break;
        }

        buf[strcspn(buf, "\n")] = '\0';

        sem_wait(&sem_empty[0]);
        mbox[0] = buf;
        sem_post(&sem_ready[0]);
    }

    return 0;
}
