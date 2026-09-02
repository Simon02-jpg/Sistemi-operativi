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
Implementare una programma che riceva in input, tramite argv[], il nomi
di N file (con N maggiore o uguale a 1).
Per ogni nome di file F_i ricevuto input dovra' essere attivato un nuovo thread T_i.
Il main thread dovra' leggere indefinitamente stringhe dallo standard-input 
e dovra' rendere ogni stringa letta disponibile ad uno solo degli altri N thread
secondo uno schema circolare.
Ciascun thread T_i a sua volta, per ogni stringa letta dal main thread e resa a lui disponibile, 
dovra' scriverla su una nuova linea del file F_i. 

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo venga colpito esso dovra' 
riversare su standard-output e su un apposito file chiamato "output-file" il 
contenuto di tutti i file F_i gestiti dall'applicazione 
ricostruendo esattamente la stessa sequenza di stringhe (ciascuna riportata su 
una linea diversa) che era stata immessa tramite lo standard-input.

In caso non vi sia immissione di dati sullo standard-input, l'applicazione dovra' utilizzare 
non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEN 4096
#define PE(fmt, ...) do{fprintf(stderr, fmt " Error: %s\n", ##__VA_ARGS__, strerror(errno)); exit(EXIT_FAILURE);}while(0)

// Variabili globali per la sincronizzazione e lo stato
int num_threads;
char **files;
char **buffers;

// Array di semafori per orchestrare i turni
sem_t *sem_ready; // "Il main ha messo una stringa nel buffer, il thread può scrivere"
sem_t *sem_done;  // "Il thread ha scritto, il main può rimettere una stringa"

// Flag per gestire il segnale in modo safe
volatile sig_atomic_t sigint_flag = 0;

// ==========================================================
// HANDLER (Async-Signal-Safe)
// ==========================================================
void handler(int dummy) {
    (void)dummy;
    sigint_flag = 1;
}

// ==========================================================
// FUNZIONE DI RICOSTRUZIONE
// ==========================================================
void reconstruct_files() {
    printf("\n--- RECONSTRUCTING SEQUENCE ---\n");
    
    FILE *out_file = fopen("output-file", "w");
    if (!out_file) PE("Errore apertura output-file");

    // Apriamo tutti i file gestiti dai thread in modalità lettura
    FILE **in_files = malloc(num_threads * sizeof(FILE*));
    for (int i = 0; i < num_threads; i++) {
        in_files[i] = fopen(files[i], "r");
        if (!in_files[i]) PE("Errore rilettura file %s", files[i]);
    }

    int turn = 0;
    char line[MAX_LEN];
    
    // Leggiamo circolarmente esattamente come avevamo distribuito l'input
    while (1) {
        if (fgets(line, sizeof(line), in_files[turn]) != NULL) {
            printf("%s", line);
            fputs(line, out_file);
            turn = (turn + 1) % num_threads;
        } else {
            break; // Abbiamo raggiunto la fine della sequenza originale
        }
    }

    for (int i = 0; i < num_threads; i++) {
        fclose(in_files[i]);
    }
    free(in_files);
    fclose(out_file);
    
    printf("--- DONE ---\n\n");
}

// ==========================================================
// THREAD WORKER (T_i)
// ==========================================================
void * the_thread(void * param) {
    long int me = (long int)param;
    
    // O_TRUNC svuota il file all'avvio, garantendo che sia pulito
    FILE* target_file = fopen(files[me], "w");
    if (target_file == NULL) PE("Error opening %s", files[me]);

    while(1) {
        // 1. Aspetto che il Main mi dia il via
        sem_wait(&sem_ready[me]);

        // 2. Scrivo sul file e forzo il flush su disco
        fputs(buffers[me], target_file);
        fflush(target_file); 

        // 3. Dico al Main che ho finito
        sem_post(&sem_done[me]);
    }
    return NULL;
}

// ==========================================================
// MAIN THREAD
// ==========================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s file1 file2 ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    num_threads = argc - 1;
    files = argv + 1;

    // Allocazione delle risorse globali
    buffers = malloc(sizeof(char*) * num_threads);
    sem_ready = malloc(sizeof(sem_t) * num_threads);
    sem_done = malloc(sizeof(sem_t) * num_threads);

    for (int i = 0; i < num_threads; i++) {
        buffers[i] = malloc(MAX_LEN);
        sem_init(&sem_ready[i], 0, 0); // All'inizio nessun thread ha dati pronti (Rosso)
        sem_init(&sem_done[i], 0, 1);  // Tutti i buffer sono vuoti e liberi per il Main (Verde)
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, the_thread, (void*)(long)i) != 0) {
            PE("thread spawn failure");
        }
    }

    // Setup Handler
    struct sigaction act = {0};
    act.sa_handler = handler;
    // IMPORTANTE: Non usiamo SA_RESTART. In questo modo il CTRL+C interromperà 
    // la fgets bloccante, permettendoci di lanciare la ricostruzione all'istante.
    if (sigaction(SIGINT, &act, NULL) < 0) PE("sigaction error");

    char input_buf[MAX_LEN];
    int turn = 0;

    printf("Application ready. Type strings (CTRL+C to reconstruct, CTRL+D to exit):\n");

    while (1) {
        // Se c'è stato un CTRL+C
        if (sigint_flag) {
            reconstruct_files();
            sigint_flag = 0;
        }

        // fgets tiene la CPU a 0%
        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            // Se la fgets è stata "rotta" dal CTRL+C, riparti dal ciclo per gestire il flag
            if (errno == EINTR) continue; 
            
            // Se l'utente ha premuto CTRL+D (End Of File)
            if (feof(stdin)) break; 
            
            continue;
        }

        // Aspetto che il thread "turn" abbia finito con il suo vecchio buffer
        sem_wait(&sem_done[turn]);
        
        // Copio la nuova stringa nel buffer dedicato a quel thread
        strcpy(buffers[turn], input_buf);
        
        // Risveglio il thread "turn" per fargli scrivere il dato
        sem_post(&sem_ready[turn]);

        turn = (turn + 1) % num_threads;
        
        // Controllo se il segnale è arrivato nell'esatto momento tra la write e il ciclo
        if (sigint_flag) {
            reconstruct_files();
            sigint_flag = 0;
        }
    }

    return 0;
}