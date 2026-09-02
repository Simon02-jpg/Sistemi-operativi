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
di un file F. Il programa dovra' creare il file F e popolare il file
con lo stream priveniente da standard-input. Il programma dovra' generare
anche un ulteriore processo il quale dovra' riversare il contenuto  che 
viene inserito in F su un altro file denominato shadow_F, tale inserimento
dovra' essere realizzato in modo concorrente rispetto all'inserimento dei dati su F.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando un qualsiasi processo (parent o child) venga colpito si
dovra' immediatamente emettere su standard-output il contenuto del file 
che il processo child sta popolando. 

Qualora non vi sia immissione di input, l'applicazione dovra' utilizzare 
non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t sigint_flag = 0;
char shadow_name[4096];

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void print_shadow() {
    int fd = open(shadow_name, O_RDONLY);
    if (fd >= 0) {
        char buf[4096];
        ssize_t n;
        write(STDOUT_FILENO, "\n--- CONTENUTO FILE SHADOW ---\n", 31);
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        write(STDOUT_FILENO, "-----------------------------\n", 30);
        close(fd);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    snprintf(shadow_name, sizeof(shadow_name), "shadow_%s", argv[1]);

    int sem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem < 0) {
        exit(EXIT_FAILURE);
    }
    
    if (semctl(sem, 0, SETVAL, 0) < 0) {
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        FILE *f_in = fopen(argv[1], "r");
        FILE *f_out = fopen(shadow_name, "w+");
        
        if (!f_in || !f_out) {
            exit(EXIT_FAILURE);
        }

        while (1) {
            if (sigint_flag) {
                print_shadow();
                sigint_flag = 0;
            }

            struct sembuf sops = {0, -1, 0};
            if (semop(sem, &sops, 1) < 0) {
                if (errno == EINTR) continue;
                break;
            }

            char buf[4096];
            if (fgets(buf, sizeof(buf), f_in) != NULL) {
                fputs(buf, f_out);
                fflush(f_out);
            }
        }
    } else {
        FILE *fd = fopen(argv[1], "w+");
        if (!fd) {
            exit(EXIT_FAILURE);
        }

        char buffer[4096];
        while (1) {
            if (sigint_flag) {
                print_shadow();
                sigint_flag = 0;
            }

            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                if (errno == EINTR) continue;
                break;
            }

            fputs(buffer, fd);
            fflush(fd);

            struct sembuf sops = {0, 1, 0};
            semop(sem, &sops, 1);
        }
    }

    return 0;
}