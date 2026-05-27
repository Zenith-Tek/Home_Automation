import os
import sys
import glob
import subprocess

# Detect OS
is_windows = os.name == "nt"

PYTHON_CMD = "python" if is_windows else "python3"

# Find serial ports
ports = []

if is_windows:
    for i in range(1, 21):
        ports.append(f"COM{i}")
else:
    ports.extend(glob.glob("/dev/ttyUSB*"))
    ports.extend(glob.glob("/dev/ttyACM*"))

selected_port = None

# Detect ESP32
# Detect ESP32
for port in ports:
    try:
        result = subprocess.run(
            [PYTHON_CMD, "-m", "esptool", "--port", port, "chip_id"],
            capture_output=True,
            text=True,
            timeout=5
        )

        output = result.stdout + result.stderr

        print(f"Checking {port}...")

        if "ESP32" in output:
            selected_port = port
            break

    except Exception as e:
        print(f"Error checking {port}: {e}")

if selected_port is None:
    print("ESP32 not found")
    sys.exit(1)

print(f"\nUsing port: {selected_port}")

# Menu
print("\nSelect Option:")
print("1. Flash Firmware")
print("2. Erase Flash")
print("3. Erase + Flash")

choice = input("\nEnter choice: ")

# -------------------------------
# ERASE FLASH
# -------------------------------
if choice == "2" or choice == "3":

    erase_cmd = [
        PYTHON_CMD,
        "-m",
        "esptool",
        "--chip", "esp32",
        "--port", selected_port,
        "erase_flash"
    ]

    print("\nErasing flash...\n")

    result = subprocess.run(erase_cmd)

    if result.returncode != 0:
        print("\nFlash erase failed")
        sys.exit(1)

    print("\nFlash erase completed")

# -------------------------------
# FLASH FIRMWARE
# -------------------------------
if choice == "1" or choice == "3":

    flash_cmd = [
        PYTHON_CMD,
        "-m",
        "esptool",
        "--chip", "esp32",
        "--port", selected_port,
        "--baud", "921600",
        "write_flash",
        "0x1000", "bootloader.bin",
        "0x8000", "partition-table.bin",
        "0x10000", "home_automation.bin"
    ]

    print("\nFlashing firmware...\n")

    result = subprocess.run(flash_cmd)

    if result.returncode != 0:
        print("\nFirmware flashing failed")
        sys.exit(1)

    print("\nFirmware flashing completed")

print("\nDone.")