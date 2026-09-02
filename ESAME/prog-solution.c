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
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//this is the Posix version
//please compile with
//gcc prog-solution.c -lpthread -DPosix_compile

#define SIZE 4096


FILE *file;
pid_t pid;
int fd;
char *filename;
long num_threads;
char **memory_segments;
int sd1,sd2;

void parent_handler(int signo){
	printf("parent received signal %d - forwarding to child (pid %d)\n",signo,pid);
	kill(pid,signo);
}

void child_handler(int dummy){
	char buff[1024];

	sprintf(buff, "cat %s\n",filename);
	system(buff);

}	

void *parent_worker(void *);
void *child_worker(void *);

int main(int argc, char** argv){

	pthread_t tid;
//	int exit_code;
	long i;
	int ret;
			
	if (argc < 3) {
		printf("Usage: %s filename num_threads\n", argv[0]);
		return -1;
	}
	
	if ((fd = open(argv[1], O_CREAT | O_RDWR | O_TRUNC, 0666)) == -1) {
		printf("Error opening file %s\n",argv[1]);
		return -1;
	}	
	
	if ((file = fdopen(fd,"w+")) == NULL) {
		printf("Error fopening file %s\n",argv[1]);
		return -1;
	}

	filename = argv[1];
	num_threads = strtol(argv[2],NULL,10);
	
	if (num_threads < 1) {
		printf("num_threads must be greater than 0\n");
		return -1;
	}
	
	memory_segments = malloc(sizeof(char*)*num_threads);

	if (memory_segments == NULL){
		printf("memory alloction error (1)\n");
		return -1;
	}

	for ( i = 0; i < num_threads; i++) {
		memory_segments[i] = (char *)mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0) ; 
		if(memory_segments[i] == NULL){
			printf("mmap error\n");
			return -1;
		}
	}

	sd1 = semget(IPC_PRIVATE,num_threads,IPC_CREAT|0666);
	if (sd1 == -1){
		printf("semget error (1)\n");
		return -1;
	}

	for ( i = 0; i < num_threads; i++) {
		ret = semctl(sd1,i,SETVAL,1); ; 
		if(ret == -1){
			printf("semctl error (1)\n");
			return -1;
		}
	}

	sd2 = semget(IPC_PRIVATE,num_threads,IPC_CREAT|0666);
	if (sd2 == -1){
		printf("semget error (2)\n");
		return -1;
	}

	for ( i = 0; i < num_threads; i++) {
		ret = semctl(sd2,i,SETVAL,0); ; 
		if(ret == -1){
			printf("semgctl error (2)\n");
			return -1;
		}
	}

	if ((pid = fork()) == -1) {
		printf("fork error\n");
		return -1;
	}

	if(pid == 0){

		signal(SIGINT,child_handler);

		for ( i = 0; i < num_threads; i++) {
			if (pthread_create(&tid, NULL, child_worker, (void *)i ) == -1) {
				printf("pthread_create error (child)\n");
				return -1;
			}
		}

	}
	else{

		signal(SIGINT,parent_handler);

		for ( i = 0; i < num_threads; i++) {
			if (pthread_create(&tid, NULL, parent_worker, (void *)i) == -1) {
				printf("pthread_create error (parent)\n");
				return -1;
			}
		}

	}

	while(1) pause();
	
}

void *parent_worker(void *arg) {

	long me;
	struct sembuf oper;
	int ret;

	me = (long)arg;
	printf("parent worker %d started up\n",me);

	oper.sem_num = me;
	oper.sem_flg = 0;

	while(1){

		oper.sem_op = -1;
redo1:
		ret = semop(sd1,&oper,1);
		if (ret == -1 && errno != EINTR){
			printf("semop error (1)\n");
			exit(-1);
		}
		if (ret == -1) goto redo1;

redos:
		ret = scanf("%s",memory_segments[me]);
		if (ret == EOF && errno != EINTR){
			printf("scanf error (1)\n");
			exit(-1);
		}
		if (ret == -1) goto redos;

		printf("parent worker - thread %d wrote string %s\n",me,memory_segments[me]);

		oper.sem_op = 1;
redo2:
		ret = semop(sd2,&oper,1);
		if (ret == -1 && errno != EINTR){
			printf("semop error (2)\n");
			exit(-1);
		}
		if (ret == -1) goto redo2;

	}

	return NULL;
}

void *child_worker(void *arg) {

	long me;
	struct sembuf oper;
	int ret;

	me = (long)arg;

	printf("child worker %d started up\n",me);

	oper.sem_num = me;
	oper.sem_flg = 0;

	while(1){

redo1:
		oper.sem_op = -1;
		ret = semop(sd2,&oper,1);
		if (ret == -1 && errno != EINTR){
			printf("semop error (3)\n");
			exit(-1);
		}
		if (ret == -1) goto redo1;

		printf("child worker - thread %d found string %s\n",me,memory_segments[me]);
		fprintf(file,"%s\n",memory_segments[me]);
		fflush(file);

redo2:
		oper.sem_op = 1;
		ret = semop(sd1,&oper,1);
		if (ret == -1 && errno != EINTR){
			printf("semop error (4)\n");
			exit(-1);
		}
		if (ret == -1) goto redo2;

	}

	return NULL;
}


