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
Scrivere un programma che riceva in input tramite argv[] i nomi di N file,
con N magiore o uguale a 1. L'applicazione dovrà controllare che i nomi dei 
file siano diversi tra di loro.
I file dovranno essere creati oppure troncati se esistenti.
Per ogniuno dei file dovra' essere attivato un nuovo processo, che indicheremo
con Pi, che gestirà il contenuto del file. 
Tutti i processi Pi leggeranno linee di caratteri da standard input, se la linea
letta ha una lunghezza (incluso il terminatore di linea) inferiore oppure 
uguale a 10 caratteri essa dovra' essere scritta all'interno del file gestito
al processo Pi lettore. Altrimenti la linea dovrà essere comunicata al processo
parent il quale la riporterà sullo standard output.

L'applicazione dovrà essere in grado di gestire il segnale SIGINT 
(o CTRL_C_EVENT nel caso WinAPI) in modo tale che ogni processo che 
verrà colpito riporti su standard output il contenuto corrente di tutti
i file che erano stati specificati in argv[], seguendo l'ordine tramite cui
i nomi dei file erano presenti negli argomenti forniti all'applicazione.
In ogni caso, nessuno dei processi dovrà terminare la sua esecuzione in caso di arrivo
della segnalazione.

In caso non vi sia immissione di dati sullo standard input e non vi siano segnalazioni, 
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/wait.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STR 4096

volatile sig_atomic_t sigint_flag = 0;

void sigint_handler(int sig) {
    sigint_flag = 1;
}

// Struttura in memoria condivisa (mmap)
typedef struct {
    sem_t sem_stdin;
    sem_t sem_msg_ready;
    sem_t sem_msg_space;
    sem_t sem_print;
    sem_t sem_mutex;
    int active_children;
    int eof_reached;
    char msg_buffer[MAX_STR];
} shared_data_t;

int N_global;
char **filenames_global;
shared_data_t *shm;

// Funzione di stampa (Async-Signal-Safe)
void print_files() {
    sigint_flag = 0;
    
    // Assicura che un solo processo alla volta stampi su terminale
    sem_wait(&shm->sem_print);
    write(STDOUT_FILENO, "\n--- STAMPA FILE CORRENTI ---\n", 30);
    
    for (int i = 0; i < N_global; i++) {
        write(STDOUT_FILENO, filenames_global[i], strlen(filenames_global[i]));
        write(STDOUT_FILENO, ":\n", 2);
        
        int fd = open(filenames_global[i], O_RDONLY);
        if (fd >= 0) {
            char buf[1024];
            ssize_t bytes;
            while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
                write(STDOUT_FILENO, buf, bytes);
            }
            close(fd);
        }
    }
    write(STDOUT_FILENO, "----------------------------\n", 29);
    sem_post(&shm->sem_print);
}

void child_routine(int id, char *my_filename) {
    char line[MAX_STR];
    size_t line_idx = 0;

    while (1) {
        if (sigint_flag) print_files();

        if (sem_wait(&shm->sem_stdin) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (shm->eof_reached) {
            sem_post(&shm->sem_stdin);
            break;
        }

        int newline_found = 0;
        int error = 0;
        
        // Lettura sicura byte per byte da STDIN per processi condivisi
        while (line_idx < MAX_STR - 1) {
            char c;
            ssize_t ret = read(STDIN_FILENO, &c, 1);
            
            if (ret == -1) {
                if (errno == EINTR) { error = 1; break; }
                error = 2; break;
            }
            if (ret == 0) {
                shm->eof_reached = 1;
                if (line_idx == 0) error = 3; // EOF pulito
                else newline_found = 1; // EOF con dati residui
                break;
            }
            
            line[line_idx++] = c;
            if (c == '\n') { newline_found = 1; break; }
        }

        sem_post(&shm->sem_stdin);

        if (error == 1 && sigint_flag) continue;

        if (error == 3) {
            sem_wait(&shm->sem_mutex);
            shm->active_children--;
            if (shm->active_children == 0) sem_post(&shm->sem_msg_ready); // Sveglia il padre per uscire
            sem_post(&shm->sem_mutex);
            break;
        }

        if (newline_found) {
            line[line_idx] = '\0';
            
            // Logica <= 10 caratteri
            if (line_idx <= 10) {
                FILE *f = fopen(my_filename, "a");
                if (f) {
                    fprintf(f, "%s", line);
                    fclose(f);
                }
            } else { // Logica > 10 (Manda al padre)
                sem_wait(&shm->sem_msg_space);
                strcpy(shm->msg_buffer, line);
                sem_post(&shm->sem_msg_ready);
            }
            line_idx = 0; // Reset buffer per la prossima iterazione
        }
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
    if (argc < 2) exit(EXIT_FAILURE);

    int N = argc - 1;
    
    // Controllo duplicati
    for (int i = 1; i < argc; i++) {
        for (int j = i + 1; j < argc; j++) {
            if (strcmp(argv[i], argv[j]) == 0) exit(EXIT_FAILURE);
        }
    }

    N_global = N;
    filenames_global = &argv[1];

    // Creazione/Troncamento
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "w");
        if (f) fclose(f);
    }

    // Creazione Memoria Condivisa
    shm = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) exit(EXIT_FAILURE);

    // Inizializzazione Semafori pshared = 1
    sem_init(&shm->sem_stdin, 1, 1);
    sem_init(&shm->sem_msg_ready, 1, 0);
    sem_init(&shm->sem_msg_space, 1, 1);
    sem_init(&shm->sem_print, 1, 1);
    sem_init(&shm->sem_mutex, 1, 1);
    shm->active_children = N;
    shm->eof_reached = 0;
    shm->msg_buffer[0] = '\0';

    // Gestione Segnale
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    pid_t *pids = malloc(N * sizeof(pid_t));

    // Generazione N Processi
    for (int i = 0; i < N; i++) {
        pids[i] = fork();
        if (pids[i] < 0) exit(EXIT_FAILURE);
        if (pids[i] == 0) {
            child_routine(i, argv[i + 1]);
        }
    }

    // ================= MAIN (Padre) =================
    while (1) {
        if (sigint_flag) print_files();

        if (sem_wait(&shm->sem_msg_ready) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        sem_wait(&shm->sem_mutex);
        int active = shm->active_children;
        sem_post(&shm->sem_mutex);

        // Se c'è un messaggio, lo stampa
        if (strlen(shm->msg_buffer) > 0) {
            write(STDOUT_FILENO, shm->msg_buffer, strlen(shm->msg_buffer));
            shm->msg_buffer[0] = '\0'; // Consuma il messaggio
            sem_post(&shm->sem_msg_space);
        }

        // Tutti i figli sono morti, esce
        if (active == 0) break;
    }

    // Pulizia
    for (int i = 0; i < N; i++) waitpid(pids[i], NULL, 0);

    sem_destroy(&shm->sem_stdin);
    sem_destroy(&shm->sem_msg_ready);
    sem_destroy(&shm->sem_msg_space);
    sem_destroy(&shm->sem_print);
    sem_destroy(&shm->sem_mutex);
    munmap(shm, sizeof(shared_data_t));
    free(pids);

    return 0;
}