#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> 
#include <sys/time.h>

#define NUM_THREADS 2

const char *numturns = NULL;
char *stringify(int throw);
int find_winner(int throw1, int throw2);
pthread_t thread1, thread2;
int p1 = 0;
int p2 = 1;
pthread_mutex_t mutexA = PTHREAD_MUTEX_INITIALIZER; // sync cmd_cv to shared variables message/command
pthread_mutex_t mutexB = PTHREAD_MUTEX_INITIALIZER; // sync throw_cv to reads/plays/throws
pthread_cond_t cmd_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t throw_cv = PTHREAD_COND_INITIALIZER;
int wins[3]; // shared memory variable array to tally winners
int throws[2]; // shared memory variable array to store throws by threads
int rc, winner, message, command, reads, plays;
pid_t getpid(void);

/*
 * function that finds winner based on throws[] array
 * returns: -1 for TIE, 0 for LOSS, 1 for WIN
 */ 
int find_winner(int throw1, int throw2) {

	if (throw1 == throw2) // TIE
		return -1;
	else if ((throw1 == 0) && (throw2 == 2)) // ROCK BEATS SCISSORS
		return 1;
	else if ((throw1 == 2) && (throw2 == 0)) // SCISSORS DEFEATED BY ROCK
		return 0;
	else
		return (throw1 > throw2) ? 1 : 0; // handles other cases
}

/*
 * function that turns random integer into a throw
 * returns ROCK, PAPER, or SCISSORS
 */
char *stringify(int throw) {

	if (throw == 0)
		return "Rock";
	else if (throw == 1)
		return "Paper";
	else if (throw == 2)
		return "Scissors";
	else
		return "print error";
}

/*
 * function that syncronizes both threads with locks and conditions
 * modifies throws[] array and shared variables to repeat a wait-throw-wait process until signaled to exit by referee()
 */
void *players(void *threadid) {

	int pid;
	pid = *(int *)threadid;
	// setting up for a effective random number generator
	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec + tv.tv_usec + getpid());


	pthread_mutex_lock(&mutexA);

	while (reads == 0) {
		// printf("waiting for reads != 0\n");
		pthread_cond_wait(&cmd_cv, &mutexA);
	}

	message = command; // READ FIRST COMMAND
	reads--;

	pthread_mutex_unlock(&mutexA);


	while ( message == 1 ) { 

		pthread_mutex_lock(&mutexB);

		// make throw
		throws[pid] = rand() % 3;
		plays++;

		pthread_cond_signal(&throw_cv);

		pthread_mutex_unlock(&mutexB);

		pthread_mutex_lock(&mutexA);

		pthread_cond_wait(&cmd_cv, &mutexA);

		message = command; // READ NEXT COMMAND

		pthread_mutex_unlock(&mutexA);

	}

	return(NULL);
}

void referee(int rounds) {


	printf("Beginning %d rounds...\nFight!\n", rounds);
	printf("Child 1 PID: %d\n", (unsigned int) thread1);
	printf("Child 1 PID: %d\n", (unsigned int) thread2);


	for(int i = 0; i < rounds; i++) {

		pthread_mutex_lock(&mutexA);

		command = 1;
		reads = 2;

		pthread_cond_broadcast(&cmd_cv);

		pthread_mutex_unlock(&mutexA);

		// after player plays

		pthread_mutex_lock(&mutexB);


		while ( plays < 2 ) {
			pthread_cond_wait(&throw_cv, &mutexB);
		}

		printf("-------------------------\n");
		printf("Round: %d\n", i+1);

		printf("Child %d throws %s!\n", 1, stringify(throws[0]));
		printf("Child %d throws %s!\n", 2, stringify(throws[1]));

		winner = find_winner(throws[0], throws[1]);
		plays = 0;

		pthread_mutex_unlock(&mutexB);

		// tally the number of winners
		// element 0 is TIE, 1 is CHILD1, 2 is CHILD2
		if (winner == -1) {
			printf("Game is a Tie!\n");
			wins[0] += 1;
		}
		else if (winner == 0) {
			printf("Child 2 Wins!\n");
			wins[2] += 1;
		}
		else if (winner == 1) {
			printf("Child 1 Wins!\n");
			wins[1] += 1;
		}

		pthread_mutex_unlock(&mutexB);
	}


	// send stop message to children here
	pthread_mutex_lock(&mutexA);

	command = -1;

	pthread_cond_broadcast(&cmd_cv);

	pthread_mutex_unlock(&mutexA);

}

int main (int argc, const char **argv) {

	// initialize throws[] shared array to -1 default throws
	for(int i = 0; i < sizeof(throws)/4; i++)
		throws[i] = -1;

	// intialize wins shared array to 0 default values
	wins[0] = 0; wins[1] = 0; wins[2] = 0;

	// error handling for user input
	if(argc != 2) {
		fprintf(stderr, "Error: enter one number when executing to indicate number of rounds.\n");
		return 1;
	} else if (strlen(argv[1]) != 1) {
		fprintf(stderr, "invalid argument '%s'\n", argv[1]);
		return 2;
	}

	// create two threads and error handling
	rc = pthread_create(&thread1, NULL, players, &p1);
	if (rc) {
		perror("pthread_create()");
	}
	rc = pthread_create(&thread2, NULL, players, &p2);
	if (rc) {
		perror("pthread_create()");
	}

	// grab number of rounds from user
	numturns = argv[1];

	// call referee function
	referee(atoi(numturns));

	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);

	// print results
	printf("------------------------\n");
	printf("------------------------\n");
	printf("Results:\n");
	printf("Child 1: %d\n", wins[1]);
	printf("Child 2: %d\n", wins[2]);
	printf("Ties: %d\n", wins[0]);

	// compute winner of tournament
	if (wins[0] == atoi(numturns))
		printf("It's a Draw!\n");
	else
		printf("Child %d Wins!\n", (wins[1] > wins[2]) ? 1 : 2);

	pthread_mutex_destroy(&mutexA);
	pthread_mutex_destroy(&mutexB);  
	pthread_cond_destroy(&cmd_cv);
	pthread_cond_destroy(&throw_cv);
	pthread_exit(NULL);
	return 0;
}
