#!/bin/bash
# Build script for Linux Chat Applications
# Uses GTK 3 for GUI
# Auto-installs dependencies if needed

echo "==================================="
echo "  Linux Chat System Build Script"
echo "  (GTK 3 GUI - Auto Setup)"
echo "==================================="

# Function to install dependencies
install_dependencies() {
    echo ""
    echo "[SETUP] Installing required packages..."
    echo "        (This may ask for your password)"
    echo ""
    
    sudo apt-get update
    sudo apt-get install -y build-essential g++ libgtk-3-dev pkg-config
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "[SETUP] Packages installed successfully!"
    else
        echo ""
        echo "[ERROR] Failed to install packages!"
        echo "        Try running manually:"
        echo "        sudo apt-get install build-essential g++ libgtk-3-dev pkg-config"
        exit 1
    fi
}

# Check if GTK 3 dev files exist, install if not
echo ""
echo "[CHECK] Checking for required libraries..."

if ! command -v g++ &> /dev/null; then
    echo "        g++ not found - will install"
    install_dependencies
elif ! pkg-config --exists gtk+-3.0 2>/dev/null; then
    echo "        GTK 3 not found - will install"
    install_dependencies
else
    echo "        All required libraries found!"
fi

# Get GTK flags
GTK_CFLAGS=$(pkg-config --cflags gtk+-3.0)
GTK_LIBS=$(pkg-config --libs gtk+-3.0)

echo ""
echo "[1/3] Compiling Server..."
g++ -o server Server.cpp $GTK_CFLAGS $GTK_LIBS -lpthread
if [ $? -eq 0 ]; then
    echo "      Server compiled successfully!"
else
    echo "      ERROR: Server compilation failed!"
    exit 1
fi

echo "[2/3] Compiling Client..."
g++ -o client Client.cpp $GTK_CFLAGS $GTK_LIBS -lpthread
if [ $? -eq 0 ]; then
    echo "      Client compiled successfully!"
else
    echo "      ERROR: Client compilation failed!"
    exit 1
fi

echo "[3/3] Compiling Shared Memory Chat..."
g++ -o shm_chat SharedMemoryChat.cpp $GTK_CFLAGS $GTK_LIBS -lpthread -lrt
if [ $? -eq 0 ]; then
    echo "      Shared Memory Chat compiled successfully!"
else
    echo "      ERROR: Shared Memory compilation failed!"
    exit 1
fi

echo ""
echo "==================================="
echo "  BUILD SUCCESSFUL!"
echo "==================================="
echo ""
echo "To test Socket Chat:"
echo "  Terminal 1: ./server"
echo "  Terminal 2: ./client"
echo ""
echo "To test Shared Memory Chat:"
echo "  Terminal 1: ./shm_chat"
echo "  Terminal 2: ./shm_chat"
echo ""
echo "Controls:"
echo "  - Type message + Enter to send"
echo "  - Close window to exit"
echo ""
