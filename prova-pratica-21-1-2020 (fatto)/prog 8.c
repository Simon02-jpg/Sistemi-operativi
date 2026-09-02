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
Implementare una programma che ricevento in input tramite argv[] una stringa S
esegua le seguenti attivita'.
Il main thread dovra' attivare due nuovi thread, che indichiamo con T1 e T2.
Successivamente il main thread dovra' leggere indefinitamente caratteri dallo 
standard input, a blocchi di 5 per volta, e dovra' rendere disponibili i byte 
letti a T1 e T2. 
Il thread T1 dovra' inserire di volta in volta i byte ricevuti dal main thread 
in coda ad un file di nome S_diretto, che dovra' essere creato. 
Il thread T2 dovra' inserirli invece nel file S_inverso, che dovra' anche esso 
essere creato, scrivendoli ogni volta come byte iniziali del file (ovvero in testa al 
file secondo uno schema a pila).

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra' 
calcolare il numero dei byte che nei due file hanno la stessa posizione ma sono
tra loro diversi in termini di valore. Questa attivita' dovra' essere svolta attivando 
per ogni ricezione di segnale un apposito thread.

In caso non vi sia immissione di dati sullo standard input, l'applicazione dovra' 
utilizzare non piu' del 5% della capacita' di lavoro della CPU.

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
char f1_name[512];
char f2_name[512];
char shared_buf[5];
int bytes_to_write = 0;

sem_t sem_t1_ready;
sem_t sem_t2_ready;
sem_t sem_main_wait1;
sem_t sem_main_wait2;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void *checker_thread(void *arg) {
    (void)arg;
    int fd1 = open(f1_name, O_RDONLY);
    int fd2 = open(f2_name, O_RDONLY);

    if (fd1 < 0 || fd2 < 0) {
        if (fd1 >= 0) close(fd1);
        if (fd2 >= 0) close(fd2);
        return NULL;
    }

    char c1, c2;
    int diff_count = 0;

    while (read(fd1, &c1, 1) == 1 && read(fd2, &c2, 1) == 1) {
        if (c1 != c2) {
            diff_count++;
        }
    }

    printf("\nByte differenti: %d\n", diff_count);

    close(fd1);
    close(fd2);
    return NULL;
}

void *t1_worker(void *arg) {
    (void)arg;
    int fd = open(f1_name, O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (fd < 0) exit(EXIT_FAILURE);

    while (1) {
        sem_wait(&sem_t1_ready);
        if (bytes_to_write == 0) break;
        
        if (write(fd, shared_buf, bytes_to_write) < 0) {
            exit(EXIT_FAILURE);
        }
        
        sem_post(&sem_main_wait1);
    }

    close(fd);
    return NULL;
}

void *t2_worker(void *arg) {
    (void)arg;
    int fd = open(f2_name, O_CREAT | O_RDWR, 0666);
    if (fd < 0) exit(EXIT_FAILURE);

    int current_size = 0;

    while (1) {
        sem_wait(&sem_t2_ready);
        if (bytes_to_write == 0) break;

        char *temp = NULL;
        if (current_size > 0) {
            temp = malloc(current_size);
            if (!temp) exit(EXIT_FAILURE);
            lseek(fd, 0, SEEK_SET);
            if (read(fd, temp, current_size) < 0) {
                free(temp);
                exit(EXIT_FAILURE);
            }
        }

        lseek(fd, 0, SEEK_SET);
        if (write(fd, shared_buf, bytes_to_write) < 0) {
            if (temp) free(temp);
            exit(EXIT_FAILURE);
        }

        if (current_size > 0) {
            if (write(fd, temp, current_size) < 0) {
                free(temp);
                exit(EXIT_FAILURE);
            }
            free(temp);
        }

        current_size += bytes_to_write;
        sem_post(&sem_main_wait2);
    }

    close(fd);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    snprintf(f1_name, sizeof(f1_name), "%s_diretto", argv[1]);
    snprintf(f2_name, sizeof(f2_name), "%s_inverso", argv[1]);

    int fd1 = open(f1_name, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd1 >= 0) close(fd1);
    int fd2 = open(f2_name, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd2 >= 0) close(fd2);

    if (sem_init(&sem_t1_ready, 0, 0) < 0) exit(EXIT_FAILURE);
    if (sem_init(&sem_t2_ready, 0, 0) < 0) exit(EXIT_FAILURE);
    if (sem_init(&sem_main_wait1, 0, 0) < 0) exit(EXIT_FAILURE);
    if (sem_init(&sem_main_wait2, 0, 0) < 0) exit(EXIT_FAILURE);

    pthread_t t1, t2;
    if (pthread_create(&t1, NULL, t1_worker, NULL) != 0) exit(EXIT_FAILURE);
    if (pthread_create(&t2, NULL, t2_worker, NULL) != 0) exit(EXIT_FAILURE);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) exit(EXIT_FAILURE);

    while (1) {
        if (sigint_flag) {
            sigint_flag = 0;
            pthread_t checker;
            if (pthread_create(&checker, NULL, checker_thread, NULL) == 0) {
                pthread_detach(checker);
            }
        }

        int read_total = 0;
        while (read_total < 5) {
            ssize_t res = read(STDIN_FILENO, shared_buf + read_total, 5 - read_total);
            if (res < 0) {
                if (errno == EINTR) {
                    if (sigint_flag) {
                        sigint_flag = 0;
                        pthread_t checker;
                        if (pthread_create(&checker, NULL, checker_thread, NULL) == 0) {
                            pthread_detach(checker);
                        }
                    }
                    continue;
                }
                exit(EXIT_FAILURE);
            }
            if (res == 0) break;
            read_total += res;
        }

        if (read_total == 0) {
            bytes_to_write = 0;
            sem_post(&sem_t1_ready);
            sem_post(&sem_t2_ready);
            break;
        }

        bytes_to_write = read_total;
        sem_post(&sem_t1_ready);
        sem_post(&sem_t2_ready);

        sem_wait(&sem_main_wait1);
        sem_wait(&sem_main_wait2);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}