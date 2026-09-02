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
Scrivere un programma che riceva in input tramite argv[1] e argv[2] seguenti 
due parametri:
- in argv[1] la taglia del blocco B
- in argv[2] il numero di thread N.
L'applicazione dovra' quindi generare N tread, che indicheremo con T1 ... TN.
Ciascuno di questi thread dovra', secondo una turnazione circolare, acquisire 
un blocco di B bytes dallo standard input e dovra' scriverli in un file il cui 
nome dovra' essere "output_<THREAD_ID>".
Questo file dovra' essere creato dallo stesso thread in carico di gestirlo, in 
particolare al suo stesso startup.
Il main thread, dopo aver creato gli N thread che effettueranno le operazioni sopra
indicate, rimarra' in pausa.

L'applicazione dovra' gestire il segnale  SIGINT (o CTRL_C_EVENT nel caso WinAPI) 
in modo tale che quando il processo venga colpito il suo main thread dovra' 
riportare su standard output il contenuto dei file aggiornati dagli N thread
in modo che sia ricostruita esattamente la stessa sequenza di bytes originariamente 
acquisita tramite standard input.

In caso non vi sia immissione di dati sullo standard input e non vi siano segnalazioni, 
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define pe(fmt, ...) \
    do { \
        fprintf(stderr, fmt ": %s\n", ##__VA_ARGS__, strerror(errno)); \
    } while (0)

#define E EXIT_FAILURE

// Variabili globali per l'handler
long long B = 0; // Taglia del blocco in bytes
long long N = 0; // Numero di thread
int *fds = NULL; // File descriptors
sem_t *sem;      // Semafori

// =======================================================================
// HANDLER (Async-Signal-Safe)
// =======================================================================
void handler(int sig)
{
    (void)sig; // Silenzia il warning del compilatore
    
    char start_msg[] = "\n\n--- RICOSTRUZIONE OUTPUT ---\n";
    write(STDOUT_FILENO, start_msg, sizeof(start_msg) - 1);

    // 1. Riportiamo il cursore di lettura di TUTTI i file all'inizio
    for(long long i = 0; i < N; i++) {
        lseek(fds[i], 0, SEEK_SET);
    }

    // 2. Leggiamo ciclicamente blocco per blocco (B bytes) da ogni thread
    long long i = 0;
    char buf[B]; // Variable Length Array, perfetto per l'allocazione al volo
    
    while(1) {
        ssize_t bytes_read = read(fds[i], buf, B);
        
        // Se bytes_read è <= 0, significa che la sequenza originale si è fermata qui
        if(bytes_read <= 0) {
            break;
        }
        
        // Scriviamo esattamente i bytes ricostruiti
        write(STDOUT_FILENO, buf, bytes_read);
        
        i = (i + 1) % N;
    }
    
    char end_msg[] = "\n--- FINE RICOSTRUZIONE ---\n";
    write(STDOUT_FILENO, end_msg, sizeof(end_msg) - 1);
}

// =======================================================================
// FUNZIONE DEL THREAD
// =======================================================================
void* thread(void* arg)
{
    long long i = *(long long*)arg;
    
    // Creazione del file come da specifica
    char name[64];
    snprintf(name, sizeof(name), "output_%lu", (unsigned long)pthread_self());
    
    // Usiamo O_APPEND in modo che le scritture si accodino in fondo
    if((fds[i] = open(name, O_RDWR | O_APPEND | O_CREAT | O_TRUNC, 0666)) < 0) {
        pe("open error");
        exit(E);
    }
    
    while(1)
    {
        // Aspettiamo il nostro turno
        sem_wait(&sem[i]);
        
        char read_buf[B];
        ssize_t read_index = 0;
        
        // Assicuriamo di leggere esattamente un blocco di B bytes
        while(read_index < B) {
            ssize_t n = read(STDIN_FILENO, &read_buf[read_index], B - read_index);
            
            if(n == 0) {
                // L'utente ha premuto CTRL+D (EOF)
                break;
            }
            if(n < 0) {
                if(errno == EINTR) continue; // Ignoriamo le interruzioni di segnale
                pe("read error");
                exit(E);
            }
            read_index += n;
        }
        
        // Se abbiamo letto qualcosa, lo scriviamo sul nostro file
        if(read_index > 0) {
            if(write(fds[i], read_buf, read_index) < read_index) {
                pe("write error");
            }
        }
        
        // Passiamo il turno al thread successivo
        sem_post(&sem[(i + 1) % N]);
        
        // Se non siamo riusciti a riempire il blocco, lo stream è finito. Usciamo.
        if (read_index < B) {
            break;
        }
    }
    
    return NULL;
}

// =======================================================================
// MAIN
// =======================================================================
int main(int argc, char** argv)
{
    if(argc != 3) {
        fprintf(stderr, "Uso: %s <Block_Size_B> <Num_Threads_N>\n", argv[0]);
        exit(E);
    }
    
    // Parsing pulito degli argomenti
    B = strtoll(argv[1], NULL, 10);
    N = strtoll(argv[2], NULL, 10);
    if(B <= 0 || N <= 0) {
        fprintf(stderr, "Parametri non validi\n");
        exit(E);
    }
    
    // Setup Handler
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = handler;
    act.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &act, NULL) < 0) {
        pe("sigaction error");
        exit(E);
    }
    
    // Allocazione delle risorse globali
    sem = calloc(N, sizeof(sem_t));
    fds = calloc(N, sizeof(int));
    if(!sem || !fds) {
        pe("calloc error");
        exit(E);
    }
    
    // Inizializzazione Semafori
    for(long long i = 0; i < N; i++) {
        // Il flag `pshared` a 0 è più corretto per i thread dello stesso processo
        if(sem_init(&sem[i], 0, 0) < 0) {
            pe("sem_init error");
            exit(E);
        }
    }
    
    // Creazione Thread (passiamo un array statico nel main per evitare leak)
    long long args[N];
    pthread_t throwaway;
    for(long long i = 0; i < N; i++) {
        args[i] = i;
        if(pthread_create(&throwaway, NULL, thread, &args[i]) != 0) {
            pe("pthread_create error");
            exit(E);
        }
    }
    
    // Diamo il via al primo thread
    if(sem_post(&sem[0]) < 0) {
        pe("sem_post error");
        exit(E);
    }
    
    // Il main thread dorme a ciclo continuo (CPU allo 0%)
    while(1) {
        pause();
    }
    
    return 0;
}