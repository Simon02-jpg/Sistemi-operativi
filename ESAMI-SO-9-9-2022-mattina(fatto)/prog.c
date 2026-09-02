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
Implementare una programma che riceva in input, tramite argv[], i nomi
di N differenti file F1 ed FN, con N maggiore o uguale a 1, che dovranno essere creati.
Per ognuno dei file dovra' essere attivato un nuovo thread che ne gestira' il contenuto 
(indichiamo quindi con T1 ... TN i thread che dovranno essere attivati).
A turno secondo una regola circolare ogni thread Ti dovra' leggere 5 caratteri dallo stream 
di standard input e dovra' scriverli sul file che sta gestendo.
La scrittura dei 5 caratteri su ciascuno dei file deve risultare come una azione atomica,
ovvero i caratteri non possono essere scritti sui file individualmente.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra' 
riportare su standard output i 5 ultimi caratteri correntemente presenti 
su ciascuno degli N file gestiti.

In caso non vi sia immissione di dati sullo standard input, e non vi siano segnalazioni,
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int N;
sem_t *sem;
int *fds;
char **last5;

typedef struct {
    int id;
    char *filename;
} thread_arg;

void handler(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\n----- SIGINT -----\n", 20);
    
    for(int i = 0; i < N; i++) {
        write(STDOUT_FILENO, "File ", 5);
        
        char num[2];
        int n = i + 1;
        if (n > 9) {
            num[0] = (n / 10) + '0';
            num[1] = (n % 10) + '0';
            write(STDOUT_FILENO, num, 2);
        } else {
            num[0] = n + '0';
            write(STDOUT_FILENO, num, 1);
        }
        
        write(STDOUT_FILENO, ": ", 2);
        write(STDOUT_FILENO, last5[i], 5);
        write(STDOUT_FILENO, "\n", 1);
    }
}

void *worker(void *arg) {
    thread_arg *a = (thread_arg *)arg;
    int id = a->id;
    char buffer[5];

    while(1) {
        sem_wait(&sem[id]);
        
        int read_chars = 0;
        while(read_chars < 5) {
            char c;
            int ret = read(STDIN_FILENO, &c, 1);
            
            if(ret == 0) {
                exit(EXIT_SUCCESS);
            }
            if(ret < 0) {
                if(errno == EINTR) continue;
                exit(EXIT_FAILURE);
            }
            
            if(c == '\n') continue;
            
            buffer[read_chars++] = c;
        }

        write(fds[id], buffer, 5);
        memcpy(last5[id], buffer, 5);
        
        sem_post(&sem[(id + 1) % N]);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        exit(EXIT_FAILURE);
    }

    N = argc - 1;

    sem = malloc(sizeof(sem_t) * N);
    fds = malloc(sizeof(int) * N);
    last5 = malloc(sizeof(char *) * N);
    pthread_t *threads = malloc(sizeof(pthread_t) * N);
    thread_arg *args = malloc(sizeof(thread_arg) * N);

    if(!sem || !fds || !last5 || !threads || !args) {
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < N; i++) {
        fds[i] = open(argv[i + 1], O_CREAT | O_WRONLY | O_APPEND, 0666);
        if(fds[i] < 0) exit(EXIT_FAILURE);

        last5[i] = malloc(5);
        memset(last5[i], '-', 5);

        if(i == 0)
            sem_init(&sem[i], 0, 1);
        else
            sem_init(&sem[i], 0, 0);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);

    for(int i = 0; i < N; i++) {
        args[i].id = i;
        args[i].filename = argv[i + 1];
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    while(1) {
        pause();
    }

    return 0;
}