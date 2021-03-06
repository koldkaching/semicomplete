#include "mpi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void master();
void slave();

#include "log.h"

char myhost[150];
int myrank;

#define SLEEP 1
#define ITERATIONS 10

void debug(const char *format, ...) {
	char str[1024];
	char msg[1024];
	char host[150];
	va_list args;

	/* Don't debug right now */
	return;

	va_start(args,format);
	vsnprintf(msg, 1024, format, args);
	va_end(args);

	printf("%s[%d]: %s\n", myhost, myrank, msg);
	fflush(stdout);
}


int main(int argc, char **argv) {
	int rank;
	char host[150];
	int namelen;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(host,&namelen);

	strcpy(myhost,host);
	myrank = rank;
	debug("Hello World!", rank, host);
	if (rank == 0) master();
	else slave();

	debug("Finalizing!");
	MPI_Finalize();
	return 0;
}

void master() {
	int nodes;
	int count;
	int result;

	MPI_Comm_size(MPI_COMM_WORLD, &nodes);
	debug("EXAMPLE VALUE: %d", 15);

	//for (count = 0; count < ITERATIONS; count++) {
	for (;;) {
		int x = 0;
		MPI_Status status;
		debug("Waiting for data...");
		for (x = 1; x < nodes; x++) {
			MPI_Recv(&result, 1, MPI_INT, x, 1, MPI_COMM_WORLD, &status);
			result = 0;
			MPI_Send(&result, 1, MPI_INT, x, 1, MPI_COMM_WORLD);
			debug("Got %d from %d", result, status.MPI_SOURCE);
		}
	}
	debug("Master finished");
}

void slave() {
	int value;
	//for (value = 0; value < ITERATIONS; value++) {
	for(value = 0;;value++) {
		MPI_Status status;
		debug("Sending %d", value); 
		MPI_Send(&value, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
		MPI_Recv(&value, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
		debug("Got data from master: %d", value);
	}

	if (myrank % 10 == 0) {
		for (;;) { /* Never exit */
			MPI_Status status;
			debug("Sending %d", value); 
			MPI_Send(&value, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
			value++;
			debug("Sleeping...");
		}
	}
}
	


