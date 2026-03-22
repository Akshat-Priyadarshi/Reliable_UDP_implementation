# Reliable Flow Control over Unreliable Channels (KTP Socket Library)

This project implements a **custom reliable transport protocol (KTP)** over **unreliable UDP sockets**, developed as part of the Networks Laboratory coursework. It emulates **end-to-end reliable data transfer using a sliding window flow control mechanism**, ensuring ordered and loss-free message delivery at the application level.

The system provides a **socket-like API through a static C library**, allowing user programs to send and receive fixed-size messages reliably despite packet drops or delays. Reliability is achieved using **sequence numbers, acknowledgements, retransmissions, buffering, and receiver-advertised window sizes**.

The implementation makes use of **shared memory, multi-threading, and timeout-based transmission control**, and supports **multiple concurrent KTP sockets** operating over standard UDP communication channels.

This project demonstrates practical concepts of:

- Reliable data transfer protocols  
- Sliding window flow control  
- Timeout and retransmission handling  
- Concurrent network programming in C  
- Inter-process communication using shared memory  

## Getting Started

### Clone the Repository

Clone the repository to your local machine using `git clone`, then navigate to the project directory:

```
$ git clone https://github.com/myself-aman-tudu/Reliable_UDP_implementation.git

$ cd Reliable_UDP_implementation
```
## Project Structure

The repository is organized into distinct modules to separate user-level applications from our core KTP transport layer and system logs.

```text
.
├── Makefile                # Builds static library and executables
├── application/
│   ├── user1.c             # Sender: reads from file, sends via KTP socket
│   └── user2.c             # Receiver: writes ordered output to results/
├── server/
│   └── initksocket.c       # KTP Engine: manages R/S threads and GC
├── transport/
│   ├── ksocket.c           # Reliable UDP implementation (IPC, Buffering)
│   └── ksocket.h           # KTP API definitions and data structures
├── testdata/               # Input files for testing (e.g., sample1.txt)
├── results/                # Output directory for reassembled files
├── logs/                   # Detailed runtime logs for debugging
├── libksocket.a            # Compiled static library archive
├── initksocket             # Protocol engine executable
├── user1                   # Sender application executable
└── user2                   # Receiver application executable
```

### Directory Breakdown

* **`transport/`**: The core of our implementation. It houses the reliable transport logic, including sliding window management, buffering, and the KTP socket interface.
* **`server/`**: Contains the initialization code for the background process that handles shared memory and the various protocol threads (R-thread, S-thread, and Garbage Collector).
* **`application/`**: Contains our end-user logic. `user1` reads our 80-paragraph dataset and pushes it through the KTP socket, while `user2` handles the reception.
* **`testdata/` & `results/`**: Used for data verification. We place our source facts in `testdata/` and compare them against the output generated in `results/`.
* **`logs/`**: Essential for monitoring our implementation's behavior. These logs capture packet loss, retransmissions, and flow control events in real-time.
## Run the System

### Start KTP Engine
In one terminal, execute the following command to initialize the protocol environment:

```bash
make run-init
```

This starts our background initialization process. It is responsible for managing shared memory, handling protocol threads, and maintaining the buffering and retransmission logic that ensures our UDP packets remain reliable.



### Start User Applications
In two separate terminals, run the user processes to begin data transfer:

**User 1:**
```bash
make run-user1
```

**User 2:**
```bash
make run-user2
```

These processes communicate using the **KTP socket abstraction**. This layer handles the complexities of the sliding window protocol, ensuring that the data sent between users arrives in the correct order despite being transmitted over an unreliable UDP base.

Our implemenation also supports more than 2 users, for which we have added two make targets for two more users, in two new terminals run the commands.

**User 3:**
```bash
make run-user3
```

**User 4:**
```bash
make run-user4
```

Overall this is not limited to any number of users, one can run the executables generated from user1.c and user2.c with proper arguments and different pair of sockets for {sender, receiver} pair.

---


## Clean Build Files

To reset the environment or clear out old test data, use the following commands:

* **Remove executables, object files, logs, and outputs:**
  ```bash
  make clean
  ```

* **Perform a full cleanup (including the static library):**
  ```bash
  make distclean
  ```
