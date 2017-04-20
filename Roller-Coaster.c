/*
Project for the Operating systems course at FIT BUT
It solves simple roller-coaster problem, input parameters are number of passangers, cart capacity, 
max period for passangers spawning and max time for the cart to finish the ride. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

//Fork + uSleep
#include <unistd.h>
//Semaphore
#include <semaphore.h>

#include <sys/wait.h>
#include <sys/mman.h>

//Random
#include <time.h>

//Macros for memory cleaning
#define UNMAPVARIABLES
	do { 
		munmap(N, sizeof(int)); 
		munmap(I, sizeof(int)); 
		munmap(A, sizeof(int)); 
	} while (0)
						

#define DESTROYSEMS	
	do { 
		sem_destroy(sem_printer);		
		sem_destroy(sem_boarding);		
		sem_destroy(sem_unboarding);	
		sem_destroy(sem_lastboarded);	
		sem_destroy(sem_lastunboarded);	
		sem_destroy(sem_allfinished);	
		sem_destroy(sem_mainfinished);	
	} while (0)

#define UNMAPSEMS 
	do { 
		munmap(sem_printer, sizeof(sem_t));	
		munmap(sem_boarding, sizeof(sem_t));	
		munmap(sem_unboarding, sizeof(sem_t));	
		munmap(sem_lastboarded, sizeof(sem_t));	
		munmap(sem_lastunboarded, sizeof(sem_t));
		munmap(sem_allfinished, sizeof(sem_t));	
		munmap(sem_mainfinished, sizeof(sem_t));
	} while (0)

#define CLOSEFILE 
	do {
		if (fclose(f) == EOF) {
			fprintf(stderr, "%s\n", "Error: Closing file");
		}
	} while (0)

//Check if char * is digit number >= 0
int IsDigit(char *v) {
	int i = 0;
	while (v[i] != '\0') i++;

	for (int j = 0; j < i; j++) {
		if (v[j] > 57 || v[j] < 48)
			return 0;
        }

	return 1;
}

int main(int argc, char **argv) {	
	int P;		//Number of processes representing passangers
	int C;		//Cart capacity
	int PT;		//Max period(ms), for which is the nev passangers process generated
	int RT;		//Max period(ms), to finish the track

	//Opening the file for writing the process output
	FILE *f = fopen("proj2.out", "wb");
	if (f == NULL) {
		fprintf(stderr, "%s\n", "Error: Opening file");
		exit(2);
	}

	//Buffer cleaning, because of processes "fighting" during output
	setbuf(f, NULL); 
	setbuf(stderr, NULL);

	//Input conditions checking (arguments)
	if (argc == 5) {
		if (IsDigit(argv[1]) && IsDigit(argv[2]) && IsDigit(argv[3]) && IsDigit(argv[4])) {
			P = atoi(argv[1]);
			C = atoi(argv[2]);
			PT = atoi(argv[3]);
			RT = atoi(argv[4]);

			if (P > 0 && C > 0 && P > C && (P % C == 0)) {
				if (PT < 0 || PT >= 5001 || RT < 0 || RT >= 5001) {
					CLOSEFILE;

					fprintf(stderr, "%s\n", "Error: Parameters PT or RT have invalid value");
					exit(1);
				}
			} 
				else {
					CLOSEFILE;
					
					fprintf(stderr, "%s\n", "Error: Parameters P or C have invalid value");
					exit(1);
				}
		} 
			else {
				CLOSEFILE;
				
				fprintf(stderr, "%s\n", "Error: One of the arguments have wrong format");
				exit(1);
			}	

	} 
		else {
			CLOSEFILE;
			
			fprintf(stderr, "%s\n", "Error: Wrong number of arguments");
			exit(1);
		}

	//-----------------------------PROGRAM--------------------------------------

	//Variables and smeaphores initialization
	int mainPid;
	int pasPid;
	int carPid;

	sem_t *sem_printer;
	sem_t *sem_boarding;
	sem_t *sem_unboarding;

	sem_t *sem_lastboarded;
	sem_t *sem_lastunboarded;
	sem_t *sem_allfinished;

	sem_t *sem_mainfinished;

	sem_printer = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_boarding = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_unboarding = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_lastboarded = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_lastunboarded = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_allfinished = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sem_mainfinished = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	sem_init(sem_printer, 1, 1);
	sem_init(sem_boarding, 1, 0);
	sem_init(sem_unboarding, 1, 0);
	sem_init(sem_lastboarded, 1, 0);
	sem_init(sem_lastunboarded, 1, 0);
	sem_init(sem_allfinished, 1, 0);
	sem_init(sem_mainfinished, 1, 0);

	//Inicialization of the shared memory
	int *N;
	int *I;
	int *A;
	
	N = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	I = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	A = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	*N = 1;
	*I = 0;
	*A = 1;

	//Fork of the main process to create subprocesses
	if ((mainPid = fork()) < 0) {
		//Memory unmap
		UNMAPVARIABLES;

		//Semaphor destroying
		DESTROYSEMS;

		//Semaphor unmaping from the memory
		UNMAPSEMS;

		//Closing the output file
		CLOSEFILE;

		fprintf(stderr, "%s\n", "Error: fork main process");
		exit(2);
	}
	
	//Subprocess for generating passangers
	if (mainPid == 0) {	
		for (int k = 0; k < P; k++) {	
			//Sleep for passanger
			if (PT != 0)
				usleep(rand() % PT);

			//Vytvorenie pasaziera a osetrenie error-u
			if ((pasPid = fork()) < 0) {
				//Memory unmap
				UNMAPVARIABLES;

				//Semaphor destroying
				DESTROYSEMS;

				//Semaphor unmaping from the memory
				UNMAPSEMS;

				//Closing the output file
				CLOSEFILE;

				fprintf(stderr, "%s\n", "Error: fork subprocess for passengers");
				exit(2);
			}
			
			//If child == pasazier
			if (pasPid == 0) {
				//ID of every passanger
				*I = *I + 1;
				int id = *I;

				//Start of the passanger and waiting for the cart
				sem_wait(sem_printer);
				fprintf(f, "%d %s %d %s\n", *A, ": P ", id, ": started");
				*A += 1;
				sem_post(sem_printer);

				sem_wait(sem_boarding);		//post CAR

				//Start of the cart loading
				sem_wait(sem_printer);
				fprintf(f, "%d %s %d %s\n", *A, ": P ", id, ": board");
				*A += 1;

				//Output of the successful loading to the cart
				if (*N == C) {
					fprintf(f, "%d %s %d %s\n", *A, ": P ", id, ": board last");
					*A += 1;
					*N = 1;
					sem_post(sem_printer);

					//Semaphor for the car, indicating, the cart is full and can start a ride
					sem_post(sem_lastboarded);
				} 
				else {
					fprintf(f, "%d %s %d %s %d\n", *A, ": P ", id, ": board order ", *N);
					*A += 1;
					*N += 1;
					sem_post(sem_printer);
				}

				//Waiting for passangers to exit the cart
				//Waiting for cart to finish the ride
				sem_wait(sem_unboarding);

				//Begining of emptying the cart
				sem_wait(sem_printer);
				fprintf(f, "%d %s %d %s\n", *A, ": P ", id, ": unboard");
				*A += 1;
				//sem_post(sem_printer);

				if (*N == C) {
					fprintf(f, "%d %s %d %s\n", *A, ": P ", id, ": unboard last");
					*A += 1;
					*N = 1;
					sem_post(sem_printer);

					//Semaphor for the cart, all passangers have got off
					sem_post(sem_lastunboarded);
				} 
				else {
					fprintf(f, "%d %s %d %s %d\n", *A, ": P ", id, ": unboard order ", *N);
					*A += 1;
					*N += 1;
					sem_post(sem_printer);
				}

				//Every passanger is waiting until all passangers have finished the ride
				sem_wait(sem_allfinished);

				sem_wait(sem_printer);
				fprintf(f, "%d %s %d %s\n", *A, ": P ", id, ": finished");
				*A += 1;
				sem_post(sem_printer);

				//Semaphor for the main process, so it will end as the last one
				sem_post(sem_mainfinished);

				exit(0);
			}
		}

		exit(0);
	}

	//Fork of the main procces for creating the cart
	if ((carPid = fork()) < 0) {
		//Memory unmap
		UNMAPVARIABLES;

		//Semaphor destroying
		DESTROYSEMS;

		//Semaphor unmaping from the memory
		UNMAPSEMS;

		//Closing the output file
		CLOSEFILE;

		fprintf(stderr, "%s\n", "Error: fork main process");
		exit(2);
	}

	//Subprocess fot the cart
	if (carPid == 0) {	
		
		//Cart initialization
		sem_wait(sem_printer);
		fprintf(f, "%d %s\n", *A, ": C  1 : started");
		*A += 1;
		sem_post(sem_printer);

		//Cycle for "riding" the passangers around the track
		for (int k = 0; k < P/C; k++) {	
			//Start of the passangers entering
			sem_wait(sem_printer);
			fprintf(f, "%d %s\n", *A, ": C  1 : load");
			*A += 1;
			sem_post(sem_printer);

			//Approval for passangers to  get on the cart
			for (int j = 0; j < C; j++)
				sem_post(sem_boarding);

			//Cart is waiting until everyone get on
			sem_wait(sem_lastboarded);

			//Cart starts its ride
			sem_wait(sem_printer);
			fprintf(f, "%d %s\n", *A, ": C  1 : run");
			*A += 1;
			sem_post(sem_printer);
			
			//Sleep -> cart ride
			if (RT != 0) {
				usleep(rand() % RT);
			}

			//Cart has finished its ride, offboarding starts
			sem_wait(sem_printer);
			fprintf(f, "%d %s\n", *A, ": C  1 : unload");
			*A += 1;
			sem_post(sem_printer);

			//Cart allows the passangers to get off
			for (int j = 0; j < C; j++) {
				sem_post(sem_unboarding);
			}

			//Cart is waiting for all the passangers to get off
			sem_wait(sem_lastunboarded);
		}

		//All of the passangers have finished the ride, killing all of the processes at the end (except the main one)
		for (int j = 0; j < P; j++) {
			sem_post(sem_allfinished);
		}

		sem_wait(sem_printer);
		fprintf(f, "%d %s\n", *A, ": C  1 : finished");
		*A += 1;
		sem_post(sem_printer);
		
		//Semaphor for the main process, so it will end as the last one
		sem_post(sem_mainfinished);

		exit(0);
	}

	//Main process is waiting for all passangers to be terminated + the cart
	for (int k = 0; k < P + 1; k++) {
		sem_wait(sem_mainfinished);
	}

	//Terminating the main provess + cleaning the memory
	//Memory unmap
	UNMAPVARIABLES;

	//Semaphor destroying
	DESTROYSEMS;

	//Semaphor unmaping from the memory
	UNMAPSEMS;

	//Closing the output file
	CLOSEFILE;

	//End of the process
	exit(0);
}
