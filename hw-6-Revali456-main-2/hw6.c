#include "elevator.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define NSLOTS 1000


struct queue {
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int buffer[NSLOTS];
    int nitems;
    int front;
    int rear;
    int all_passengers_done;  // Add this flag
}; struct queue JOBQ;

static int passengers_completed = 0;
static pthread_mutex_t completion_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t movement_lock = PTHREAD_MUTEX_INITIALIZER;


void signal_all_passengers_done(void) {
    pthread_mutex_lock(&JOBQ.lock);
    JOBQ.all_passengers_done = 1;
    pthread_cond_broadcast(&JOBQ.not_empty);  // Wake up any waiting elevators
    pthread_mutex_unlock(&JOBQ.lock);
}


void qinit(void) {
    memset(&JOBQ, 0, sizeof(struct queue));
    pthread_mutex_init(&JOBQ.lock, NULL);
    pthread_cond_init(&JOBQ.not_empty, NULL);
    pthread_cond_init(&JOBQ.not_full, NULL);
    JOBQ.all_passengers_done = 0; 
}

void qinsert(int item) {
    pthread_mutex_lock(&JOBQ.lock);
    while (JOBQ.nitems == NSLOTS) {
        pthread_cond_wait(&JOBQ.not_full, &JOBQ.lock);
    }
    JOBQ.buffer[JOBQ.rear] = item;
    JOBQ.rear = (JOBQ.rear + 1) % NSLOTS;
    JOBQ.nitems += 1;
    pthread_cond_signal(&JOBQ.not_empty);
    pthread_mutex_unlock(&JOBQ.lock);
}

int qremove(void) {
    pthread_mutex_lock(&JOBQ.lock);
    
    // If no items and all passengers done, return special value
    if (JOBQ.nitems == 0) {
        pthread_mutex_lock(&completion_lock);
        if (passengers_completed >= PASSENGERS) {
            pthread_mutex_unlock(&completion_lock);
            pthread_mutex_unlock(&JOBQ.lock);
            return -1;
        }
        pthread_mutex_unlock(&completion_lock);
    }
    
    while (JOBQ.nitems == 0) {
        pthread_cond_wait(&JOBQ.not_empty, &JOBQ.lock);
        
        // Check again after waking up
        if (JOBQ.nitems == 0) {
            pthread_mutex_lock(&completion_lock);
            if (passengers_completed >= PASSENGERS) {
                pthread_mutex_unlock(&completion_lock);
                pthread_mutex_unlock(&JOBQ.lock);
                return -1;
            }
            pthread_mutex_unlock(&completion_lock);
        }
    }

    int item = JOBQ.buffer[JOBQ.front];
    JOBQ.front = (JOBQ.front + 1) % NSLOTS;
    JOBQ.nitems -= 1;
    pthread_cond_signal(&JOBQ.not_full);
    pthread_mutex_unlock(&JOBQ.lock);
    return item;
}


typedef struct {
    int id;
    int current_floor;
    int is_available;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Elevator;


Elevator elevators[ELEVATORS];

// Passenger struct
typedef struct {
    int id;
    int from_floor;
    int to_floor;
    int passenger_ready_for_pickup;
    int passenger_in_elevator;
    int passenger_exited_elevator;
    int elevator_at_pickup;
    int elevator_at_destination;
    int reset_flag;
    int assigned_elevator_id;  
    pthread_mutex_t lock;
    pthread_cond_t cond_pickup;
    pthread_cond_t cond_in_elevator;
    pthread_cond_t cond_exit;
} Passenger;

static int passengers_ridden = 0;

static pthread_mutex_t passenger_lock = PTHREAD_MUTEX_INITIALIZER;
// Array of passengers
#define MAX_PASSENGERS 10
Passenger passengers[PASSENGERS];

// Global elevator lock
pthread_mutex_t elevator_lock;

// Scheduler initialization
void scheduler_init(void) {
    pthread_mutex_init(&elevator_lock, NULL);
    qinit();
    
    // Reset completion counter
    passengers_completed = 0;
    
    // Initialize elevators
    for (int i = 0; i < ELEVATORS; i++) {
        elevators[i].id = i;
        elevators[i].current_floor = 0;
        elevators[i].is_available = 1;
        pthread_mutex_init(&elevators[i].lock, NULL);
        pthread_cond_init(&elevators[i].cond, NULL);
    }

    // Initialize passengers
    for (int i = 0; i < PASSENGERS; i++) {
        passengers[i].from_floor = 0;
        passengers[i].to_floor = 0;
        passengers[i].passenger_ready_for_pickup = 0;
        passengers[i].passenger_in_elevator = 0;
        passengers[i].passenger_exited_elevator = 0;
        passengers[i].elevator_at_pickup = 0;
        passengers[i].elevator_at_destination = 0;
        passengers[i].assigned_elevator_id = -1;
        pthread_mutex_init(&passengers[i].lock, NULL);
        pthread_cond_init(&passengers[i].cond_pickup, NULL);
        pthread_cond_init(&passengers[i].cond_in_elevator, NULL);
        pthread_cond_init(&passengers[i].cond_exit, NULL);
    }
}

// Move the elevator between floors
static void move_elevator(int elevator_id, int source, int destination, void (*move_direction)(int, int))
{
    pthread_mutex_lock(&movement_lock);
    
    int direction = (destination > source) ? 1 : -1;
    int steps = abs(destination - source);

    for (int i = 0; i < steps; i++) {
        move_direction(elevator_id, direction);
    }
    
    pthread_mutex_unlock(&movement_lock);
}

void scheduler_reset_passenger(int passenger_id) {
    
    pthread_mutex_lock(&completion_lock);
    pthread_mutex_unlock(&completion_lock);
   
    Passenger *passenger = &passengers[passenger_id];

    pthread_mutex_lock(&passenger->lock);

    // Set the reset flag
    passenger->reset_flag = 1;

    // Reset passenger state
    passenger->from_floor = 0;
    passenger->to_floor = 0;
    passenger->passenger_ready_for_pickup = 0;
    passenger->passenger_in_elevator = 0;
    passenger->passenger_exited_elevator = 0;
    passenger->elevator_at_pickup = 0;
    passenger->elevator_at_destination = 0;
    passenger->assigned_elevator_id = -1;

    // Clear the reset flag and notify waiting threads
    passenger->reset_flag = 0;
    pthread_cond_broadcast(&passenger->cond_pickup);
    pthread_cond_broadcast(&passenger->cond_in_elevator);
    pthread_cond_broadcast(&passenger->cond_exit);

    pthread_mutex_unlock(&passenger->lock);
}

void passenger_request(int passenger_id, int from_floor, int to_floor,
                       void (*enter)(int, int), void (*exit)(int, int)) {

        scheduler_reset_passenger(passenger_id);

    Passenger *passenger = &passengers[passenger_id];

    pthread_mutex_lock(&passenger->lock);
    passenger->from_floor = from_floor;
    passenger->to_floor = to_floor;
    passenger->passenger_ready_for_pickup = 1;

    qinsert(passenger_id);

    // Wait for elevator arrival
    while (!passenger->elevator_at_pickup) {
        pthread_cond_wait(&passenger->cond_pickup, &passenger->lock);
    }

    enter(passenger_id, passenger->assigned_elevator_id);
    passenger->passenger_in_elevator = 1;
    pthread_cond_signal(&passenger->cond_in_elevator);

    // Wait for destination
    while (!passenger->elevator_at_destination) {
        pthread_cond_wait(&passenger->cond_exit, &passenger->lock);
    }

    exit(passenger_id, passenger->assigned_elevator_id);
    passenger->passenger_exited_elevator = 1;
    
    // Increment completion counter
    pthread_mutex_lock(&completion_lock);
    passengers_completed++;
    pthread_mutex_unlock(&completion_lock);
    
    pthread_cond_signal(&passenger->cond_exit);
    pthread_mutex_unlock(&passenger->lock);
}

void elevator_ready(int elevator_id, int at_floor,
                    void (*move_direction)(int, int), void (*door_open)(int),
                    void (*door_close)(int)) {
    Elevator *elevator = &elevators[elevator_id];
    
    pthread_mutex_lock(&elevator->lock);
    elevator->current_floor = at_floor;
    elevator->is_available = 1;

    // Get next passenger from queue
    int passenger_id = qremove();  // Already has built-in waiting
    if (passenger_id == -1) {  // No more passengers, time to exit
        pthread_mutex_unlock(&elevator->lock);
        return;
    }
    elevator->is_available = 0;
    
    Passenger *passenger = &passengers[passenger_id];
    pthread_mutex_lock(&passenger->lock);

    passenger->assigned_elevator_id = elevator_id;  

    // Move to pickup floor
    move_elevator(elevator_id, at_floor, passenger->from_floor, move_direction);
    door_open(elevator_id);
    passenger->elevator_at_pickup = 1;
    pthread_cond_signal(&passenger->cond_pickup);

    // Single wait for passenger to enter
    pthread_cond_wait(&passenger->cond_in_elevator, &passenger->lock);

    door_close(elevator_id);

    // Move to destination floor
    move_elevator(elevator_id, passenger->from_floor, passenger->to_floor, move_direction);
    door_open(elevator_id);
    passenger->elevator_at_destination = 1;
    pthread_cond_signal(&passenger->cond_exit);

    // Single wait for passenger to exit
    pthread_cond_wait(&passenger->cond_exit, &passenger->lock);

    door_close(elevator_id);
    elevator->current_floor = passenger->to_floor;
    pthread_mutex_unlock(&passenger->lock);
    pthread_mutex_unlock(&elevator->lock);

    return;
}
