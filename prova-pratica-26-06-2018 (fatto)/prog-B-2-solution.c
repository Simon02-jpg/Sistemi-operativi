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
You can also modify the Makefile if requesed, since this file will also
be automatically stored together with your program and will be part
of the final data to be evaluated for your exam.

PLEASE BE CAREFUL THAT THE LAST SAVED VERSION OF THE prog.c FILE (and of
the Makfile) WILL BE AUTOMATICALLY STORED WHEN YOU CLOSE YOUR EXAMINATION 
VIA THE CLOSURE CODE YOU RECEIVED, OR WHEN THE TIME YOU HAVE BEEN GRANTED
TO DEVELOP YOUR PROGRAM EXPIRES. 


SPECIFICATION TO BE IMPLEMENTED:
Implementare un programma che riceva in input tramite argv[2] un numero
intero N maggiore o uguale ad 1 (espresso come una stringa di cifre 
decimali), e generi N nuovi processi. Ciascuno di questi leggera' in modo 
continuativo un valore intero da standard input, e lo comunichera' al
processo padre tramite memoria condivisa. Il processo padre scrivera' ogni
nuovo valore intero ricevuto su di un file, come sequenza di cifre decimali. 
I valori scritti su file devono essere separati dal carattere ' ' (blank).
Il pathname del file di output deve essere comunicato all'applicazione 
tramite argv[1].
L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che se il processo padre venga colpito il contenuto
del file di output venga interamente riversato su standard-output.
Nel caso in cui non vi sia immissione in input, l'applicazione non deve 
consumare piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t sigint_flag = 0;

typedef struct {
    sem_t sem_parent;
    sem_t sem_children;
    int value;
} SharedData;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void print_file(int fd) {
    off_t current_pos = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);
    
    char buf[4096];
    ssize_t n;
    write(STDOUT_FILENO, "\n", 1);
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, n);
    }
    write(STDOUT_FILENO, "\n", 1);
    
    lseek(fd, current_pos, SEEK_SET);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];
    int num_proc = atoi(argv[2]);

    if (num_proc < 1) {
        exit(EXIT_FAILURE);
    }

    int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        exit(EXIT_FAILURE);
    }

    SharedData *shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        exit(EXIT_FAILURE);
    }

    if (sem_init(&shared->sem_parent, 1, 0) < 0) exit(EXIT_FAILURE);
    if (sem_init(&shared->sem_children, 1, 1) < 0) exit(EXIT_FAILURE);

    for (int i = 0; i < num_proc; i++) {
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        
        if (pid == 0) {
            signal(SIGINT, SIG_IGN);
            while (1) {
                sem_wait(&shared->sem_children);
                
                int val;
                if (scanf("%d", &val) != 1) {
                    sem_post(&shared->sem_children);
                    exit(EXIT_SUCCESS);
                }
                
                shared->value = val;
                sem_post(&shared->sem_parent);
            }
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) exit(EXIT_FAILURE);

    while (1) {
        if (sigint_flag) {
            print_file(fd);
            sigint_flag = 0;
        }

        if (sem_wait(&shared->sem_parent) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d ", shared->value);
        if (write(fd, buf, len) < 0) {
            exit(EXIT_FAILURE);
        }

        sem_post(&shared->sem_children);
    }

    return 0;
}