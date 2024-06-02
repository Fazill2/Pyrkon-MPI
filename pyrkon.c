#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include<unistd.h>
#include<stdbool.h>

#define TICKET_REQUEST_TAG 1
#define TICKET_REPLY_TAG 2

#define RELEASED 0
#define HELD 1
#define REQUESTED 2
int rank, size;
int num_tickets;
int local_clock = 0;
int reply_count = 0;
int* ticket_queue;
int ticket_queue_size = 0;

// int[] funZone = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
int ticket_state = RELEASED;




void request_ticket(){
    int i, t;
    MPI_Status status;

    ticket_state = REQUESTED;
    local_clock++;

    for (i = 0; i < size; i++){
        if (i != rank) {
            MPI_Send(&local_clock, 1, MPI_INT, i, TICKET_REQUEST_TAG, MPI_COMM_WORLD);
        }
    }

    reply_count = 0;
    t = local_clock;

    for (;;) {
        MPI_Recv(&local_clock, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int rcvRank = status.MPI_SOURCE;
        int rcvClock = local_clock;
        if (status.MPI_TAG == TICKET_REQUEST_TAG) {
            printf("Proces %d otrzymał request od procesu %d ze znacznikiem czasu %d i własnym znacznikiem %d, porównanie %d\n", rank, rcvRank, rcvClock, t, (ticket_state == REQUESTED && (rcvClock, rcvRank) < (t, rank)));
            if (ticket_state == HELD || (ticket_state == REQUESTED && (rcvClock, rcvRank) < (t, rank))) {
                ticket_queue[ticket_queue_size++] = status.MPI_SOURCE;
                printf("Proces %d dodaje proces %d do kolejki\n", rank, status.MPI_SOURCE);
            } else {
                printf("Proces %d odpowiada procesowi %d\n", rank, status.MPI_SOURCE);
                MPI_Send(&local_clock, 1, MPI_INT, status.MPI_SOURCE, TICKET_REPLY_TAG, MPI_COMM_WORLD);
            }
        } else if (status.MPI_TAG == TICKET_REPLY_TAG) {
            reply_count++;
            printf("Proces %d otrzymał odpowiedź od procesu %d, reply count %d\n", rank, rcvRank, reply_count);
            if (reply_count >= (size - num_tickets)) {
                ticket_state = HELD;
                printf("Proces %d posiada bilet na Pyrkon\n", rank);
                break;
            }
        }
    }
}

void release_ticket() {
    int i;
    ticket_state = RELEASED;
    printf("Proces %d zwalnia bilet na Pyrkon\n", rank);
    for (i = 0; i < ticket_queue_size; i++) {
        MPI_Send(&local_clock, 1, MPI_INT, ticket_queue[i], TICKET_REPLY_TAG, MPI_COMM_WORLD);
    }

    ticket_queue_size = 0;
}



int main(int argc, char **argv)
{
    num_tickets = atoi(argv[1]);
    printf("Liczba biletów na Pyrkon: %d\n", num_tickets);
	MPI_Init(&argc, &argv);
    

    MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ticket_queue = (int*)malloc(size * sizeof(int));

    
    while (true){
        sleep(1);
        if (ticket_state == RELEASED) {
            request_ticket();
            sleep(3); // symulacja robienia czegoś
        } else if (ticket_state == HELD) {
            release_ticket();
            break;
        }
    }
    MPI_Finalize();
}