# About The Project

Serena OS is what you get when modern operating system design and implementation meets vintage hardware. Serena is built around pervasive preemptive concurrency and multiple users. The kernel is object-oriented and designed to be cross-platform and future proof. It currently runs on Amiga systems with a 68030 or better CPU installed.

https://github.com/user-attachments/assets/c60bad93-5011-49ef-907d-ff006c287c9f

One of the most significant differences to other existing operating systems is that there are no threads in Serena OS. Instead the kernel and applications achieve parallism with the help of dispatch queues. An application creates a dispatch queues and it then uses the queue to dispatch work (functions/callbacks/closures) that should execute concurrently. It does not need to create threads and implement a work queue or messaging system on its own. Dispatch queues take care of the queuing and they automatically acquire virtual processors to do the work and as needed. Virtual processors are shared between processes and move between them as needed.

Another interesting aspect is interrupt handling. Code which wants to react to an interrupt can register a counting semaphore with the interrupt controller for the interrupt it wants to handle. The interrupt controller will then signal the semaphore every time the interrupt occurs. Use of a counting semaphore ensures that the code which is interested in the interrupt does not miss the occurrence of an interrupt. The advantage of translating interrupts into signals on a semaphore is that the interrupt handling code executes in a well-defined context that is the same kind of context that any other kind of code runs in. It also gives the interrupt handling code more flexibility since it does not have to immediately react to an interrupt. The information that an interrupt has happened is never lost, whether the interrupt handler code happened to be busy with other things at the time of the interrupt or not.

The kernel is generally reentrant. This means that virtual processors continue to be scheduled and context switched preemptively even while the CPU is executing inside the kernel. Additionally a full compliment of counting semaphores, condition variables and lock APIs are available inside the kernel. The API of those objects closely resembles what you would find in a user space implementation of a traditional OS.

Serena implements a hierarchical process structure similar to POSIX. A process may spawn a number of child processes and it may pass a command line and environment variables to its children. A process accesses I/O resources via I/O channels which are similar to file descriptors in POSIX.

There are two notable differences between the POSIX style process model and the Serena model though: first instead of using fork() followed by exec() to spawn a new process, you use a single function in Serena called os_spawn(). This makes spawning a process much faster and significantly less error prone.

Secondly, a child process does not inherit the file descriptors of its parent by default. The only exception are the file descriptors 0, 1 and 2 which represent the terminal input and output streams. This model is much less error prone compared to the POSIX model where a process has to be careful to close file descriptors that it doesn't want to pass on to a child process before it spawns a child. Doing this was easy in the early days of Unix when applications were pretty much self contained and when there was no support for dynamic libraries. It's the opposite today because applications are far more complex and depend on many 3rd party libraries.

The executable file format at this time is the Atari ST GemDos file format which is a close relative to the aout executable format. This file format will be eventually replaced with a file format that will be able to support dynamic libraries. However for now it is good enough to get the job done.

The kernel implements SerenaFS which is a hierarchical file system with permissions and user and group information. A file system may be mounted on top of a directory located in another file system to expand the file namespace. All this works similar to how it works in POSIX systems. A process which wants to spawn a child process can specify that the child process should be confined to a sub-tree of the global file system namespace.

The boot file system is currently RAM-based. The ROM contains a disk image which is created with the diskimage tool and which serves as a template for the RAM disk. This ROM disk image is copied to RAM at boot time.

User space has support for libc, libsystem, libclap and the very beginnings of libm. Libsystem is a library that implements the user space side of the kernel interface. Libclap is a library that implements argument parsing for command line interface programs.

Serena OS comes with a shell which implements a formally defined shell language. You can find the shell document [here](Commands/shell/README.md).

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
* Support for aout/GemDos executables
* Support for pipes
* Monotonic clock
* Repeating timers
* Counting semaphores, condition variables and locks (mutexes)
* Zorro II and III expansion board detection and enumeration
* Event driver with support for keyboard, mouse, digital Joystick, analog joystick (paddles) and light pens
* Simple graphics driver (not taking advantage of the Blitter yet)
* VT52 and VT100 series compatible interactive console

The following user space services are available at this time:

* A system library with support for processes, dispatch queues, time and file I/O
* C99 compatible libc
* Beginnings of a C99 compatible libm
* libclap command line interface argument parsing library

The following user space programs are available at this time:

* An [interactive shell](Commands/shell/README.md) with command line editing and history support

The level of completeness and correctness of the various modules varies quite a bit at this time. Things are generically planned to improve over time :)

## Supported Hardware

The following hardware is supported at this time:

* Amiga 500, 600, 1200 and 2000 with at least 1MB RAM and a 68030 accelerator installed
* Amiga 3000 and 4000
* Newer than the original chipsets work, but their specific features are not used
* Motorola 68030, 68040 and 68060 CPU. Note that the CPU has to be at least a 68030 class CPU
* Motorola 68881 and 68882 FPU
* Commodore Amiga floppy disk drive
* Zorro II and Zorro III RAM expansion boards

## Getting Started

Setting the project up for development and running the OS is a bit involved. The instructions below are for Windows but they should work pretty much the same on Linux and macOS.

### Prerequisites

The first thing you will need is an Amiga computer emulator. I'm using WinUAE which you can download from [https://www.winuae.net/download](https://www.winuae.net/download)

Download the WinUAE installer and run it. This will place the emulator inside the 'Program Files' directory on your boot drive.

Next download and install the VBCC compiler and assembler needed to build the operating system. You can find the project homepage is at [http://www.compilers.de/vbcc.html](http://www.compilers.de/vbcc.html) and the download page for the tools at [http://sun.hasenbraten.de/vbcc](http://sun.hasenbraten.de/vbcc).

The version that I'm using for my development and that I know works correctly on Windows 11 is 0.9h. Be sure to add an environment variable with the name `VBCC` which points to the VBCC folder on your disk and add the `vbcc\bin` folder to the `PATH` environment variable.

Note that you need to have Microsoft Visual Studio and command line tools installed because the Microsoft C compiler is needed to build the build tools on Windows.

Finally install GNU make for Windows and make sure that it is in the `PATH` environment variable. A straight-forward way to do this is by executing the following winget command in a shell window: `winget install GnuWin32.Make`.

### Building the Build Tools

You only need to execute this step once and before you try to build the OS. The purpose of this step is to build a couple of tools that are needed to build the kernel and user space libraries. You can find documentation for these tools [here](Tools/README.md).

 First open a Developer Command Prompt in Windows Terminal and then cd into the `Serena/Tools` folder. Type `make` and hit return. This will build all required tools and place them inside a `Serena/build/tools` folder. The tools will be retained in this location even if you do a full clean of the OS project.

### Building the Operating System

Open the Serena project folder in Visual Studio Code and select `Build All` from the `Run Build Task...` menu. This will build the kernel, libsystem, libc, libm and shell and generate a single `Serena.rom` file inside the `Serena/product/Kernel/` folder. This ROM file contains the kernel, user space libraries and the shell.

### Running the Operating System

First you'll need to create an Amiga configuration with at least a 68030 CPU (i.e. Amiga 3000 or 4000) in WinUAE if you haven't already. The easiest way to do this is by going to Quickstart and selecting A4000 as the model. Then go to the Hardware/ROM page and update the "Main ROM file" text field such that it points to the `Serena.rom` file inside the `Serena/build/product/` folder on your disk. Then go to the Hardware/Floppy Drives page, enable drive DF0 and point it to the `boot_disk.adf` file inside the `Serena/build/product` directory on your disk. Finally give your virtual Amiga at least 1MB of Fast RAM by going to the Hardware/RAM page and setting the "Slow" entry to 1MB. Save this configuration so that you don't have to recreate it next time you want to run the OS.

Load the configuration and then hit the Start button or simply double-click the configuration in the Configurations page to run the OS. The emulator should open a screen that shows a boot message and then a shell prompt. See the [shell](Commands/shell/README.md) page for a list of commands that are supported by the shell.

## License

Distributed under the MIT License. See `LICENSE.txt` for more information.

## Contact

Dietmar Planitzer - [@linkedin](https://www.linkedin.com/in/dplanitzer)

Project link: [https://github.com/dplanitzer/Serena](https://github.com/dplanitzer/Serena)
