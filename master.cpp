/*
 * Author: Brandon Lodnon
 * Date: 09/28/2020
 * Prof: Bhatia
 * Course: 4760
 */

//Includes for functions
#include <sys/time.h>
#include <unistd.h>
#include <fstream> 
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <string>
#include <cstring>

#define KEY 0x1111
#define BUFFER_LENGTH 1024

//So I dont have to use annoying std everywhere
using namespace std;

//Shared memory struct that will be used to pass data between files
struct shared_memory {
	int count;
	char data[20][256];
	int slaveProcessGroup;
};

int shmid;
struct shared_memory *shmptr;

int semid;

void trySpawnChild(int count); //spawns child if less than 20 processes are in the system
void spawn(int count); //helper function of spawnChild for code simplicity
void sigHandler(int SIG_CODE); //Signal handler for master to handle Ctrl+C interrupt
void timerSignalHandler(int); //Signal handler for master to handle time out
void allocateSHM();
void releaseSHM();
void allocateSEM();
void releaseSEM();

//Parent Interrupt function for timer
void parentInterrupt(int seconds);
//Timers decrement from it_value to zero, generate a signal, and reset to it_interval.
void timer(int seconds);

const int MAX_NUM_OF_PROCESSES_IN_SYSTEM = 20; // Hardcap for number of processes ever allowed in system. This will never change 
int currentConcurrentProcessesInSystem = 0;// Holds/keeps track of current processes in system
int maxTotalProcessesInSystem = 4; //Softcap set by user to hold max processes ever allowed in system (default 4)
int maxConcurrentProcessesInSystem = 2; // Cap set by user to hold the max number of processes ever in the system at any given time (default 2)
int durationBeforeTermination = 100; // Cap set by user to control when master will terminate in seconds (default 100)
int shmSegmentID; //Segment id for shared memory

int status = 0; //used for wait status in spawnChild function
int count = 0;
int startTime; //will hold time right before forking starts, used in main and timer signal handler


int main(int argc, char** argv){
	signal(SIGINT, sigHandler);
	

	int c;// for getopt
	while((c = getopt(argc, argv, "hn:s:t:")) != -1) {
		switch(c){
			case 'h':// Help command
				
				cout<< "USAGE: " << endl;
				cout<< "./master -h for help command" << endl;
				cout<< "./master [-n x] [-s x] [-t time] infile" << endl;
				cout<< "PARAMETERS: " << endl;
				cout<< "-h Describes how to project should be run and then terminate." << endl;
				cout<< "-n x Indicate the maximum total of child processes master will ever create. (Default 4)" << endl;
				cout<< "-s x Indicate the number of children allowed to exist in the system at the same time (Default 2)" << endl;
				cout<< "-t time The time in seconds after which the processes will terminate, even if it has not finished (Default 100)" << endl;
				cout<< "infile Input file containing strings to be tested" << endl;
				exit(0);
			break;
			
			case 'n'://Max Processes softcap
				maxTotalProcessesInSystem = atoi(optarg);
				if((maxTotalProcessesInSystem <= 0) || maxTotalProcessesInSystem > MAX_NUM_OF_PROCESSES_IN_SYSTEM){
					perror("MaxProcesses: Cannot be Zero or greater than 20");
					exit(1);
				}
			break;
			case 's':// max concurrent processes softcape
				maxConcurrentProcessesInSystem = atoi(optarg);
				if(maxConcurrentProcessesInSystem <= 0){
					perror("maxConcurrentProcessesInSystem: Cannot be negative or zero");
					exit(1);
				}		
			break;
			case 't':// master Termination softcap set by user 
				durationBeforeTermination = atoi(optarg);
				if(durationBeforeTermination <= 0) {
					perror("durationBeforeTermination: Master cannot have a negative or zero duration");
					exit(1);
				}
			break;
			
			default:
				perror("NOT VALID PARAM: Use -h for Help");
				cout<< "USAGE: " << endl;
				cout<< "./master -h for help command" << endl;
				cout<< "./master [-n x] [-s x] [-t time] infile" << endl;
				exit(1);
			break;
						
				
		}
	}

/*=====================================================================================*/
	//setvbuf(stdout, NULL, _IONBF, 0);
	//setvbuf(stderr, NULL, _IONBF, 0);

	allocateSHM();
	allocateSEM();
/*=====================================================================================*/
	//Start timer for Termination
	parentInterrupt(durationBeforeTermination);

	//Try and open the file given, If its not found then exit
	int i = 0;
	FILE *fp = fopen(argv[optind], "r");
	if(fp == 0)
	{
		perror("fopen: File not found");
		exit(1);
	}
	// allocate a string to hold each line and put it into shared memory to be passed into palin
	char line[256];

	while(fgets(line, sizeof(line), fp) != NULL) {
		line[strlen(line) - 1] = '\0';
		printf("%s\n", line);
		strcpy(shmptr->data[i], line);
		i++;
	}
	
	//check to see if the max processes is greater than lines if so, change the softcap and set it equal to number of lines
	if(i < maxTotalProcessesInSystem){
		maxTotalProcessesInSystem = i;
	}
	//check and make sure it doesnt go over hardcap
	if (maxTotalProcessesInSystem < maxConcurrentProcessesInSystem) {
		maxConcurrentProcessesInSystem = maxTotalProcessesInSystem;
	}
	
	//pass in max total processes to sharedMemory
	shmptr->count = maxTotalProcessesInSystem;
	// check and see if maxprocesses has been reached if so, your done, if not, try and spawn a child
	while(count < maxConcurrentProcessesInSystem){
	 	trySpawnChild(count++);
	 }

	 //wait for all child processes to finish or time to run out, then free up memory and close
	while(currentConcurrentProcessesInSystem > 0){
	 	wait(NULL);
	 	--currentConcurrentProcessesInSystem;
		trySpawnChild(count++);
	}
	
	
/*=====================================================================================*/
	releaseSHM();
	releaseSEM();
/*=====================================================================================*/

	return 0;
}

//spawns child if less than 20 processes are in the system
void trySpawnChild(int count){ 
	if((currentConcurrentProcessesInSystem < maxConcurrentProcessesInSystem) && (count < maxTotalProcessesInSystem)){
			spawn(count);
	}
	else{
		count--;
	}
}

//Spawns children, assigns them a group ID, and then have them go into palin
void spawn(int count){
	++currentConcurrentProcessesInSystem;
	if(fork() == 0){
	 	if(count == 1) shmptr->slaveProcessGroup = getpid();
	 	setpgid(0, shmptr->slaveProcessGroup);
		char buf[256];
		sprintf(buf, "%d", count + 1);
		execl("./palin", "palin", buf, (char*) NULL);
		exit(0);
	 }
}

//This is my signal handler to kill all processes when cntrl +c is called. It just kills anything with the slave process ID which is always given atspawn
void sigHandler(int signal){
	killpg(shmptr->slaveProcessGroup, SIGTERM);
	int status;

	//THIS IS NOT NECCASSARY BUT I LIKE TO BE NOTIFIED IF A PROCESSES ISNT KILLED CORRECTLY
	while (wait(&status) > 0) {
		if (WIFEXITED(status)) printf("OK: Child exited with exit status: %d\n", WEXITSTATUS(status));
		else printf("ERROR: Child has not terminated correctly\n");
	}
	releaseSHM();
	releaseSEM();
	cout << "Exiting master process" << endl;
	exit(0);
}


//Used to count down and when it hits zero, send a signal
void parentInterrupt(int seconds)
{
	timer(seconds);

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = &sigHandler;
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGALRM, &sa, NULL) == -1)
	{
		perror("ERROR");
	}
}


void timer(int seconds)
{
	//Timers decrement from it_value to zero, generate a signal, and reset to it_interval.
	//	//A timer which is set to zero (it_value is zero or the timer expires and it_interval is zero) stops.
	struct itimerval value;
	value.it_value.tv_sec = seconds;
	value.it_value.tv_usec = 0;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	if(setitimer(ITIMER_REAL, &value, NULL) == -1)
	{
		perror("ERROR");
	}
}

void allocateSEM(){
	if((semid = semget(KEY, 1, 0666 | IPC_CREAT | IPC_EXCL)) == -1){
		perror("semget");
		exit(1);
	}
	if((semctl(semid, 0, SETVAL, 1) == -1)){
		perror("semctl");
		exit(1);
	}
}

void releaseSEM(){
	if(semctl(semid, 0, IPC_RMID) == -1) {
		perror("semctl");
		exit(1);
	}
}

void allocateSHM(){
	if((shmid = shmget(KEY, sizeof(struct shared_memory), 0666 | IPC_CREAT | IPC_EXCL)) == -1){
		perror("shmget");
		exit(1);
	} else {
		shmptr = (struct shared_memory*) shmat(shmid, NULL, 0);
	}
}

void releaseSHM(){
	if(shmptr != NULL) {
		if (shmdt(shmptr) == -1) {
			perror("shmdt");
			exit(1);
		}			
	}
	if(shmid > 0) {
		if(shmctl(shmid, IPC_RMID, NULL) == -1){
			perror("shmctl");
		} 
	}
}

