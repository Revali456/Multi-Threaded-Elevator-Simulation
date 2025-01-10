#include "elevator.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static struct passenger {
    int from_floor;
    int to_floor;
} Passenger;

int passenger_ready_for_pickup;
int passenger_in_elevator;
int passenger_exited_elevator;

int elevator_at_pickup;
int elevator_at_destination;


void scheduler_init(void)
{
    // initialize some data structures
    memset(&Passenger, 0x00, sizeof(Passenger));
    passenger_ready_for_pickup = 0;
    passenger_in_elevator = 0;
    passenger_exited_elevator = 0;
    elevator_at_pickup = 0;
    elevator_at_destination = 0;
}


void passenger_request(int passenger, int from_floor, int to_floor,
                       void (*enter)(int, int), void (*exit)(int, int))
{
    // inform elevator of floor
    Passenger.from_floor = from_floor;
    Passenger.to_floor = to_floor;

    // signal ready and wait
    passenger_ready_for_pickup = 1;
    while (!elevator_at_pickup);

    // enter elevator and wait
    enter(0, 0);
    passenger_in_elevator = 1;
    while (!elevator_at_destination);

    // exit elevator and signal
    exit(0, 0);
    passenger_exited_elevator = 1;
}


// example procedure that makes it easier to work with the API
// move elevator from source floor to destination floor
// you will probably have to modify this in the future ...

static void move_elevator(int source, int destination, void (*move_direction)(int, int))
{
    int direction, steps;
    int distance = destination - source;
    if (distance > 0) {
        direction = 1;
        steps = distance;
    } else {
        direction = -1;
        steps = -1*distance;
    }
    for (; steps > 0; steps--)
        move_direction(0, direction);
}


void elevator_ready(int elevator, int at_floor,
                    void (*move_direction)(int, int), void (*door_open)(int),
                    void (*door_close)(int))
{
    // wait for passenger to press button and move
    while (!passenger_ready_for_pickup);
    move_elevator(at_floor, Passenger.from_floor, move_direction);

    // open door and signal passenger
    door_open(0);
    elevator_at_pickup = 1;

    // wait for passenger to enter then close and move
    while (!passenger_in_elevator);
    door_close(0);
    move_elevator(Passenger.from_floor, Passenger.to_floor, move_direction);

    // open door the signal
    door_open(0);
    elevator_at_destination = 1;

    // wait for passenger to leave and close door
    while (!passenger_exited_elevator);
    door_close(0);
}
