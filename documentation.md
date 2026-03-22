# KTP Protocol: End-to-End Reliable Flow Control

## 1. Core Data Structures

### 1.1 Sliding Window Control (struct window)
This structure manages the state of our sliding window for both the sender and receiver.

| Field | Type | Description |
| :--- | :--- | :--- |
| base | int | Starting index of the current window |
| size | uint16_t | Maximum window size |
| msg_seq | uint16_t[] | Sequence numbers for messages within the window |
| last_seq | uint16_t | The highest sequence number used so far |
| last_ack | uint16_t | (Receiver) The sequence number of the last sent ACK |
| received | bool[] | (Receiver) Tracks arrival status of window slots |
| timeout | time_t[] | (Sender) Retransmission timers for sent packets |

### 1.2 Socket State (struct k_sockinfo)
Maintains the full state for each active KTP socket, including buffers and sync flags.

* Networking: Handles sockfd (UDP), source/destination addresses, and binding status.
* Buffers: Implements send_buff and recv_buff as fixed-size circular arrays.
* Flow Control: Contains instances of swnd (Send Window) and rwnd (Receive Window).
* State Flags: Tracks if the socket is free, closed, or experiencing nospace (buffer full).
* Termination: Manages fin_time and fin_retries for the teardown handshake.

---

## 2. Interface and API

### Public API Functions
These functions provide the interface for our user-level applications:

* k_socket(): Allocates a free socket entry in shared memory and initializes its state.
* k_bind(): Sets the local and remote addressing information.
* k_sendto(): Pushes data into the send buffer. The protocol engine then handles reliable delivery.
* k_recvfrom(): Pulls ordered data from the receive buffer.
* k_close(): Initiates the FIN handshake and marks the socket for garbage collection.

### Internal System Helpers
* Shared Memory: k_shmget, k_shmat, and k_shmdt manage our IPC back-end.
* Synchronization: wait_sem and signal_sem ensure atomic access to socket structures across different processes.
* Simulation: dropMessage(p) allows us to test the robustness of our sliding window by simulating a lossy network.

---

## 3. Protocol Engine Implementation

The system relies on three dedicated worker threads to handle the heavy lifting of the transport layer.

### Worker Threads
1.  threadR (The Receiver): This is the heart of the incoming logic. It parses packets, manages the receive window, triggers cumulative ACKs, and handles the binding of the underlying UDP socket.
2.  threadS (The Sender): This thread monitors the send buffer. It transmits new data when the window allows and handles retransmissions if a timeout occurs for unacknowledged packets or FIN signals.
3.  threadG (Garbage Collector): A background utility that scans for closed sockets or dead processes to reap resources and prevent memory leaks.

### Packet Handling Logic
We use a 520-byte packet structure:
* Header (8 bytes): Type (4B), Sequence No (2B), Window Size (2B).
* Payload (512 bytes): The raw data.
* Packet Types: Supports DATA, ACK, FIN, and FAK (FIN-ACK).

---

## 4. Experimental Findings

To evaluate the robustness of our Reliable UDP implementation, we conducted a series of tests using a **100 KB** dataset consisting of approximately **197 messages** (including the EOF marker). The tests were performed with a constant retransmission timeout of **5 seconds**.

### Performance Data

The following table tracks the relationship between network drop probability ($p$) and transmission overhead.

| Drop Probability ($p$) | No. of DATA Transmissions | Avg. Transmissions per Message |
| :--- | :--- | :--- |
| 0.00 | 184 | 0.934 |
| 0.05 | 255 | 1.294 |
| 0.10 | 307 | 1.558 |
| 0.15 | 327 | 1.660 |
| 0.20 | 412 | 2.091 |
| 0.25 | 364 | 1.848 |
| 0.30 | 454 | 2.305 |
| 0.35 | 464 | 2.355 |
| 0.40 | 682 | 3.462 |
| 0.45 | 654 | 3.320 |
| 0.50 | 793 | 4.025 |

### Observations & Analysis

Based on the experimental data and log analysis, we observed the following behaviors in the protocol engine:

* **Correlation of Loss and Retransmission:** As the drop probability increases, the frequency of retransmission events rises significantly, leading to a direct increase in the average number of transmissions required per message.
* **Stochastic Fluctuations:** We noted small non-monotonic jumps in the data (e.g., $p=0.20$ vs $p=0.25$, or $p=0.40$ vs $p=0.45$). These occur due to the inherent randomness of the simulated loss patterns in individual test runs.
* **Reliability at High Loss:** Even at a extreme loss rate of **0.50**, the protocol successfully achieved 100% reliable delivery through its sliding window and timeout mechanisms, though the transmission overhead increased to over 4 times the base rate.
* **Flow Control Integrity:** During periods of high congestion or full receiver buffers, the system correctly triggered flow control signaling, with application-level send calls returning `ENOSPACE` until window space was cleared via acknowledgments.