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
Scrivere un programma che riceva in input tramite argv[] N coppie di stringhe 
con N maggiore o uguale a 1, in cui la prima stringa della coppia indica il 
nome di un file. 
Per ogni coppia di strighe dovra' essere attivato un processo che dovra' creare 
il file associato alla prima delle stringhe della coppia o poi ogni 5 secondi 
dovra' effettuare il controllo su quante volte la seconda delle stringhe della 
coppia compare nel file, riportando il risultato su standard output.
Il main thread del processo originale dovra' leggere lo stream da standard input in 
modo indefinito, e dovra' scrivere i byte letti in uno dei file identificati 
tramite i nomi passati con argv[]. La scelta del file dove scrivere dovra' 
avvenire in modo circolare a partire dal primo file identificato tramite argv[], 
e il cambio del file destinazione dovra' avvenire qualora venga ricevuto il 
segnale SIGINT (o CTRL_C_EVENT nel caso WinAPI).
Se il segnale SIGINT (o CTRL_C_EVENT nel caso WinAPI) colpira' invece uno degli 
altri processi, questo dovra' riportare il contenuto del file che sta gestendo 
su standard output.

In caso non vi sia immissione di dati sullo standard input, l'applicazione 
dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t print_flag = 0;
volatile sig_atomic_t change_flag = 0;

void child_handler(int sig) {
    (void)sig;
    print_flag = 1;
}

void parent_handler(int sig) {
    (void)sig;
    change_flag = 1;
}

void child_process(const char *filename, const char *search_str) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = child_handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) exit(EXIT_FAILURE);

    int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) exit(EXIT_FAILURE);

    while (1) {
        unsigned int rem = 5;
        while (rem > 0) {
            rem = sleep(rem);
            if (print_flag) {
                print_flag = 0;
                lseek(fd, 0, SEEK_SET);
                char buf[4096];
                ssize_t n;
                write(STDOUT_FILENO, "\n--- CONTENUTO FILE ---\n", 24);
                while ((n = read(fd, buf, sizeof(buf))) > 0) {
                    write(STDOUT_FILENO, buf, n);
                }
            }
        }

        lseek(fd, 0, SEEK_SET);
        FILE *f = fdopen(dup(fd), "r");
        if (!f) continue;
        
        int count = 0;
        char line[4096];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            if (strcmp(line, search_str) == 0) {
                count++;
            }
        }
        fclose(f);

        char msg[512];
        int len = snprintf(msg, sizeof(msg), "File [%s]: '%s' trovato %d volte\n", filename, search_str, count);
        write(STDOUT_FILENO, msg, len);
    }
}

int main(int argc, char **argv) {
    if (argc < 3 || argc % 2 == 0) {
        exit(EXIT_FAILURE);
    }

    int N = (argc - 1) / 2;

    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        if (pid == 0) {
            child_process(argv[i * 2 + 1], argv[i * 2 + 2]);
            exit(EXIT_SUCCESS);
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = parent_handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) exit(EXIT_FAILURE);

    int *fds = malloc(N * sizeof(int));
    if (!fds) exit(EXIT_FAILURE);

    for (int i = 0; i < N; i++) {
        fds[i] = open(argv[i * 2 + 1], O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (fds[i] < 0) exit(EXIT_FAILURE);
    }

    int current = 0;
    char buf[4096];

    while (1) {
        if (change_flag) {
            current = (current + 1) % N;
            change_flag = 0;
        }

        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            if (errno == EINTR) continue;
            break;
        }

        if (write(fds[current], buf, strlen(buf)) < 0) {
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < N; i++) {
        close(fds[i]);
    }
    free(fds);

    return 0;
}