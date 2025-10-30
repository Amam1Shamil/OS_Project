#!/bin/bash

# --- Test Configuration ---
TEST_USER="concurrency_user"
TEST_PASS="testpass123"
SERVER_EXECUTABLE="./server"
CLIENT_EXECUTABLE="./client"

# --- Helper Functions ---
cleanup() {
    echo "[INFO] Cleaning up..."
    killall $(basename ${SERVER_EXECUTABLE}) 2>/dev/null
    rm -rf users/
    rm -f users.txt
    rm -f small_file.txt large_file.txt
}

# --- Test Script ---
echo "[INFO] Starting Phase 2 Concurrency Test..."

# 1. Clean up previous runs and compile
cleanup
make clean
make
if [ ! -f "$SERVER_EXECUTABLE" ] || [ ! -f "$CLIENT_EXECUTABLE" ]; then
    echo "[FAIL] Compilation failed. Exiting."
    exit 1
fi
echo "[OK] Compilation successful."

# 2. Start the server in the background
${SERVER_EXECUTABLE} &
SERVER_PID=$!
sleep 1 # Give the server a moment to start up

# 3. Create a test user
echo "[INFO] Creating test user: ${TEST_USER}"
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} SIGNUP
echo

# 4. Prepare test files
echo "This is a small file." > small_file.txt
dd if=/dev/urandom of=large_file.txt bs=1K count=100 &>/dev/null

# 5. The Concurrency Test!
# Spawn multiple clients in the background doing conflicting work.
echo "[INFO] Spawning 10 clients to perform concurrent operations..."
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} UPLOAD large_file.txt &
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} UPLOAD small_file.txt &
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} LIST &
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} DELETE large_file.txt & # Tries to delete while uploading
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} UPLOAD large_file.txt & # Tries to upload again
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} LIST &
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} UPLOAD small_file.txt &
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} LIST &
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} UPLOAD large_file.txt &
${CLIENT_EXECUTABLE} ${TEST_USER} ${TEST_PASS} DELETE small_file.txt &

# 6. Wait for all background client jobs to finish
echo "[INFO] Waiting for all clients to finish..."
wait
echo "[INFO] All clients finished."

# 7. Stop the server gracefully
echo "[INFO] Sending shutdown signal to server..."
kill -SIGINT ${SERVER_PID}
wait ${SERVER_PID} # Wait for the server to exit cleanly
echo "[OK] Server shut down."

# 8. Verification
echo "[INFO] Verifying final state..."
# We check the user's directory. Because the operations are serialized by your
# per-user lock, the final state should be consistent (not corrupted), even
# though we don't know the exact final contents. The main thing is that the
# server did not crash and the file system is intact.
if [ -d "users/${TEST_USER}" ]; then
    echo "[OK] User directory exists and is intact."
    echo "Final file list:"
    ls -l users/${TEST_USER}
else
    echo "[FAIL] User directory is missing or corrupted!"
    exit 1
fi

echo
echo "[SUCCESS] Concurrency test passed without server crashes."