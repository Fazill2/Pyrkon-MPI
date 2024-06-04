#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include<unistd.h>
#include<stdbool.h>

#define TICKET_REQUEST_TAG 1
#define TICKET_REPLY_TAG 2
#define TRAINING_REQUEST_TAG 3
#define TRAINING_REPLY_TAG 4

#define RELEASED 0
#define HELD 1
#define REQUESTED 2
int rank, size;
int num_tickets;
int local_clock = 0;
int reply_count = 0;
int* ticket_queue;
int ticket_queue_size = 0;
int num_of_trainings;
int* trainings;
int training_queue_size = 0;
int* training_queue;
int* training_indices;
// int[] funZone = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
int ticket_state = RELEASED;

MPI_Datatype MPI_PAKIET_T;

typedef struct {
    int ts;
    int src;
    int data;
} packet_t;

int t;
bool pyrkon_done = false;
bool process_done_with_pyrkon = false;

bool compare_timestamps(int ts1, int src1, int ts2, int src2) {
    return (ts1 < ts2) || (ts1 == ts2 && src1 < src2);
}

void request_ticket(){
    int i;
    MPI_Status status;

    ticket_state = REQUESTED;
    local_clock++;

    packet_t packet = {local_clock, rank, 0};
    for (i = 0; i < size; i++){
        if (i != rank) {
            MPI_Send(&packet, 1, MPI_PAKIET_T, i, TICKET_REQUEST_TAG, MPI_COMM_WORLD);
        }
    }
    reply_count = 0;
    t = local_clock;
}

void release_ticket() {
    int i;
    ticket_state = RELEASED;
    printf("Proces %d zwalnia bilet na Pyrkon\n", rank);
    packet_t send_packet = {local_clock, rank, 0};
    for (i = 0; i < ticket_queue_size; i++) {
        MPI_Send(&send_packet, 1, MPI_PAKIET_T, ticket_queue[i], TICKET_REPLY_TAG, MPI_COMM_WORLD);
    }

    ticket_queue_size = 0;
}

void pyrkon() {
    if (ticket_state == RELEASED) {
        request_ticket();
        t = local_clock;
    } else if (ticket_state == HELD && process_done_with_pyrkon) {
        release_ticket();
    } else if (ticket_state == HELD && !process_done_with_pyrkon){
        printf("Proces %d jest na Pyrkonie\n", rank);
        sleep(5);
        process_done_with_pyrkon = true;
    } else {
        MPI_Status status;
        packet_t packet;
        MPI_Recv(&packet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int rcvRank = packet.src;
        int rcvClock = packet.ts;
        int data = packet.data;
        if (status.MPI_TAG == TICKET_REQUEST_TAG) {
            printf("Proces %d otrzymał request od procesu %d ze znacznikiem czasu %d i własnym znacznikiem %d, porównanie %d\n", rank, rcvRank, rcvClock, t, (ticket_state == REQUESTED && (rcvClock, rcvRank) < (t, rank)));
            if (ticket_state == HELD || (ticket_state == REQUESTED && compare_timestamps(t, rank, rcvClock, rcvRank))) {
                ticket_queue[ticket_queue_size++] = status.MPI_SOURCE;
                printf("Proces %d dodaje proces %d do kolejki\n", rank, status.MPI_SOURCE);
            } else {
                printf("Proces %d odpowiada procesowi %d\n", rank, status.MPI_SOURCE);
                packet_t send_packet = {local_clock, rank, 0};
                // MPI_Send(&local_clock, 1, MPI_INT, status.MPI_SOURCE, TICKET_REPLY_TAG, MPI_COMM_WORLD);
                MPI_Send(&send_packet, 1, MPI_PAKIET_T, status.MPI_SOURCE, TICKET_REPLY_TAG, MPI_COMM_WORLD);
            }
        } else if (status.MPI_TAG == TICKET_REPLY_TAG) {
            reply_count++;
            printf("Proces %d otrzymał odpowiedź od procesu %d, reply count %d\n", rank, rcvRank, reply_count);
            if (reply_count >= (size - num_tickets)) {
                ticket_state = HELD;
                printf("Proces %d posiada bilet na Pyrkon\n", rank);
                sleep(5);
            }
        }
    }
}



int main(int argc, char **argv)
{
    num_tickets = atoi(argv[1]);
    printf("Liczba biletów na Pyrkon: %d\n", num_tickets);
    printf("Liczba warsztatów: %d\n", size);
	MPI_Init(&argc, &argv);
    const int nitems=3; /* bo packet_t ma trzy pola */
    int       blocklengths[3] = {1,1,1};
    MPI_Datatype typy[3] = {MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint     offsets[3]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);


    MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ticket_queue = (int*)malloc(size * sizeof(int));

    
    while (true){
        sleep(1);
        pyrkon();
    }
    MPI_Finalize();
}