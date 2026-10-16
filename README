My implementation of a xv6 port for the MangoPi MQ-Pro allwinner-D1 board. 
![MangoPi MQ1PH-1GB](MangoPi-MQ1PH-1GB.jpg)

This project ports MIT xv6 from its original QEMU virt environment to the MangoPi MQ Pro, a real RISC-V development board based on the Allwinner D1.
The goal is to adapt xv6’s low-level platform code for real hardware, including memory mapping, UART, interrupts, timers, storage, and the boot process through U-Boot and OpenSBI.
Various features from the labs and section exercises have been added, and as I learn more I am adding more things as well.
Beyond this, I am looking to add more features such as eMMC storage, USB DFU firmware updates, device-tree support, GPIO/I2C/SPI peripherals, and a reproducible flashing workflow.

Checklist of things I am looking to add:
## xv6 MangoPi Port

* [x] Build xv6 for RISC-V
* [x] Boot xv6 on MangoPi D1
* [x] Get UART output/input working
* [x] Configure timer + external interrupts
* [x] Fix memory map / DRAM layout
* [x] Parse device tree
* [x] Boot through U-Boot + OpenSBI
* [ ] Add SD / eMMC storage support
* [x] Pthreads-like API
* [x] Counting semaphores
* [ ] Boot xv6 from eMMC
* [ ] Add USB DFU support
* [ ] Flash/update xv6 over DFU
* [ ] Add GPIO / I2C / SPI support
* [ ] Add hardware watchdog
* [ ] Create `make flash` / `xfel` workflow
* [ ] Run `usertests` on real hardware

Before I started doing this, I had done the xv6 labs here:https://github.com/hungwn2/Systems-stuff/branches

