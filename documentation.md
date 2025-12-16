# Linux Chat Systems Documentation

## 1. Project Overview
This project implements two types of chat systems on Linux using C++ and GTK 3 for the Graphical User Interface (GUI).

1.  **Socket-Based Chat**: A Client-Server model using TCP sockets. It supports communication over a network (different machines).
2.  **Shared Memory Chat**: A peer-to-peer style chat for users on the *same machine*, utilizing POSIX Shared Memory and Semaphores for synchronization.

## 2. System 1: Socket Chat (Client-Server)

### 2.1 Implementation Details
*   **Architecture**: Client-Server.
*   **Protocol**: TCP/IP (Stream Sockets).
*   **Concurrency**: Multithreading.
    *   **Server**: Uses a dedicated thread for network acceptance and communication loops.
    *   **Client**: Uses a dedicated thread to receive messages from the server asynchronously while the main thread handles the GUI.
*   **Key Files**:
    *   `Server.cpp`: Implements the server logic (bind, listen, accept, recv, send).
    *   `Client.cpp`: Implements the client logic (connect, send, recv).

### 2.2 Features
*   **Reliability**: Uses `MSG_NOSIGNAL` to prevent crashes if a peer disconnects unexpectedly (SIGPIPE).
*   **Thread Safety**: Uses `std::mutex` to protect shared resources (socket descriptors, connection state) between the UI thread and network threads.
*   **Graceful Exit**: Properly closes sockets and shuts down threads when the window is closed.

### 2.3 How to Run
1.  **Start Server**:
    ```bash
    ./server
    ```
2.  **Start Client(s)**:
    ```bash
    ./client
    ```
3.  **Connect**: In the Client window, enter the Server's IP (use `127.0.0.1` for localhost) and click **CONNECT**.

---

## 3. System 2: Shared Memory Chat

### 3.1 Implementation Details
*   **IPC Mechanism**: POSIX Shared Memory (`shm_open`, `mmap`).
*   **Synchronization**: POSIX Named Semaphores (`sem_open`, `sem_wait`, `sem_post`).
*   **Data Structure**:
    ```cpp
    struct SharedData {
        int messageCount;
        char messages[50][256];
    };
    ```
*   **Logic**:
    *   All instances map the same memory region.
    *   A semaphore ensures only one process writes to the message buffer at a time.
    *   A "Reader Thread" polls the message count and updates the GUI when new messages appear.

### 3.2 Features
*   **Zero-Copy**: Communication happens directly via memory mapping, which is extremely fast.
*   **Persistence**: Messages persist in memory as long as the system is running (or until manually cleared), allowing new users to see recent history.
*   **Robustness**: Handles buffer overflow by resetting the chat when the limit (50 messages) is reached.

### 3.3 How to Run
1.  Open multiple terminals.
2.  Run the application in each:
    ```bash
    ./shm_chat
    ```
3.  Type and send messages. All open windows will see the messages.

---

## 4. Build Instructions

### Prerequisites
*   **OS**: Linux (Ubuntu/Debian recommended)
*   **Compiler**: `g++`
*   **Libraries**: GTK 3 Development Libraries (`libgtk-3-dev`)

### Installation
```bash
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev
```

### Compiling
A helper script is provided to compile all components:

```bash
chmod +x build.sh
./build.sh
```

This will generate three executables:
*   `server`
*   `client`
*   `shm_chat`
