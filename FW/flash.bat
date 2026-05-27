@echo off

echo Searching for ESP32...

python -m esptool --chip esp32 --port COM3 --baud 921600 write_flash ^
0x1000 bootloader.bin ^
0x8000 partition-table.bin ^
0x10000 home_automation.bin

pause