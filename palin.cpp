/**
Author: Brandon London
Course: 4760
Prof: Bhatia
Date: 09/30/20
*/

//======================================================================================Includes=============================================================================
#include <iostream>
#include <unistd.h>
#include <ctime>
#include <fstream> 
#include <cstdlib>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
//=========================================================================================================================================================================
using namespace std;

//Struct to share data and shared memory between files
struct shared_memory {
	int count;
        char data[20][256];
	int slaveProcessGroup;
};

//Set state for critical section, se we can see a status for who wants in ect.
enum state { idle, want_in, in_cs };
#define KEY 0x1111
#define BUFFER_LENGTH 1024
void allocateSHM();
void allocateSEM();
void semWait();
void semSignal();
//Make sure we have a handler incase of cntrl +c
void sigHandler(int);

//Function to get local time. Nothing more, nothing less
char* getFormattedTime(); 

//process id, used by multiple functions, keeps track of processes
int id; 
int shmid;
struct shared_memory *shmptr;

int semid;
struct sembuf p = { 0, -1, 0 };
struct sembuf v = { 0, +1, 0 };
int main(int argc, char ** argv){
	//Singal for cntrl and termination
	signal(SIGTERM, sigHandler);
	//Initialize index
	int index;

	if(argc < 2){ //If no arguments were supplied, id must not be set
		perror("No argument supplied for id");
		exit(1);
	}
	else{
		id = atoi(argv[1]);
		index = id - 1;
	}
	
	srand(time(0) + id); //generate different delays each run

	int N; //number of slave processes master will spawn

	//Generate the key, will be same as master
	//Grab shared mem
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	allocateSHM();
	allocateSEM();

	//Number of slaves master will spawn
	N = shmptr->count;
	
	printf("%s: Process %d wants in critical section\n", getFormattedTime(), id);
//TELLS IF ITS A PALINDROME OR NOT
//========================================================================================
	int l = 0;
	int r = strlen(shmptr->data[id-1]) - 1;
	bool palin = true;

	while(r > l)
	{
		if(tolower(shmptr->data[index][l]) != tolower(shmptr->data[index][r])) {
			palin = false;
			break;
		}
		l++;
		printf("%s\n", "hhhhh");
		r--;
	}
//========================================================================================
	semWait();
/*============================================================!!!!!!!!!!!!!!!!Critical section!!!!!!!!!!!!!!!!!!!!!!!============================================================ */

	
	printf("%s: Process %d in critical section\n", getFormattedTime(), id);

	//sleep for random amount of time (between 0 and 2 seconds);
	sleep(rand() % 3);

//WRITES TO PALIN.OUT AND NONPALIN.OUT AND LOGFILE
	FILE *file = fopen(palin ? "palin.out" : "nopalin.out", "a+");
	if (file == NULL) {
		perror("");
		exit(1);
	}
	fprintf(file, "%s\n", shmptr->data[id - 1]);
	printf("%s\n", shmptr->data[id - 1]);
	fclose(file);
	
	//Write to log file, all processes, time and ids
	file = fopen("output.log", "a+");
	if (file == NULL) {
		perror("");
		exit(1);
	}
	fprintf(file, "%s %d %d %s\n", getFormattedTime(), getpid(), id - 1, shmptr->data[id -1]);
	fclose(file);
	
/*=======================================================================================exit from critical section;=================================================== */

//	cerr << getFormattedTime() << ": Process " << id << " exiting critical section\n";

	printf("%s: Process %d exiting critical section\n", getFormattedTime(), id);
	//sleep for random amount of time (between 0 and 2 seconds);
	semSignal();

	return 0;
}

//Waits for a signal. Kept print f for debugging
void sigHandler(int signal) {
	if (signal == SIGTERM) {
		printf("In Signal Handler Function\n");
		exit(1);
	}
}


//Again gets time, nothing more, nothing less
char* getFormattedTime(){
	int timeStringLength;
	string timeFormat;

	timeStringLength = 9;
	timeFormat = "%H:%M:%S";

	time_t seconds = time(0);

	struct tm * ltime = localtime(&seconds);
	char *timeString = new char[timeStringLength];

	strftime(timeString, timeStringLength, timeFormat.c_str(), ltime);

	return timeString;
}
void allocateSEM(){
	if((semid = semget(KEY, 1, 0666 )) == -1){
		perror("semget");
		exit(1);
	}
}

void allocateSHM(){
	if((shmid = shmget(KEY, sizeof(struct shared_memory), 0666 )) == -1){
		perror("shmget");
		exit(1);
	} else {
		shmptr = (struct shared_memory*) shmat(shmid, NULL, 0);
	}
}

void semWait() {
	if (semop(semid, &p, 1) == -1) {
		perror("semop");
		exit(1);
	}
}

void semSignal() {
	if (semop(semid, &v, 1) == -1) {
		perror("semop");
		exit(1);
	}
}
