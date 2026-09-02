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
Si sviluppi una applicazione che riceva tramite argv[] la seguente linea di comando

    nome_prog -f file1 [file2] ... [fileN] -s stringa1 [stringa2] ... [stringaN] 
    
indicante N nomi di file (con N > 0) ed N ulteriori stringhe (il numero dei nomi dei 
file specificati deve corrispondere al numero delle stringhe specificate).

L'applicazione dovra' generare N processi figli concorrenti, in cui l'i-esimo di questi 
processi effettuera' la gestione dell'i-esimo dei file identificati tramite argv[].
Tale file dovra' essere rigenerato allo startup dell'applicazione.
Il main thread del processo originale dovra' leggere indefinitamente stringhe da 
standard input e dovra' comparare ogni stringa letta che le N stringhe ricevute in 
input tramite argv[].
Nel caso in cui la stringa letta sia uguale alla i-esima delle N stringhe ricevuta 
in input, questa dovra' essere comunicata all'i-esimo processo figlio in modo che questo 
la possa inserire in una linea del file di cui sta effettuando la gestione. Invece, 
se il main thread legge una stringa non uguale ad alcuna delle N stringhe ricevute 
in input, questa stringa dovra essere comunicata a tutti gli N processi figli
attivi, che la dovranno scrivere sui relativi file in una nuova linea.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando uno qualsiasi dei processi figli venga colpito 
dovra' riportare il contenuto del file da esso correntemente gestito in un file
con lo stesso nome ma con suffisso "_backup".  Invece il processo originale non dovra'
terminare o eseguire alcuna attivita' in caso di segnalazione.

In caso non vi sia immissione di dati sullo standard input e non vi siano segnalazioni, 
l'applicazione dovra' utilizzare non piu' del 5% della capacita' di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
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

volatile sig_atomic_t backup_flag = 0;

void child_sigint_handler(int sig) {
    backup_flag = 1;
}

typedef struct {
    sem_t sem_data_ready;
    sem_t sem_space_avail;
    char buffer[MAX_STR];
} shared_channel_t;

int main(int argc, char** argv) {
    int s_index = -1;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            s_index = i;
            break;
        }
    }

    if (s_index == -1 || strcmp(argv[1], "-f") != 0) {
        exit(EXIT_FAILURE);
    }

    int N = s_index - 2; 
    if (N < 1 || argc - (s_index + 1) != N) {
        exit(EXIT_FAILURE);
    }

    shared_channel_t *shm = mmap(NULL, N * sizeof(shared_channel_t), 
                                 PROT_READ | PROT_WRITE, 
                                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; i++) {
        sem_init(&shm[i].sem_data_ready, 1, 0);
        sem_init(&shm[i].sem_space_avail, 1, 1);
    }

    pid_t *pids = malloc(N * sizeof(pid_t));

    for (int i = 0; i < N; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) {
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = child_sigint_handler;
            sigaction(SIGINT, &sa, NULL);

            char *my_file = argv[2 + i];
            
            FILE *fd_init = fopen(my_file, "w");
            if (fd_init) fclose(fd_init);

            while (1) {
                if (sem_wait(&shm[i].sem_data_ready) < 0) {
                    if (errno == EINTR) {
                        if (backup_flag) {
                            backup_flag = 0;
                            char backup_name[512];
                            snprintf(backup_name, sizeof(backup_name), "%s_backup", my_file);
                            
                            FILE *f_in = fopen(my_file, "r");
                            FILE *f_bak = fopen(backup_name, "w");
                            if (f_in && f_bak) {
                                char buf[MAX_STR];
                                size_t bytes;
                                while ((bytes = fread(buf, 1, sizeof(buf), f_in)) > 0) {
                                    fwrite(buf, 1, bytes, f_bak);
                                }
                            }
                            if (f_in) fclose(f_in);
                            if (f_bak) fclose(f_bak);
                        }
                        continue; 
                    }
                    break;
                }

                if (strcmp(shm[i].buffer, "EXIT\n") == 0) {
                    break;
                }

                FILE *fd = fopen(my_file, "a");
                if (fd) {
                    fprintf(fd, "%s\n", shm[i].buffer);
                    fflush(fd);
                    fclose(fd);
                }

                sem_post(&shm[i].sem_space_avail);
            }
            exit(EXIT_SUCCESS);
        }
    }

    signal(SIGINT, SIG_IGN);

    char input_buffer[MAX_STR];

    while (1) {
        if (fgets(input_buffer, MAX_STR, stdin) == NULL) {
            if (errno == EINTR) continue;
            break; 
        }

        char clean_input[MAX_STR];
        strcpy(clean_input, input_buffer);
        clean_input[strcspn(clean_input, "\n")] = '\0';

        int target_index = -1;
        for (int i = 0; i < N; i++) {
            if (strcmp(clean_input, argv[s_index + 1 + i]) == 0) {
                target_index = i;
                break;
            }
        }

        if (target_index != -1) {
            sem_wait(&shm[target_index].sem_space_avail);
            strcpy(shm[target_index].buffer, clean_input);
            sem_post(&shm[target_index].sem_data_ready);
        } else {
            for (int i = 0; i < N; i++) {
                sem_wait(&shm[i].sem_space_avail);
                strcpy(shm[i].buffer, clean_input);
                sem_post(&shm[i].sem_data_ready);
            }
        }
    }

    for (int i = 0; i < N; i++) {
        sem_wait(&shm[i].sem_space_avail);
        strcpy(shm[i].buffer, "EXIT\n");
        sem_post(&shm[i].sem_data_ready);
    }

    for (int i = 0; i < N; i++) {
        waitpid(pids[i], NULL, 0);
    }

    for (int i = 0; i < N; i++) {
        sem_destroy(&shm[i].sem_data_ready);
        sem_destroy(&shm[i].sem_space_avail);
    }
    
    munmap(shm, N * sizeof(shared_channel_t));
    free(pids);

    return 0;
}