  ## About The Project

Apollo is an experimental operating system for Amiga 3000/4000 computers with support for user and kernel space concurrency and reentracy.

One aspect that sets it aside from traditional threading-based OSs is that it is purely built around dispatch queues similar to Apple's Grand Central Dispatch rather than threads. So there is no support for creating threads in user space nor in kernel space. Instead the kernel implements a virtual processor concept and it maintains a pool of virtual processors. The size of the pool is dynamically adjusted based on the needs of the dispatch queues. All kernel and user space concurrency is achieved by creating dispatch queues and by submitting work items to dispatch queues. Work items are from the viewpoint of the user just closures (function callbacks plus state).

Another interesting aspect is interrupt handling. Code which wants to react to an interrupt registers either a binary or a counting semaphore with the interrupt controller for the interrupt it wants to handle. The interrupt system will then signal the semaphore when an interrupt occures. You would use a counting semaphore if it is important that you do not miss any interrupt occurences and a binary semaphore when you only care about the fact that at least one interrupt has occured. The advantage of translating interrupts into signals on a semaphore is that the interrupt handling code executes in a well-defined context that is the same kind of context that any other kind of code runs in. It also gives the interrupt handling code more flexibility since it does not have to immediately react to an interrupt. The information that an interrupt has happened is never lost whether the interrupt handler code happened to be busy with other things at the time of the interrupt or not.

The kernel itself is fully reentrant and supports permanent concurrency. This means that virtual processors continue to be scheduled and context switched even while the CPU is executing inside kernel space. There is also a full set of (counting/binary) semaphores, condition variables and locks available inside the kernel. The API of those objects closely resembles what you would find in a user space implementation of a typical OS.

There are a number of (unit) tests. However you currently have to manually invoke them because there's no automated unit testing framework yet. But then - manual testing is better than no testing.

Finally there is a set of fundamental routines for manipulating byte and bit ranges, doing 32bit and 64bit arithmetic and general support for the C programming language.

The following kernel services are implemented at this time:

* Kernel and user space separation in the sense of code privilige separation (not memory space separation)
* Dispatch queues with execution priorities
* Virtual processors
* Simple memory management (no virtual memory support)
* Floppy disk driver
* Monotonic clock
* Repeating timers
* Binary and counting semaphores, condition variables and locks (mutexes)
* Expansion board detection and enumeration
* Event driver with support for keyboard, mouse, digital Joystick, analog joystock (paddles) and light pens
* Simple graphics driver (not taking advantage of the Blitter yet)
* Console driver
* Beginnings of a syscall interface
* Basic 32bit and 64bit math routines
* Interrupt handling

Note that there is no support for a file system or shell at this time. Also the level of completness and correctness varies substantially. Things are generically planned to improve over time :)

## Getting Started

Setting the project up for development and running the OS is a bit involved. The instructions below are for Windows but they should work pretty much the same on Linux and macOS.

### Prerequisites

The first thing you will need is an Amiga computer emulator. I'm using WinUAE which you can download from [https://www.winuae.net/download](https://www.winuae.net/download)

Download the WinUAE installer and run it. This will place the emulator inside the 'Program Files' directory on your boot drive.

Next download and install the VBCC compiler and assembler needed to build the operating system. You can find the project homepage is at [http://www.compilers.de/vbcc.html](http://www.compilers.de/vbcc.html) and the download page for the tools at [http://sun.hasenbraten.de/vbcc](http://sun.hasenbraten.de/vbcc).

The version that I'm using for my development and that I know works correctly on Windows 11 is 0.9h. Be sure to add an environment variable with the name `VBCC` which points to the VBCC folder on your disk and add the `vbcc\bin` folder to the `PATH` environment variable.

Next make sure that you have Python 3 installed and that it can be invoked from the command line with the "python" command.

Finally install make for Windows and make sure that it is in the `PATH` environment variable. An simple way to do this is by taking advantage of winget: `winget install GnuWin32.Make`.

### Building Apollo

Open the Apollo project folder in Visual Studio Code and select `Build Kernel` from the `Run Build Task...` menu. This will build the kernel and and generate a `Boot.rom` file insde the `Apollo/build/Kernel/` folder.

### Running Apollo

You first need to create an Amiga configuration with at least a 68030 CPU (eg Amiga 3000 or 4000) in WinUAE if you haven't already. The easiest way to do this is by going to Quickstart and selecting A4000 as the model. Then go to the Hardware/ROM page and change the Main ROM file text field to point to `Apollo.rom` located inside the `Apollo/product/Kernel/` folder. Finally assign at least 1MB of Fast RAM by going to the Hardware/RAM page and setting the Slow entry to 1MB. Save this configuration so that you don't have to recreate it next time you want to run the OS.

Load the configuration and then hit the Start button or simply double-click the configuration in the Configurations page to run the OS.

## License

Distributed under the MIT License. See `LICENSE.txt` for more information.

## Contact

Dietmar Planitzer - [@linkedin](https://www.linkedin.com/in/dplanitzer)

Project link: [https://github.com/dplanitzer/Apollo](https://github.com/dplanitzer/Apollo)
