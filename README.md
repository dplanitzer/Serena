# About The Project

Serena OS is what you get when modern operating system design and implementation meets vintage hardware like the Amiga computers. It is based on dispatch queues rather than threads, supports multiple users, is inspired by POSIX, yet retains its own character, is strongly object-oriented in terms of design and implementation and prepared for a cross platform future.

Check out the [Wiki](https://github.com/dplanitzer/Serena/wiki) for more details on the OS, how to build it, run it and how to create apps for it.

https://github.com/user-attachments/assets/c60bad93-5011-49ef-907d-ff006c287c9f

Serena OS comes with a powerful shell which implements a formally defined shell language. You can find the shell documentation [here](Commands/shell/README.md).

## Features

The following kernel services are implemented at this time:

* Kernel and user space separation in the sense of code privilege separation (not memory space separation)
* Dispatch queues with execution priorities
* Virtual processors with priorities and pervasive preemptive scheduling
* Interrupt handling with support for direct and semaphore-based interrupt handling
* Simple memory management (no virtual memory support yet)
* In-kernel object runtime system (used for drivers and file systems)
* Hierarchical processes with support for command line arguments, environment variables and I/O resource descriptor inheritance
* Hierarchical file system structure with support for mounting/unmounting file systems
* The SerenaFS file system
* Support for floppy disks, ROM and RAM-based disks
* Support for aout/GemDOS executables
* Support for pipes
* Monotonic clock and repeating timers
* Counting semaphores, condition variables and locks (mutexes)
* Zorro II and III expansion board detection and enumeration
* Event driver with support for keyboard, mouse, digital Joystick, analog joystick (paddles) and light pens
* Graphics driver with support for sprites (not taking advantage of the Blitter yet)
* VT52 and VT100 series compatible interactive console

The following user space services are available:

* System API with support for processes, dispatch queues, time and file I/O
* C99 compatible libc
* Beginnings of a C99 compatible libm
* libclap command line interface argument parsing library

The following user space programs are available at this time:

* An [interactive shell](Commands/shell/README.md) with command line editing and history support
* Many tools to work with files, filesystems, disks, processes, etc

The level of completeness and correctness of the various modules varies quite a bit at this time. Things are generically planned to improve over time :)

## Supported Hardware

The following hardware is supported at this time:

* Amiga 500, 600 and 2000 with at least 1MB RAM and a 68020 accelerator installed
* Amiga 1200
* Amiga 3000 and 4000
* Newer than the original chipsets work, but their specific features are not used
* Motorola 68020, 68030, 68040 and 68060 CPU. Note that the CPU has to be at least a 68020 class CPU
* Motorola 68881 and 68882 FPU
* Commodore Amiga floppy disk drive
* Zorro II and Zorro III RAM expansion boards

## Getting Started

Follow the instructions in the [Wiki](https://github.com/dplanitzer/Serena/wiki) to set up the needed development environment to build the OS, build it and run it with the help of an Amiga emulator or on real Amiga hardware.

## License

Distributed under the MIT License. See `LICENSE.txt` for more information.

## Contact

Dietmar Planitzer - [@linkedin](https://www.linkedin.com/in/dplanitzer)

Project link: [https://github.com/dplanitzer/Serena](https://github.com/dplanitzer/Serena)
