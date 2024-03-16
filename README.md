# About The Project

Serena is an experimental operating system with an object-oriented kernel for Amiga 3000/4000 computers with support for pervasive preemptive concurrency and multiple users.

One aspect that sets it aside from traditional threading-based OSs is that it is purely built around dispatch queues similar to Apple's Grand Central Dispatch. There is no support for creating threads in user space nor in kernel space. Instead the kernel implements a virtual processor concept and it dynamically manages a pool of virtual processors. The size of the pool is automatically adjusted based on the needs of the dispatch queues. All kernel and user space concurrency is achieved by creating dispatch queues and by submitting work items to dispatch queues. Work items are simply closures (a function with associated state) from the viewpoint of the user.

Another interesting aspect is interrupt handling. Code which wants to react to an interrupt can register a counting semaphore with the interrupt controller for the interrupt it wants to handle. The interrupt controller will then signal the semaphore when an interrupt occurs. The use of a counting semaphore ensures that the code which is interested in the interrupt does not miss the occurrence of an interrupt. The advantage of translating interrupts into signals on a semaphore is that the interrupt handling code executes in a well-defined context that is the same kind of context that any other kind of code runs in. It also gives the interrupt handling code more flexibility since it does not have to immediately react to an interrupt. The information that an interrupt has happened is never lost, whether the interrupt handler code happened to be busy with other things at the time of the interrupt or not.

The kernel is generally reentrant. This means that virtual processors continue to be scheduled and context switched preemptively even while the CPU is executing inside the kernel. Additionally a full compliment of counting semaphores, condition variables and lock APIs are available inside the kernel. The API of those objects closely resembles what you would find in a user space implementation of a traditional OS.

Serena implements a hierarchical process system similar to POSIX. A process may spawn a number of child processes and it can pass a command line and environment variables to its children. A process accesses I/O resources via file descriptors (again similar to POSIX).

There are two notable differences between the POSIX style process model and the Serena model though: first instead of using fork() followed by exec() to spawn a new process, you use a single function in Serena called spawn(). This makes spawning a process much faster and significantly less error prone. Secondly a child process does not inherit the file descriptors of its parent by default. The only exception are the file descriptors 0, 1 and 2 which represent the terminal input and output streams. This model is much less error prone compared to the POSIX model where a process has to be careful to close file descriptors that it doesn't want to pass on to a child process before it execs the child. Doing this was easy in the early days of Unix when applications were pretty much self contained and when there was no support for dynamic libraries. It's the opposite today because applications are far more complex and depend on many 3rd party libraries.

The executable file format at this time is the Atari ST GemDos file format which is a close derivative of the aout executable format. This file format will eventually get replaced with a file format that will be able to support dynamic libraries. However for now it is good enough to get the job done.

The kernel supports a hierarchical file system layout. A file system may be mounted on top of a directory of another file system to expand the file name tree. All this works similar to how it works in POSIX systems. A process which wants to spawn a child process can specify that the child process should be confined to a sub-tree of the file system namespace.

The boot file system is always RAM-based. This allows the kernel to complete its boot sequence even if there is no physical disk available or the physical medium if defective.

At this point a very simple shell exists which allows you to navigate the RAM-based boot file system and to list the content of directories. There is also a kernel unit test rig to test various kernel APIs and kernel functionality.

Finally there is the beginning of a standard C library for user space programs available. The library implements C99 level functionality.

## Features

The following kernel services are implemented at this time:

* Kernel and user space separation in the sense of code privilege separation (not memory space separation)
* Dispatch queues with execution priorities
* Virtual processors with priorities and preemptive scheduling
* Interrupt handling with support for direct and semaphore-based interrupt handling
* Simple memory management (no virtual memory support yet)
* In-kernel object runtime system (used for drivers and file systems)
* Hierarchical processes with support for command line arguments, environment variables and I/O resource descriptor inheritance
* Hierarchical file systems with support for mounting/unmounting file systems
* A RAM-based file system
* Support for aout/GemDos executables
* Support for pipes
* Floppy disk driver
* Monotonic clock
* Repeating timers
* Counting semaphores, condition variables and locks (mutexes)
* Zorro II and III expansion board detection and enumeration
* Event driver with support for keyboard, mouse, digital Joystick, analog joystick (paddles) and light pens
* Simple graphics driver (not taking advantage of the Blitter yet)
* VT52 and VT100 series compatible interactive console

The following user space services are available at this time:

* A system library with support for processes, dispatch queues, time and file I/O
* A fairly complete implementation of a C99 compatible standard C library
* The beginnings of a C99 compatible floating-point math library

The following user space programs are available at this time:

* A simple [interactive shell](Commands/sh/README.md) with command line editing and history support

The level of completeness and correctness of the various modules varies quite a bit at this time. Things are generically planned to improve over time :)

## Supported Hardware

The following hardware is supported at this time:

* Amiga 2000, 3000 and 4000 motherboards
* Newer than the original chipsets work, but their specific features are not used
* Motorola 68030, 68040 and 68060 CPU. Note that the CPU has to be at least a 68030 class CPU
* Motorola 68881 and 68882 FPU
* Standard Commodore Amiga floppy drive
* Zorro II and Zorro III memory expansion boards

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

You only need to execute this step once and before you try to build the OS. The purpose of this step is to build a couple of tools that are needed to build the kernel and user space libraries. You can find dcoumentation for these tools [here](Tools/README.md).

 First open a Developer Command Prompt in Windows Terminal and then cd into the `Serena/Tools` folder. Type `make` and hit return. This will build all required tools and place them inside a `Serena/build/tools` folder. The tools will be retained in this location even if you do a full clean of the OS project.

### Building the Operating System

Open the Serena project folder in Visual Studio Code and select `Build All` from the `Run Build Task...` menu. This will build the kernel, libsystem, libc, libm and shell and generate a single `Serena.rom` file inside the `Serena/product/Kernel/` folder. This ROM file contains the kernel, user space libraries and the shell.

### Running the Demo

You first need to create an Amiga configuration with at least a 68030 CPU (i.e. Amiga 3000 or 4000) in WinUAE if you haven't already. The easiest way to do this is by going to Quickstart and selecting A4000 as the model. Then go to the Hardware/ROM page and change the Main ROM file text field to point to the `Serena.rom` file inside the `Serena/build/product/Kernel/` folder. Finally give your virtual Amiga at least 1MB of Fast RAM by going to the Hardware/RAM page and setting the Slow entry to 1MB. Save this configuration so that you don't have to recreate it next time you want to run the OS.

Load the configuration and then hit the Start button or simply double-click the configuration in the Configurations page to run the OS.

## License

Distributed under the MIT License. See `LICENSE.txt` for more information.

## Contact

Dietmar Planitzer - [@linkedin](https://www.linkedin.com/in/dplanitzer)

Project link: [https://github.com/dplanitzer/Serena](https://github.com/dplanitzer/Serena)
