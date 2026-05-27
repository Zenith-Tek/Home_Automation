#!/bin/bash

PORT=$(ls /dev/ttyUSB* 2>/dev/null | head -n 1)

if [ -z "$PORT" ]; then
    echo "ESP32 not found"
    exit 1
fi

echo "Using port: $PORT"

python3 -m esptool --chip esp32 --port $PORT --baud 921600 write_flash \
0x1000 bootloader.bin \
0x8000 partition-table.bin \
0x10000 home_automation.bin