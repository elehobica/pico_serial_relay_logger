# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
### Added
* Initial implementation
  * Bidirectional transparent relay between UART0 (GP0 TX / GP1 RX, 115200 bps 8N1) and USB CDC
  * Raw dump of all relayed bytes (both directions) to `serial_NNN.log` on microSD, new file at every boot
  * Composite USB device with two CDC ports: "Serial Relay" and "Console" (status / help)
  * Standalone operation without USB host (UART capture and microSD logging only)
  * Support Raspberry Pi Pico / Pico W (rp2040) and Pico 2 / Pico 2 W (rp2350)
  * GitHub Actions for build and release, `build_docker.sh` for Docker-based local build
