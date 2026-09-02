/******************************************************************
Welcome to the Operating System examination

You are editing the '/home/esame/prog.c' file. You cannot remove 
this file, just edit it so as to produce your own program according to
the specification listed below.

In the '/home/esame/'directory you can find a Makefile that you can 
use to compile this prpogram to generate an executable named 'prog' 
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
Implementare una programma che riceva in input, tramite argv[], il nome
di un file F ed N stringhe S_1 .. S_N (con N maggiore o uguale
ad 1.
Per ogni stringa S_i dovra' essere attivato un nuovo thread T_i, che fungera'
da gestore della stringa S_i.
Il main thread dovra' leggere indefinitamente stringhe dallo standard-input. 
Ogni nuova stringa letta dovra' essere comunicata a tutti i thread T_1 .. T_N
tramite un buffer condiviso, e ciascun thread T_i dovra' verificare se tale
stringa sia uguale alla stringa S_i da lui gestita. In caso positivo, ogni
carattere della stringa immessa dovra' essere sostituito dal carattere '*'.
Dopo che i thread T_1 .. T_N hanno analizzato la stringa, ed eventualmente questa
sia stata modificata, il main thread dovra' scrivere tale stringa (modificata o non)
su una nuova linea del file F. 
In altre parole, la sequenza di stringhe provenienti dallo standard-input dovra' 
essere riportata su file F in una forma 'epurata'  delle stringhe S1 .. SN, 
che verranno sostituite da strighe  della stessa lunghezza costituite esclusivamente
da sequenze del carattere '*'.
Inoltre, qualora gia' esistente, il file F dovra' essere troncato (o rigenerato) 
all'atto del lancio dell'applicazione.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra' 
riversare su standard-output il contenuto corrente del file F.

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
char buffer[4096];
int N;
char **strings;
char *filename;

sem_t *sem_start;
sem_t *sem_done;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void print_file() {
    int fd = open(filename, O_RDONLY);
    if (fd >= 0) {
        char buf[4096];
        ssize_t bytes;
        write(STDOUT_FILENO, "\n", 1);
        while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, bytes);
        }
        close(fd);
    }
}

void* worker(void* arg) {
    long id = (long)arg;
    char *my_string = strings[id];

    while (1) {
        sem_wait(&sem_start[id]);

        pthread_mutex_lock(&buffer_mutex);
        if (strcmp(buffer, my_string) == 0) {
            memset(buffer, '*', strlen(buffer));
        }
        pthread_mutex_unlock(&buffer_mutex);

        sem_post(&sem_done[id]);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        exit(EXIT_FAILURE);
    }

    filename = argv[1];
    N = argc - 2;
    strings = argv + 2;

    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        exit(EXIT_FAILURE);
    }

    sem_start = malloc(N * sizeof(sem_t));
    sem_done = malloc(N * sizeof(sem_t));
    if (!sem_start || !sem_done) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; i++) {
        if (sem_init(&sem_start[i], 0, 0) < 0) exit(EXIT_FAILURE);
        if (sem_init(&sem_done[i], 0, 0) < 0) exit(EXIT_FAILURE);

        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, (void*)(long)i) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (sigint_flag) {
            print_file();
            sigint_flag = 0;
        }

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            if (errno == EINTR) continue;
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        for (int i = 0; i < N; i++) {
            sem_post(&sem_start[i]);
        }

        for (int i = 0; i < N; i++) {
            sem_wait(&sem_done[i]);
        }

        write(fd, buffer, strlen(buffer));
        write(fd, "\n", 1);
    }

    close(fd);
    return 0;
}