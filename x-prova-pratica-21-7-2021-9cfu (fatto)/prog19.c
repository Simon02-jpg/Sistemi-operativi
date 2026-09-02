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
di un file F e un insieme di N stringhe (con N almeno pari ad 1). Il programa dovra' creare 
il file F e popolare il file con le stringhe provenienti da standard-input. 
Ogni stringa dovra' essere inserita su una differente linea del file.
L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito si
dovranno  generare file F e verificare, per una delle N stringhe di input, quante volte la inversa di tale stringa 
sia presente nel e N processi concorrenti ciascuno dei quali dovra' analizzare il contenuto
del file. Il risultato del controllo dovra' essere comunicato su standard
output tramite un messaggio. Quando tutti i processi avranno completato questo controllo, 
il contenuto del file F dovra' essere inserito in "append" in un file denominato "backup"
e poi il file F dova' essere troncato.

Qualora non vi sia immissione di input o di segnali, l'applicazione dovra' utilizzare 
non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t sigint_flag = 0;
char *filename;
char **strings;
int N;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void reverse_string(const char *src, char *dest) {
    int len = strlen(src);
    for (int i = 0; i < len; i++) {
        dest[i] = src[len - 1 - i];
    }
    dest[len] = '\0';
}

void process_task(int id) {
    char reversed[4096];
    reverse_string(strings[id], reversed);

    FILE *f = fopen(filename, "r");
    if (!f) exit(EXIT_FAILURE);

    int count = 0;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, reversed) == 0) {
            count++;
        }
    }
    fclose(f);

    char msg[4096];
    int len = snprintf(msg, sizeof(msg), "[Processo %d] Inversa di '%s' ('%s') trovata %d volte\n", getpid(), strings[id], reversed, count);
    write(STDOUT_FILENO, msg, len);
    exit(EXIT_SUCCESS);
}

void do_backup_and_truncate() {
    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        if (pid == 0) {
            process_task(i);
        }
    }

    for (int i = 0; i < N; i++) {
        wait(NULL);
    }

    FILE *in = fopen(filename, "r");
    FILE *out = fopen("backup", "a");
    if (in && out) {
        char buf[4096];
        size_t bytes;
        while ((bytes = fread(buf, 1, sizeof(buf), in)) > 0) {
            fwrite(buf, 1, bytes, out);
        }
    }
    if (in) fclose(in);
    if (out) fclose(out);

    FILE *trunc = fopen(filename, "w");
    if (trunc) fclose(trunc);
}

int main(int argc, char **argv) {
    if (argc < 3) exit(EXIT_FAILURE);

    filename = argv[1];
    strings = &argv[2];
    N = argc - 2;

    FILE *f = fopen(filename, "w");
    if (f) fclose(f);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) < 0) exit(EXIT_FAILURE);

    char buf[4096];
    while (1) {
        if (sigint_flag) {
            sigint_flag = 0;
            do_backup_and_truncate();
        }

        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            if (errno == EINTR) continue;
            break;
        }

        FILE *fout = fopen(filename, "a");
        if (fout) {
            fputs(buf, fout);
            fclose(fout);
        }
    }

    return 0;
}