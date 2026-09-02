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
Implementare un programma che riceva in input tramite argv[] i pathname 
associati ad N file, con N maggiore o uguale ad 1. Per ognuno di questi
file generi un processo che legga tutte le stringhe contenute in quel file
e le scriva in un'area di memoria condivisa con il processo padre. Si 
supponga per semplicita' che lo spazio necessario a memorizzare le stringhe
di ognuno di tali file non ecceda 4KB. 
Il processo padre dovra' attendere che tutti i figli abbiano scritto in 
memoria il file a loro associato, e successivamente dovra' entrare in pausa
indefinita.
D'altro canto, ogni figlio dopo aver scritto il contenuto del file nell'area 
di memoria condivisa con il padre entrera' in pausa indefinita.
L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo padre venga colpito da esso dovra' 
stampare a terminale il contenuto corrente di tutte le aree di memoria 
condivisa anche se queste non sono state completamente popolate dai processi 
figli.

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

#define MAX_SIZE 4096
#define PE(fmt, ...) do{fprintf(stderr, fmt " Error: %s\n", ##__VA_ARGS__, strerror(errno)); exit(EXIT_FAILURE);}while(0)

typedef struct {
    volatile size_t bytes_written;
    char data[MAX_SIZE];
} SharedArea;

int N;
SharedArea *shared_mem;

void handler(int sig) {
    (void)sig;
    
    char header[] = "\n--- CONTENUTO AREE DI MEMORIA CONDIVISA ---\n";
    write(STDOUT_FILENO, header, sizeof(header) - 1);
    
    for (int i = 0; i < N; i++) {
        char prefix[] = ">>> File: \n";
        write(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
        
        if (shared_mem[i].bytes_written > 0) {
            write(STDOUT_FILENO, shared_mem[i].data, shared_mem[i].bytes_written);
        }
        write(STDOUT_FILENO, "\n", 1);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Uso: %s <file1> <file2> ... <fileN>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    N = argc - 1;

    shared_mem = mmap(NULL, N * sizeof(SharedArea), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_mem == MAP_FAILED) PE("mmap shared_mem");

    sem_t *shared_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_sem == MAP_FAILED) PE("mmap shared_sem");

    if (sem_init(shared_sem, 1, 0) < 0) PE("sem_init error");

    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            PE("fork error");
        } 
        else if (pid == 0) {
            signal(SIGINT, SIG_IGN);

            FILE *f = fopen(argv[i + 1], "r");
            if (f) {
                char word[256];
                size_t offset = 0;
                
                while (fscanf(f, "%255s", word) == 1) {
                    size_t len = strlen(word);
                    
                    if (offset + len + 1 < MAX_SIZE) {
                        memcpy(shared_mem[i].data + offset, word, len);
                        offset += len;
                        shared_mem[i].data[offset++] = '\n'; 
                        shared_mem[i].bytes_written = offset; 
                    }
                }
                fclose(f);
            } else {
                perror("Errore apertura file nel figlio");
            }

            sem_post(shared_sem);
            while(1) pause(); 
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) < 0) PE("sigaction error");

    for (int i = 0; i < N; i++) {
        sem_wait(shared_sem);
    }

    printf("\n[PADRE] Tutti i figli hanno terminato la scrittura in memoria condivisa.\n");
    printf("[PADRE] In attesa indefinita. Premi CTRL+C per ispezionare le aree...\n");

    while(1) pause();

    return 0;
}