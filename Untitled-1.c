/*
SPECIFICATION TO BE IMPLEMENTED:
Scrivere un programma che riceva in input tramite argv[] un insime di N
stringhe, con N maggiore o uguale ad 1, che indicheremo con S1 ... SN.
Il main thread del programma dovra' leggere indefinitamente stringhe dallo standard input
e per ogni nuova stringa letta S dovra' attivare un nuovo thread, che riferiamo come T, 
passando a questo nuovo thread la stringa S letta come parametro.


Questo thread T dovra' controllare se la stringa S sia uguale a ciascuna
delle N stringhe S1 ... SN originariamente ricevute in input dal programma. Per ogni 
stringa trovata uguale all'atto del controllo, dovra' essere incrementato un contatore
apposito (si presuppne quindi che il programma gestisca N di questi contatori, uno per 
ognuna delle stringhe S1 ... SN ricevute tramite argv[]).

L'applicazione dovra' gestire il segnale  SIGINT (o CTRL_C_EVENT nel caso WinAPI) 
in modo tale che quando il processo venga colpito il suo main thread dovra' 
riportare su standard output il valore degli N contatori, su linee diverse dello 
stream di output, associando nel messaggio di output ciascuno dei valori alla relativa 
stringa S1 ... SN. La stessa informazione dovra' essere scritta all'interno di un file 
dal nome "output.txt" in modo che questo contenga sempre e solo i valori dei contatori 
piu' aggionati all'atto del processamento del segnale ricevuto.

In caso non vi sia immissione di dati sullo standard input e non vi siano segnalazioni, 
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.
*/

#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STR 4096
#define MAX_WORDS 128

volatile sig_atomic_t signal_received = 0;

char *target_strings[MAX_WORDS];
int counters[MAX_WORDS] = {0};
int N = 0;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
struct thread_args {
    char stringa_letta[MAX_STR];
};

void sigint_handler(int sig) {
    (void)sig;
    signal_received = 1;
}
void* worker(void* arg) {
    struct thread_args *my_data = (struct thread_args*)arg;
    my_data->stringa_letta[strcspn(my_data->stringa_letta, "\n")] = '\0';
    for (int i = 0; i < N; i++) {
        if (strcmp(my_data->stringa_letta, target_strings[i]) == 0) {
            pthread_mutex_lock(&count_mutex);
            counters[i]++;
            pthread_mutex_unlock(&count_mutex);
        }
    }
    free(my_data);
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s S1 [S2 ... SN]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    N = argc - 1;
    if (N > MAX_WORDS) N = MAX_WORDS;
    for (int i = 0; i < N; i++) {
        target_strings[i] = argv[i + 1];
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);
    char input_buffer[MAX_STR];
    printf("Dispatcher attivo. Inserisci stringhe (Ctrl+C per il report, Ctrl+D per uscire):\n");
    while (1) {
        if (signal_received) {
            signal_received = 0;
            printf("\n--- REPORT CONTATORI ---\n");
            for (int i = 0; i < N; i++) {
                printf("%s: %d\n", target_strings[i], counters[i]);
            }
            fflush(stdout);
            FILE *f_out = fopen("output.txt", "w");
            if (f_out) {
                for (int i = 0; i < N; i++) {
                    fprintf(f_out, "%s: %d\n", target_strings[i], counters[i]);
                }
                fclose(f_out);
            }
        }
        if (fgets(input_buffer, MAX_STR, stdin) == NULL) {
            if (errno == EINTR) {
                errno = 0;
                continue; 
            }
            break; 
        }
        struct thread_args *args = malloc(sizeof(struct thread_args));
        if (!args) exit(EXIT_FAILURE);
        strcpy(args->stringa_letta, input_buffer);
        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, args) == 0) {
            pthread_detach(tid);
        } else {
            free(args);
        }
    }
    pthread_mutex_destroy(&count_mutex);
    printf("Programma terminato.\n");
    return 0;
}