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
dovranno  generare N thread concorrenti ciascuno dei quali dovra' analizzare il contenuto
del file F e verificare, per una delle N stringhe di input, quante volte tale stringa 
sia presente nel file. Il risultato del controllo dovra' essere comunicato su standard
output tramite un messaggio. Quando tutti i thread avranno completato questo controllo, 
il contenuto del file F dovra' essere inserito in "append" in un file denominato "backup"
e poi il file F dova' essere troncato.

Qualora non vi sia immissione di input o di segnali, l'applicazione dovra' utilizzare 
non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

volatile sig_atomic_t sigint_flag = 0;

char *filename;
char **strings;
int N;

typedef struct {
    int id;
} arg_t;

void *worker(void *arg)
{
    arg_t *a = (arg_t *)arg;
    int id = a->id;

    FILE *f = fopen(filename, "r");
    if (!f) {
        free(a);
        pthread_exit(NULL);
    }

    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, strings[id]) == 0)
            count++;
    }

    printf("[Thread %d] stringa '%s' trovata %d volte\n", id, strings[id], count);

    fclose(f);
    free(a);
    return NULL;
}

void backup_and_truncate()
{
    printf("\n--- SIGINT: backup in corso ---\n");

    pthread_t *t = malloc(sizeof(pthread_t) * N);

    for (int i = 0; i < N; i++) {
        arg_t *a = malloc(sizeof(arg_t));
        a->id = i;
        pthread_create(&t[i], NULL, worker, a);
    }

    for (int i = 0; i < N; i++) {
        pthread_join(t[i], NULL);
    }

    FILE *in = fopen(filename, "r");
    FILE *out = fopen("backup", "a");

    if (in && out) {
        char buf[1024];

        while (fgets(buf, sizeof(buf), in)) {
            fputs(buf, out);
        }
    }

    if (in) fclose(in);
    if (out) fclose(out);

    FILE *truncate = fopen(filename, "w");
    if (truncate) fclose(truncate);

    free(t);

    printf("--- backup completato ---\n");
}

void handler(int sig)
{
    (void)sig;
    sigint_flag = 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("uso: %s file string1 ... stringN\n", argv[0]);
        exit(1);
    }

    filename = argv[1];
    strings = &argv[2];
    N = argc - 2;

    FILE *f = fopen(filename, "w");
    if (f) fclose(f);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);

    char buf[1024];

    while (1) {

        if (sigint_flag) {
            sigint_flag = 0;   
            backup_and_truncate();
        }

        if (!fgets(buf, sizeof(buf), stdin)) {
            if (sigint_flag) continue;
            
            if (feof(stdin)) {
                printf("\n--- EOF: chiusura programma ---\n");
                break;
            }
            continue;
        }

        FILE *fout = fopen(filename, "a");
        if (fout) {
            fputs(buf, fout);
            fclose(fout);
        }

        if (sigint_flag) {
            sigint_flag = 0;
            backup_and_truncate();
        }
    }

    return 0;
}