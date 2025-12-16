#!/bin/bash

# Script to trigger Raspberry Pi Pico bootloader and upload firmware
# Usage: ./upload.sh [port]
# If port is not specified, it will try to auto-detect

# Function to find the serial port
find_port() {
    # Try common macOS/Linux serial port patterns for Pico
    # Pico typically appears as /dev/cu.usbmodem* or /dev/ttyACM* or /dev/ttyUSB*
    for pattern in /dev/cu.usbmodem* /dev/tty.usbmodem* /dev/ttyACM* /dev/ttyUSB* /dev/cu.usbserial*; do
        for port in $pattern; do
            if [ -e "$port" ]; then
                echo "$port"
                return 0
            fi
        done
    done
    return 1
}

# Function to list available ports
list_ports() {
    echo "Available serial ports:"
    for pattern in /dev/cu.usbmodem* /dev/tty.usbmodem* /dev/ttyACM* /dev/ttyUSB* /dev/cu.usbserial*; do
        for port in $pattern; do
            if [ -e "$port" ]; then
                echo "  $port"
            fi
        done
    done
}

# Function to normalize port name (try cu.* if tty.* is given, and vice versa)
normalize_port() {
    local port="$1"
    # On macOS, if given tty.*, try cu.* instead (needed for stty)
    if [[ "$port" == /dev/tty.* ]] && [[ "$OSTYPE" == "darwin"* ]]; then
        local cu_port="${port/tty./cu.}"
        if [ -e "$cu_port" ]; then
            echo "$cu_port"
            return 0
        fi
    fi
    # If given cu.*, use it as-is
    if [[ "$port" == /dev/cu.* ]]; then
        echo "$port"
        return 0
    fi
    # Otherwise return as-is
    echo "$port"
    return 0
}

# Get port from argument or auto-detect
if [ -n "$1" ]; then
    PORT=$(normalize_port "$1")
else
    PORT=$(find_port)
    if [ -z "$PORT" ]; then
        echo "Error: Could not find serial port."
        list_ports
        echo ""
        echo "Please specify it as an argument:"
        echo "  ./upload.sh /dev/cu.usbmodem14101"
        echo "  ./upload.sh /dev/ttyACM0"
        exit 1
    fi
    echo "Auto-detected port: $PORT"
fi

# Check if port exists (but be lenient - port might appear when we use it)
if [ ! -e "$PORT" ]; then
    echo "Warning: Port $PORT does not exist right now."
    echo "Available ports:"
    list_ports
    echo ""
    echo "Attempting to continue anyway (port might appear when accessed)..."
    # Don't exit - try to continue
fi

echo "Triggering Pico bootloader on $PORT at 1200 bps..."
# Open serial port at 1200 baud to trigger bootloader mode
# This works because Pico firmware can detect baud rate change and enter bootloader
# Using stty to set baud rate (macOS uses -f flag)
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    if ! stty -f "$PORT" 1200 2>/dev/null; then
        echo "Error: Could not access port $PORT. Make sure the device is connected."
        list_ports
        exit 1
    fi
    sleep 0.1
    stty -f "$PORT" 1200 2>/dev/null
else
    # Linux
    if ! stty -F "$PORT" 1200 2>/dev/null; then
        echo "Error: Could not access port $PORT. Make sure the device is connected."
        list_ports
        exit 1
    fi
    sleep 0.1
    stty -F "$PORT" 1200 2>/dev/null
fi

echo "Waiting 2 seconds for bootloader to activate..."
sleep 2

echo "Uploading firmware with PlatformIO..."
pio run -t upload

if [ $? -eq 0 ]; then
    echo "✓ Upload successful!"
else
    echo "✗ Upload failed!"
    exit 1
fi


