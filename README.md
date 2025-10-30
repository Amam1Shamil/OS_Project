OS Lab Project (F2025)

BSCS23200 - Soban Ahmed
BSCS23161 Mohsin Fawad
BSCS23025 - Amam Shamil

This project is a multi-threaded file storage server built in C using POSIX threads. It implements a simplified Dropbox-like service, designed to handle multiple concurrent clients safely and efficiently.

## Core Features

*   *Multi-Threaded Architecture*: Implements a producer-consumer model with dedicated thread pools for client handling and file operations.
*   *User Management*: Supports user SIGNUP and LOGIN with credentials stored on the server.
*   *File Operations*: Authenticated users can UPLOAD, DOWNLOAD, LIST, and DELETE files.
*   *Concurrency Control*: A per-user locking mechanism serializes operations for a single user, preventing data races and ensuring data consistency even with multiple simultaneous sessions from the same account.
*   *Robust Shutdown*: Gracefully shuts down on SIGINT (Ctrl+C), ensuring all threads terminate and all allocated memory is freed.
*   *Bonus Features*: Implements a priority task queue (giving UPLOAD/DOWNLOAD higher priority) and basic file encoding (ROT13) for "encryption at rest".

## Prerequisites

To compile and run this project, you will need a Linux-based system with the following tools installed:
*   gcc
*   make
*   git

You can install them on a Debian/Ubuntu system with:
```bash
sudo apt-get update
sudo apt-get install build-essential git



How to Compile
Clone the Repository
code
Bash
git clone [GitHub Repository URL]
cd [Repository Folder Name]
Use the Makefile
The provided Makefile simplifies the compilation process.
To build the standard server and client:
code
Bash
make
This creates two executables: server and client.
To build the special server for race detection (TSAN):
code
Bash
make tsan
This creates an executable named server_tsan.
To clean up all compiled files:
code
Bash
make clean
How to Run and Test
1. Running the Server
Start the server by running the compiled executable. It will listen for connections on port 8080.
code
Bash
./server
2. Manual Testing (Single Client)
For quick, interactive tests, you can use netcat (nc) to connect to the server from a new terminal.
code
Bash
nc localhost 8080
You can then manually type commands like SIGNUP user pass, LOGIN user pass, LIST, etc.
3. Automated Concurrency Testing (Required for Phase 2)
The run_concurrency_test.sh script provides an automated test to verify the server's correctness under concurrent load, as required by Phase 2.
Make the script executable (only need to do this once):
code
Bash
chmod +x run_concurrency_test.sh
Run the test script:
code
Bash
./run_concurrency_test.sh
This script will automatically:
Compile the code.
Start the server in the background.
Create a test user.
Spawn 10 clients simultaneously that perform conflicting operations (uploading and deleting the same files) for the same user.
Wait for all clients to finish.
Gracefully shut down the server.
Verify that the server did not crash and the user's data directory is in a consistent state.
A successful run will end with the message [SUCCESS] Concurrency test passed without server crashes.
Project Structure
server.c: Main server logic, including thread creation, socket handling, and the client/worker thread functions.
client.c: A command-line client for automated testing and interaction.
queue.h/queue.c: Implementation of a thread-safe priority queue for tasks.
users.h/users.c: Handles user creation, authentication, and storage quota calculations.
concurrency.h/concurrency.c: Implements the fine-grained, per-user locking mechanism.
Makefile: The build script for the project.
run_concurrency_test.sh: The automated test script for verifying concurrency correctness.
code
Code
---
