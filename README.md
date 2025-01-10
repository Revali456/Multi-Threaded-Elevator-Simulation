# Elevator Scheduling Program

This project is a multi-threaded elevator scheduling system implemented in C. It demonstrates the use of threads, synchronization, and event signaling to simulate the behavior of an elevator responding to passenger requests.

## Features

### Core Functionality

1. **Passenger Request Handling**
   - Passengers can request the elevator from a starting floor to a destination floor using the `passenger_request` function.

2. **Elevator Movement**
   - The elevator moves to the requested floors, opens and closes doors, and responds to passenger actions.

3. **Synchronization**
   - The system ensures proper synchronization between the passenger and elevator using flags for readiness, pickup, and destination handling.

### Key Components

- **Passenger Struct**: Stores the `from_floor` and `to_floor` for a passenger.
- **Flags**:
  - `passenger_ready_for_pickup`: Signals that a passenger is ready for pickup.
  - `passenger_in_elevator`: Indicates that the passenger has entered the elevator.
  - `passenger_exited_elevator`: Signals that the passenger has exited the elevator.
  - `elevator_at_pickup`: Indicates that the elevator has arrived at the pickup floor.
  - `elevator_at_destination`: Indicates that the elevator has reached the destination floor.

## How It Works

1. **Initialization**
   - The `scheduler_init` function initializes the system state and flags.

2. **Passenger Interaction**
   - Passengers call `passenger_request` with their current floor, destination floor, and custom `enter` and `exit` functions.

3. **Elevator Control**
   - The `elevator_ready` function handles the elevator's movement and interaction with passengers by:
     - Moving to the passenger's floor using `move_elevator`.
     - Opening and closing doors at appropriate times.
     - Responding to passenger actions signaled via flags.

## Prerequisites

- A Linux-based system with GCC installed.
- Basic knowledge of multi-threading and synchronization in C.

## Compilation and Usage

1. Compile the program:
   ```bash
   gcc -o elevator_program elevator_program.c -lpthread -Wall -Wextra -pedantic
   ```

2. Run the program:
   - Integrate this with a simulation or test setup to invoke passenger requests and elevator operations.

3. Example Usage:
   ```c
   scheduler_init();
   passenger_request(1, 3, enter_function, exit_function);
   elevator_ready(0, 0, move_function, door_open_function, door_close_function);
   ```

## Limitations

- **Single Passenger**: The current implementation supports one passenger at a time.
- **Static Behavior**: No dynamic scheduling for multiple requests or optimization.

## Future Improvements

- Add support for handling multiple passengers and requests concurrently.
- Implement scheduling algorithms for optimizing elevator movement.
- Improve error handling and edge case management.

## Contributing

Contributions are welcome! Feel free to fork the repository and submit pull requests for enhancements or bug fixes.

## Acknowledgments

- Inspired by multi-threading and synchronization problems in operating systems.
- Thanks to the open-source community for examples and documentation.

