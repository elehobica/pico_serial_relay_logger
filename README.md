# UART serial relay and logger for Raspberry Pi Pico series

[![Build](https://github.com/elehobica/pico_serial_logger/actions/workflows/build-binaries.yml/badge.svg)](https://github.com/elehobica/pico_serial_logger/actions/workflows/build-binaries.yml)

## Overview
* Transparent bidirectional relay between UART0 (GP0 / GP1) of the target serial device and USB CDC
* All relayed bytes in both directions are dumped as-is to a log file on microSD card
* A new log file (`serial_NNN.log`) is created at every boot
* Two USB CDC ports on the single USB connector (composite device)
  * "Serial Relay": transparent relay port to the target device
  * "Console": status monitoring / help of this device
* Works standalone without PC (USB power only): UART capture and microSD logging keep running
* Baud rate toward the target device: 115200 bps 8N1 (fixed for now)
* No driver installation needed on Windows 10 / macOS / Linux

## Supported Board and Peripheral Devices
* Raspberry Pi Pico or Raspberry Pi Pico W (rp2040)
* Raspberry Pi Pico 2 or Raspberry Pi Pico 2 W (rp2350)
* microSD cards

## Pin Assignment & Connection

### UART (target serial device)
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 1 | GP0 | UART0_TX | to target RXD |
| 2 | GP1 | UART0_RX | from target TXD |
| 3 | GND | GND | GND |

Note:
* 3.3 V logic level only. Use a level shifter for 5 V devices and a transceiver for RS-232C levels.

### microSD card

| Pico Pin # | Pin Name | Function | microSD Pin Name | microSD Pin # |
----|----|----|----|----
|  4 | GP2 | SPI0_SCK | CLK | 5 |
|  5 | GP3 | SPI0_TX | CMD | 3 |
|  6 | GP4 | SPI0_RX | DAT0 | 7 |
|  7 | GP5 | SPI0_CSn | CD/DAT3 | 2 |
|  8 | GND | GND | VSS | 6 |
| 36 | 3V3(OUT) | 3.3V | VDD | 4 |

Note:
* As for the wire length between Pico and SD card, short wiring as possible is desired, otherwise errors such as Mount error and Write fail will occur.
* In case of card mount error or fundamental access errors, please confirm with [pico_fatfs tests](https://github.com/elehobica/pico_fatfs/tree/main/tests)

## Log file
* File name: `serial_NNN.log` in the root directory (`NNN` = 3-digit sequence number starting from 001)
* At every boot, the root directory is scanned and a new file with (max existing number + 1) is created
* Contents: raw bytes exactly as relayed, both directions (UART -> USB and USB -> UART) interleaved in one file. No timestamps, no direction markers
* Data is flushed to the card about once per second, so at most about 1 second of data is lost on power-off
* File timestamp is fixed (no RTC on the device). Sequence number indicates the order
* FAT32 and exFAT formats are supported. Format microSD card with [official SD Card Formatter](https://www.sdcard.org/downloads/formatter/) before usage

## Standalone operation
* PC connection is not required. Supply 5 V power through the USB connector (USB charger) or VSYS pin
* UART capture and microSD logging run regardless of the USB host state
* Log files can be reviewed later on PC

## USB ports
* The device appears as one USB device with two CDC (virtual COM) ports
  * Windows: two COM ports. Check "Bus reported device description" in Device Manager for "Serial Relay" / "Console"
  * Linux: two `/dev/ttyACMx` (`/dev/serial/by-id/` shows the interface names)
  * macOS: two `/dev/cu.usbmodem*`
* "Serial Relay" port
  * Relaying to USB starts when the terminal opens the port (DTR asserted). Bytes received while the port is closed are logged to microSD only
  * The baud rate setting on the PC terminal is ignored for now. The UART side is fixed at 115200 bps
* "Console" port
  * Banner with help and status is shown when the terminal opens the port

## Serial command interface (Console port)
| Key | Function |
----|----
| s | Show status (UART counters and errors, USB port state, microSD state and log file) |
| h | Help |

## LED indicator
| Priority | State | LED |
----|----|----
| 1 | microSD mount error / write error | Repeated burst of N blinks (0.2 s each) with a pause. N = 6: mount error, N = 7: write error (see console for FRESULT) |
| 2 | UART data flowing (within last 200 ms) | Fast blink (50 ms ON / 50 ms OFF) |
| 3 | Idle, logging ready | Steady ON |

Note:
* Repeated burst of 5 blinks forever at boot indicates cyw43 (Wi-Fi chip) initialization failure on Pico W / Pico 2 W
* USB host presence is not indicated by LED because the device also works standalone

## How to build with docker image
* Builds the firmware inside [pico-sdk-dev-docker:sdk-2.3.0](https://hub.docker.com/r/elehobica/pico-sdk-dev-docker) (same image used by CI). Requires Docker; no local Pico SDK setup is needed.
* `build_docker.sh` at the project root drives the container build.
```
$ git clone -b main https://github.com/elehobica/pico_serial_logger.git
$ cd pico_serial_logger
$ git submodule update -i
$ ./build_docker.sh           # build both targets (default)
$ ./build_docker.sh pico      # build only Pico / Pico W      -> build/pico_serial_logger.uf2
$ ./build_docker.sh pico2     # build only Pico 2 / Pico 2 W  -> build2/pico_serial_logger.uf2
```
* Outputs: `build/pico_serial_logger.uf2` (Pico / Pico W), `build2/pico_serial_logger.uf2` (Pico 2 / Pico 2 W)
* Download "*.uf2" on RPI-RP2 or RP2350 drive

## How to build in local
* See ["Getting started with Raspberry Pi Pico"](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
* Put "pico-sdk", "pico-examples" and "pico-extras" on the same level with this project folder.
* Set environmental variables for PICO_SDK_PATH, PICO_EXTRAS_PATH and PICO_EXAMPLES_PATH
* Confirmed with Pico SDK 2.3.0
```
> git clone -b 2.3.0 https://github.com/raspberrypi/pico-sdk.git
> cd pico-sdk
> git submodule update -i
> cd ..
> git clone -b sdk-2.3.0 https://github.com/raspberrypi/pico-examples.git
>
> git clone -b sdk-2.3.0 https://github.com/raspberrypi/pico-extras.git
> 
> git clone -b main https://github.com/elehobica/pico_serial_logger.git
> cd pico_serial_logger
> git submodule update -i
> cd ..
```
### Windows
* Build is confirmed in Developer Command Prompt for VS 2022 and Visual Studio Code on Windows environment
* Confirmed with cmake-3.27.2-windows-x86_64 and gcc-arm-none-eabi-10.3-2021.10-win32
* Lanuch "Developer Command Prompt for VS 2022"
```
> cd pico_serial_logger
> mkdir build && cd build
> cmake -G "NMake Makefiles" ..  ; (for Raspberry Pi Pico 1 series)
> cmake -G "NMake Makefiles" -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..  ; (for Raspberry Pi Pico 2)
> nmake
```
* Put "*.uf2" on RPI-RP2 or RP2350 drive
### Linux
* Build is confirmed with [pico-sdk-dev-docker:sdk-2.3.0](https://hub.docker.com/r/elehobica/pico-sdk-dev-docker)
```
$ cd pico_serial_logger
$ mkdir build && cd build
$ cmake ..  # (for Raspberry Pi Pico 1 series)
$ cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..  # (for Raspberry Pi Pico 2)
$ make -j4
```
* Download "*.uf2" on RPI-RP2 or RP2350 drive

## Future work
* Variable baud rate (via CDC line coding from the PC terminal, or stored in flash by console command)
* Timestamp of log files (needs RTC or NTP)
