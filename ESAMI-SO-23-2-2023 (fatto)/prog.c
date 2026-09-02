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
Scrivere un programma che riceva in input tramite argv[] un numero di parametri
superiore a 1 (incluso il nome del programma). Ogni parametro registrato a partire
da argv[1] deve corrispondere ad una stringa di un unico carattere, ed ognuna di 
queste stringhe dovra' essere diversa dalle altre. 
Per ognuna di queste stringhe l'applicazione dovra' attivare un thread che ne 
effettuera' la gestione.
Dopo aver attivato tali thread, l'applicazione dovra' leggere ogni stringa proveniente 
da standard input e renderla disponibile a tutti i thread attivati in precedenza. 
Ciascuno di essi dovra' controllare se qualche carattere presente nella stringa proveniente
da standard input corrisponde al carattere della stringa che lui stesso sta gestendo, 
ed in tal caso dovra' sostituire quel carattere nella stringa proveniente dallo standard input
sovrascrivendolo col carattere '*'. 
Al termine delle attivita' di controllo e sostituzione di caratteri da parte 
di tutti i thread, l'applicazione dovra' scrivere su un file dal nome "output.txt"
la stringa originale proveniente da standard input e quella ottenuta tramite le 
sostituzioni di carattere (anche se non realmente avvenute), su due linee consecutive del file.

L'applicazione dovra' gestire il segnale  SIGINT (o CTRL_C_EVENT nel caso WinAPI) 
in modo tale che quando il processo venga colpito dovra' riportare su standard output 
le stringhe presenti in output.txt che possono aver subito sostituzione di carattere.

In caso non vi sia immissione di dati sullo standard input e non vi siano segnalazioni, 
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

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
char *target_chars;
char original_str[4096];
char mod_str[4096];

sem_t *sem_start;
sem_t sem_done;

void handler(int sig) {
    (void)sig;
    sigint_flag = 1;
}

void print_modified_strings() {
    FILE *f = fopen("output.txt", "r");
    if (f) {
        char line1[4096];
        char line2[4096];
        write(STDOUT_FILENO, "\n", 1);
        while (fgets(line1, sizeof(line1), f) && fgets(line2, sizeof(line2), f)) {
            write(STDOUT_FILENO, line2, strlen(line2));
        }
        fclose(f);
    }
}

void* worker(void* arg) {
    long id = (long)arg;
    char my_char = target_chars[id];

    while (1) {
        sem_wait(&sem_start[id]);
        
        for (int i = 0; mod_str[i] != '\0'; i++) {
            if (mod_str[i] == my_char) {
                mod_str[i] = '*';
            }
        }
        
        sem_post(&sem_done);
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        exit(EXIT_FAILURE);
    }

    N = argc - 1;
    target_chars = malloc(N);
    sem_start = malloc(N * sizeof(sem_t));

    if (!target_chars || !sem_start) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; i++) {
        target_chars[i] = argv[i + 1][0];
        if (sem_init(&sem_start[i], 0, 0) < 0) exit(EXIT_FAILURE);
    }
    if (sem_init(&sem_done, 0, 0) < 0) exit(EXIT_FAILURE);

    FILE *f_init = fopen("output.txt", "w");
    if (f_init) fclose(f_init);

    for (long i = 0; i < N; i++) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, (void*)i) != 0) {
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
            print_modified_strings();
            sigint_flag = 0;
        }

        if (fgets(original_str, sizeof(original_str), stdin) == NULL) {
            if (errno == EINTR) continue;
            break;
        }

        original_str[strcspn(original_str, "\n")] = '\0';
        strcpy(mod_str, original_str);

        for (int i = 0; i < N; i++) {
            sem_post(&sem_start[i]);
        }

        for (int i = 0; i < N; i++) {
            sem_wait(&sem_done);
        }

        FILE *f_out = fopen("output.txt", "a");
        if (f_out) {
            fprintf(f_out, "%s\n%s\n", original_str, mod_str);
            fclose(f_out);
        }
    }

    return 0;
}