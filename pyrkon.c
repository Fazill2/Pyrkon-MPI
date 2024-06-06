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
// liczba biletów na Pyrkon
int num_tickets;
// zegar Lamporta
int local_clock = 0;
// liczba odpowiedzi na request biletu
int reply_count = 0;
// kolejka procesów czekających na bilet
int* ticket_queue;
// rozmiar kolejki procesów czekających na bilet
int ticket_queue_size = 0;
// request id procesów czekających na warsztat
int* ticket_request_ids;
// liczba warsztatów
int num_of_trainings;
// tablica z rozmiarami warsztatów
int* trainings;
// kolejka procesów czekających na warsztat
int* training_queue;
// rozmiar kolejki procesów czekających na warsztat
int training_queue_size = 0;
// indeksy warsztatów, które proces ma zamiar odwiedzić
int* training_indices;
// indeks aktualnie odwiedzanego warsztatu
int training_index;
// stan trenowania
int training_state = RELEASED;
// stan biletu na Pyrkon
int ticket_state = RELEASED;
// timestamp ostatniego requestu
int last_request = 0;

MPI_Datatype MPI_PAKIET_T;

// struktura pakietu
typedef struct {
    int ts;
    int src;
    int data;
    int requestId;
} packet_t;

// znacznik czasu
int t;
// czy pyrkon się skończył
bool pyrkon_done = false;
// czy proces zakończył swoje działanie na Pyrkonie
bool process_done_with_pyrkon = false;

// porównanie znaczników czasu, w przypadku równości porównanie numerów procesów
bool compare_timestamps(int ts1, int src1, int ts2, int src2) {
    return (ts1 < ts2) || (ts1 == ts2 && src1 < src2);
}

void request_ticket(){
    int i;
    MPI_Status status;

    ticket_state = REQUESTED;
    local_clock++;
    last_request = local_clock;
    reply_count = 0;
    packet_t packet = {local_clock, rank, 0, last_request};
    for (i = 0; i < size; i++){
        if (i != rank) {
            MPI_Send(&packet, 1, MPI_PAKIET_T, i, TICKET_REQUEST_TAG, MPI_COMM_WORLD);
        }
    }
    t = local_clock;
}

void release_ticket() {
    int i;
    ticket_state = RELEASED;
    printf("Proces %d zwalnia bilet na Pyrkon\n", rank);
    packet_t send_packet = {local_clock, rank, 0};
    for (i = 0; i < ticket_queue_size; i++) {
        packet_t send_packet = {local_clock, rank, 0, ticket_request_ids[i]};
        // printf("Request id: %d\n", send_packet.requestId);
        MPI_Send(&send_packet, 1, MPI_PAKIET_T, ticket_queue[i], TICKET_REPLY_TAG, MPI_COMM_WORLD);
    }
    ticket_request_ids = (int*)realloc(ticket_request_ids, 1 * sizeof(int));
    ticket_queue = (int*)realloc(ticket_queue, 1 * sizeof(int));

    ticket_queue_size = 0;
}

void pyrkon() {
    if (ticket_state == RELEASED) {
        request_ticket();
        t = local_clock;
    } else if (ticket_state == HELD && process_done_with_pyrkon) {
        local_clock++;
        release_ticket();
        process_done_with_pyrkon = false;
    } else if (ticket_state == HELD && !process_done_with_pyrkon){
        printf("Proces %d jest na Pyrkonie\n", rank);
        local_clock++;
        sleep(5);
        process_done_with_pyrkon = true;
    } else {
        MPI_Status status;
        packet_t packet;
        MPI_Recv(&packet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int rcvRank = packet.src;
        int rcvClock = packet.ts;
        int data = packet.data;
        int requestId = packet.requestId;
        // printf("Timestamp: %d, src: %d, data: %d, requestId: %d\n", rcvClock, rcvRank, data, requestId);
        // printf("Received request id: %d\n", requestId);
        local_clock = (rcvClock > local_clock ? rcvClock : local_clock) + 1;
        if (status.MPI_TAG == TICKET_REQUEST_TAG) {
            // printf("Proces %d otrzymał request od procesu %d ze znacznikiem czasu %d i własnym znacznikiem %d, porównanie %d\n", rank, rcvRank, rcvClock, last_request, (ticket_state == REQUESTED && compare_timestamps(last_request, rank, rcvClock, rcvRank)));
            if (ticket_state == HELD || (ticket_state == REQUESTED && compare_timestamps(last_request, rank, rcvClock, rcvRank))) {
                ticket_request_ids = (int*)realloc(ticket_request_ids, (ticket_queue_size + 1) * sizeof(int));
                ticket_queue = (int*)realloc(ticket_queue, (ticket_queue_size + 1) * sizeof(int));
                ticket_request_ids[ticket_queue_size] = requestId;
                ticket_queue[ticket_queue_size] = rcvRank;
                ticket_queue_size++;
                
                // printf("Proces %d dodaje proces %d do kolejki\n", rank, status.MPI_SOURCE);
            } else {
                // printf("Proces %d odpowiada procesowi %d\n", rank, status.MPI_SOURCE);
                packet_t send_packet = {local_clock, rank, 0, requestId};
                MPI_Send(&send_packet, 1, MPI_PAKIET_T, status.MPI_SOURCE, TICKET_REPLY_TAG, MPI_COMM_WORLD);
            }
        } else if (status.MPI_TAG == TICKET_REPLY_TAG) {
            if (requestId == last_request) {
                reply_count++;
                // printf("Proces %d otrzymał odpowiedź od procesu %d, reply count %d\n", rank, rcvRank, reply_count);
                if (reply_count >= (size - num_tickets)) {
                    ticket_state = HELD;

                    printf("Proces %d posiada bilet na Pyrkon\n", rank);
                    sleep(5);
                }
            }
        }
    }
}



int main(int argc, char **argv)
{
    num_tickets = atoi(argv[1]);
    // num_of_trainings = atoi(argv[2]);
    // trainings = (int*)malloc(num_of_trainings * sizeof(int));
    // training_queue = (int*)malloc(size * sizeof(int));
    // training_indices = (int*)malloc(1 * sizeof(int));
    // for (int i = 0; i < num_of_trainings; i++) {
    //     trainings[i] = atoi(argv[3]);
    // }
    // printf("Liczba biletów na Pyrkon: %d\n", num_tickets);
    // printf("Liczba warsztatów: %d\n", size);
	MPI_Init(&argc, &argv);
    const int nitems=4; /* bo packet_t ma trzy pola */
    int       blocklengths[4] = {1,1,1,1};
    MPI_Datatype typy[4] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint     offsets[4]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
    offsets[3] = offsetof(packet_t, requestId); 


    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);


    MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ticket_queue = (int*)malloc(size * sizeof(int));
    ticket_request_ids = (int*)malloc(1 * sizeof(int));

    
    while (true){
        pyrkon();
    }
    MPI_Finalize();
}