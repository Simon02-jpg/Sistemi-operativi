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
given specification. Most of the requested header files (for either 
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
Implementare un'applicazione che riceva in input tramite argv[] il 
nome di un file F ed una stringa indicante un valore numerico N maggiore
o uguale ad 1.
L'applicazione, una volta lanciata dovra' creare il file F ed attivare 
N thread. Inoltre, l'applicazione dovra' anche attivare un processo 
figlio, in cui vengano attivati altri N thread. 
I due processi che risulteranno attivi verranno per comodita' identificati
come A (il padre) e B (il figlio) nella successiva descrizione.

Ciascun thread del processo A leggera' stringhe da standard input. 
Ogni stringa letta dovra' essere comunicata al corrispettivo thread 
del processo B tramite memoria condivisa, e questo la scrivera' su una 
nuova linea del file F. Per semplicita' si assuma che ogni stringa non
ecceda la taglia di 4KB. 

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando il processo A venga colpito esso dovra' 
inviare la stessa segnalazione verso il processo B. Se invece ad essere 
colpito e' il processo B, questo dovra' riversare su standard output il 
contenuto corrente del file F.

Qalora non vi sia immissione di input, l'applicazione dovra' utilizzare 
non piu' del 5% della capacita' di lavoro della CPU. 

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 4096
#define PE(fmt, ...) do{fprintf(stderr, fmt " Error: %s\n", ##__VA_ARGS__, strerror(errno)); exit(EXIT_FAILURE);}while(0)

// Struttura dati che andrà in Memoria Condivisa
typedef struct {
    sem_t sem_a;        // Semaforo per il thread del processo A (Padre)
    sem_t sem_b;        // Semaforo per il thread del processo B (Figlio)
    char buffer[SIZE];  // Area di memoria per trasferire la stringa (Max 4KB)
} ThreadData;

// Variabili globali per gli handler
pid_t child_pid = -1;
char *global_filename = NULL;
int file_fd = -1;
ThreadData *shared_mem = NULL;

// ==========================================================
// HANDLERS (Async-Signal-Safe)
// ==========================================================
void parent_handler(int signo) {
    // Il padre inoltra il segnale al figlio
    if (child_pid > 0) {
        kill(child_pid, signo);
    }
}

void child_handler(int signo) {
    (void)signo;
    // Il figlio stampa il contenuto del file usando SOLO funzioni safe
    int fd = open(global_filename, O_RDONLY);
    if (fd >= 0) {
        char buf[4096];
        ssize_t n;
        write(STDOUT_FILENO, "\n--- CONTENUTO CORRENTE FILE ---\n", 33);
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, n);
        }
        write(STDOUT_FILENO, "-------------------------------\n", 32);
        close(fd);
    }
}

// ==========================================================
// THREAD WORKERS
// ==========================================================
void *parent_worker(void *arg) {
    long id = (long)arg;
    
    while(1) {
        // Aspetta che il figlio abbia liberato il buffer
        sem_wait(&shared_mem[id].sem_a);

        // Mutex nativo di POSIX per leggere da stdin in modo thread-safe
        flockfile(stdin);
        
        // fgets blocca il thread tenendo la CPU a 0% finché non c'è input
        if (fgets(shared_mem[id].buffer, SIZE, stdin) == NULL) {
            funlockfile(stdin);
            break; // Uscita pulita in caso di EOF (CTRL+D)
        }
        
        funlockfile(stdin);

        // Avvisa il figlio che la stringa è pronta
        sem_post(&shared_mem[id].sem_b);
    }
    return NULL;
}

void *child_worker(void *arg) {
    long id = (long)arg;
    
    while(1) {
        // Aspetta che il padre abbia scritto la stringa
        sem_wait(&shared_mem[id].sem_b);

        // Scrive sul file in modo atomico grazie al flag O_APPEND
        size_t len = strlen(shared_mem[id].buffer);
        if (write(file_fd, shared_mem[id].buffer, len) < 0) {
            perror("write error");
        }

        // Avvisa il padre che il buffer è nuovamente libero
        sem_post(&shared_mem[id].sem_a);
    }
    return NULL;
}

// ==========================================================
// MAIN
// ==========================================================
int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Uso: %s <filename> <num_threads>\n", argv[0]);
        return -1;
    }

    global_filename = argv[1];
    long num_threads = strtol(argv[2], NULL, 10);
    
    if (num_threads < 1) {
        printf("num_threads deve essere >= 1\n");
        return -1;
    }

    // 1. Creazione file F con O_APPEND per garantire scritture concorrenti sicure
    file_fd = open(global_filename, O_CREAT | O_WRONLY | O_APPEND | O_TRUNC, 0666);
    if (file_fd < 0) PE("Errore apertura file");

    // 2. Allocazione della memoria condivisa strutturata
    shared_mem = mmap(NULL, sizeof(ThreadData) * num_threads, 
                      PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_mem == MAP_FAILED) PE("mmap error");

    // 3. Inizializzazione Semafori POSIX in memoria condivisa
    for (long i = 0; i < num_threads; i++) {
        // pshared = 1 permette ai semafori di funzionare tra PROCESSI diversi
        sem_init(&shared_mem[i].sem_a, 1, 1); // Il padre parte col verde (1)
        sem_init(&shared_mem[i].sem_b, 1, 0); // Il figlio parte col rosso (0)
    }

    // 4. Creazione processo Figlio
    if ((child_pid = fork()) < 0) PE("fork error");

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);

    if (child_pid == 0) {
        // === RAMO PROCESSO B (FIGLIO) ===
        
        // Isoliamo il figlio in un suo process group.
        // Questo evita che premendo CTRL+C riceva il segnale dal terminale: 
        // lo riceverà SOLO se glielo inoltra esplicitamente il padre (come da specifica).
        setpgid(0, 0); 
        
        struct sigaction sa = {0};
        sa.sa_handler = child_handler;
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, NULL);

        for (long i = 0; i < num_threads; i++) {
            if (pthread_create(&threads[i], NULL, child_worker, (void*)i) != 0)
                PE("pthread_create child");
        }
        
    } else {
        // === RAMO PROCESSO A (PADRE) ===
        
        struct sigaction sa = {0};
        sa.sa_handler = parent_handler;
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, NULL);

        for (long i = 0; i < num_threads; i++) {
            if (pthread_create(&threads[i], NULL, parent_worker, (void*)i) != 0)
                PE("pthread_create parent");
        }
    }

    // Entrambi i processi restano in attesa (CPU 0%)
    while(1) {
        pause();
    }

    return 0; // Mai raggiunto
}