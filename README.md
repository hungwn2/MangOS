
My implementation of a xv6 port for the MangoPi MQ-Pro allwinner-D1 board. 
<img width="2384" height="2384" alt="image" src="https://github.com/user-attachments/assets/fed629eb-033e-49ef-93e2-c9067010072e" />

This project ports MIT xv6 from its original QEMU virt environment to the MangoPi MQ Pro, a real RISC-V development board based on the Allwinner D1.
https://pdos.csail.mit.edu/6.828/2026/xv6.html

The goal is to adapt xv6’s low-level platform code for real hardware, including memory mapping, UART, interrupts, timers, storage, and the boot process through U-Boot and OpenSBI.
Various features from the labs and section exercises have been added, and as I learn more I am adding more things as well. Hopefully, the end result can is a system that I can use for everday tasks.



Checklist of things I am looking to add:
## xv6 MangoPi Port

* [x] Build xv6 for RISC-V
* [x] Boot xv6 on MangoPi D1
* [x] Get UART output/input working
* [x] Configure timer + external interrupts
* [x] Fix memory map / DRAM layout
* [x] Parse device tree
* [x] Boot through U-Boot + OpenSBI
* [x] Add SD / eMMC storage support
* [x] Pthreads-like API
* [x] Condition Variables 
* [x] Counting semaphores
* [x] Boot xv6 from eMMC
* [ ] pwd.h and /etc/passwd
* [ ] HDMI Framebuffer (using D1 DMA)
* [ ] UNIX-style signals + handlers
* [ ] Add USB DFU support
* [ ] lseek syscall
* [ ] symbolic links in filesystem
* [ ] Flash/update xv6 over DFU
* [ ] Add GPIO / I2C / SPI support
* [ ] Add hardware watchdog
* [ ] Rust Support?
* [ ] doubly indirect block pointer inodes (max filesize 1MiB -> 512MiB)
* [ ] users, groups, and permissions in filesystem
* [ ] Shell in serial
* [ ] PWD support
* [ ] Play doom?
* [x] Create `make flash` / `xfel` workflow
* [x] Run `usertests` on real hardware

Before I started doing this, I had done the xv6 labs here:https://github.com/hungwn2/Systems-stuff/branches


