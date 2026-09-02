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
Implementare una programma che riceva in input, tramite argv[], 
N differenti stringhe S1 ... SN, con N maggiore o uguale a 1.
Per ognuna delle stringhe dovra' essere attivato un nuovo processo  
che gestira' tale stringa  (indichiamo quindi con P1 ... PN i 
processi che dovranno essere attivati).
Il processo originale dovra' leggere stringhe dallo standard input, e dovra'
comunicare ogni stringa letta a P1. P1 dovra' verificare se la stringa ricevuta
e' uguale alla stringa S1 da lui gestita, e dovra' incrementare un contatore
in caso positivo. Altrimenti, in caso negativo, dovra' comunicare la stringa
ricevuta al processo P2 che fara' lo stesso controllo, e cosi' via fino a PN.

L'applicazione dovra' gestire il segnale SIGINT (o CTRL_C_EVENT nel caso
WinAPI) in modo tale che quando uno qualsiasi dei processi Pi venga colpito
esso dovra' riportare su standard output il valore del contatore che indica
quante volte la stringa Si e' stata trovata uguale alla stringa che 
il processo originale aveva letto da standard input. Il processo originale 
non dovra' invece eseguire alcuna attivita' all'arrivo della segnalazione.

In caso non vi sia immissione di dati sullo standard input, e non vi siano 
segnalazioni, l'applicazione dovra' utilizzare non piu' del 5% della capacita' 
di lavoro della CPU.

*****************************************************************/
#ifdef Posix_compile
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#else
#include <windows.h>
#endif

#define PE(fmt, ...) do{printf(fmt " Error: [line: %d][%s]\n", ##__VA_ARGS__, __LINE__, strerror(errno));}while(0);
#define E() do{exit(EXIT_FAILURE);}while(0);

// Variabili globali per l'handler (Async-Signal-Safe)
char msg[256];
int msg_len = 0;

void handler(int sig)
{
    (void)sig;
    if (msg_len > 0) {
        write(STDOUT_FILENO, msg, msg_len);
    }
}

// Funzione eseguita esclusivamente dai processi Child (P1... PN)
static inline void child(char* my_string, int rdp, int wrp, int pnum)
{
    // Setup Handler per i figli
    struct sigaction act = {0};
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    sigaction(SIGINT, &act, NULL);

    int counter = 0;
    
    // Inizializza il messaggio di default a zero
    msg_len = snprintf(msg, sizeof(msg), "[P%d] Stringa '%s' trovata %d volte\n", pnum, my_string, counter);

    // Mappiamo i File Descriptor su FILE* per leggere riga per riga
    FILE *f_in = fdopen(rdp, "r");
    FILE *f_out = (wrp != -1) ? fdopen(wrp, "w") : NULL;

    char buf[4096];
    
    // La fgets blocca la CPU a 0% e previene accavallamenti tra stringhe
    while(fgets(buf, sizeof(buf), f_in) != NULL) 
    {
        // Rimuoviamo il newline per compararlo correttamente
        char cmp_buf[4096];
        strcpy(cmp_buf, buf);
        cmp_buf[strcspn(cmp_buf, "\n")] = '\0';

        if(strcmp(my_string, cmp_buf) == 0)
        {
            counter++;
            // Aggiorniamo il buffer condiviso con l'handler in modo sicuro
            msg_len = snprintf(msg, sizeof(msg), "[P%d] Stringa '%s' trovata %d volte\n", pnum, my_string, counter);
        }
        else if(f_out != NULL)
        {
            // Non è la nostra stringa, passiamola al prossimo processo (incluso il \n originale)
            fputs(buf, f_out);
            fflush(f_out); // Fondamentale svuotare il buffer!
        }
    }
    
    // Uscita pulita in caso di EOF
    fclose(f_in);
    if(f_out) fclose(f_out);
    exit(0);
}


int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("Uso: %s str1 str2 ...\n", argv[0]);
        E();
    }
    
    char* my_string = NULL;
    int rdp = -1;
    int wrp = -1;
    int pnum = 0;
    
    // Creazione Pipeline a cascata
    for (int i = 1; i < argc; i++)
    {
        int pipefd[2];
        if (pipe(pipefd) < 0) { PE("Pipe error"); E(); }
        
        pid_t ret = fork();
        if(ret < 0)
        {
            PE("Fork error"); E();
        }
        else if(ret == 0) // CHILD
        {
            pnum = i;
            my_string = argv[i];
            
            // Chiude il vecchio descriptor di lettura ereditato dal padre (evita leak!)
            if (rdp != -1) close(rdp); 
            
            close(pipefd[1]); // Il figlio non scrive su QUESTA pipe
            rdp = pipefd[0];  // Il figlio legge da QUESTA pipe
        }
        else // PARENT (o il Child precedente che agisce da padre per un istante)
        {
            close(pipefd[0]); // Chiudo il lato lettura della pipe appena creata
            wrp = pipefd[1];  // Salvo il lato scrittura verso il nuovo figlio
            break;            // Smetto di creare processi, tocca al figlio continuare il ciclo
        }
    }
    
    if(pnum > 0)
    {
        // Sono un processo Child
        child(my_string, rdp, wrp, pnum);
    }
    else
    {
        // Sono il Processo Padre Originale (pnum == 0)
        
        // 1. IGNORARE SIGINT COME DA SPECIFICA
        signal(SIGINT, SIG_IGN); 

        // 2. LETTURA INPUT E INVIO A P1
        char buf[4096];
        FILE *f_out = fdopen(wrp, "w");

        printf("Inserisci stringhe (CTRL+D per terminare):\n");
        
        // La fgets blocca il main, CPU allo 0%
        while(fgets(buf, sizeof(buf), stdin) != NULL)
        {
            if (strlen(buf) > 1) { // Evita invii a vuoto
                fputs(buf, f_out);
                fflush(f_out);
            }
        }
        
        // Chiusura pipe triggera l'EOF a catena per spegnere i figli pulitamente
        fclose(f_out); 
        
        // Aspettiamo P1 (che a sua volta aspetterà P2 prima di chiudersi)
        wait(NULL); 
        printf("\nProgramma terminato.\n");
    }
    
    return 0;
}