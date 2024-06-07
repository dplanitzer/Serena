# Build Tools

This folder contains the sources for the build tools that are needed to build Serena OS on a host system. All build tools except diskimage can be built on either Windows or POSIX compatible hosts. Note however that the diskimage tool currently builds on Windows only.

Invoke the make file in this directory to build all tools. You will then be able to find the compiled executables in the `build/tools` folder.

The following sections explain the purpose of each tool.

## Building the Tools

Use the Makefile in the Tools folder to build all of the tools in a single step. Note that the makefile is currently for Windows only and that it assumes that you have the Microsoft Visual Studio C/C++ development tools installed.

The makefile must be invoked from a Developer Command Prompt. You can build individual tools by passing the name of the tool (as it appears in the sections below) to the make utility.

## Libtool

Libtool is used to create static libraries. It is similar to the "ar" POSIX tool, but strictly focused on creating and managing static libraries.

To create a static library from a set of a.out files, invoke libtool like this:

```
libtool create path/to/libc.a a.o b.o c.o ... 
```

Where the first argument after the "create" keyword is the path and name of the static library that should be created and where the rest of the argument line is a list of object code files that go into the static library.

Use the following command to list the contents of an existing static library:

```
libtool list path/to/lib
```

This will show the name and size of each object code file inside the static library.

## Makerom

The makerom tool is used to assemble a series of files into an Amiga ROM image. This image can then be used in the next step to burn an EPROM or it can be loaded into an Amiga emulator like WinUAE.

Makerom accepts a list of files as input. At least one input file must be specified and this first file will be placed at the very beginning of the ROM. The first file should be a raw binary of the operating system kernel. All other files are placed on 4 byte boundaries after the kernel binary, in the order in which they appear on the command line.

Invoke makerom like this to create a ROM image:

```
makerom path/to/rom_image path/to/kernel [path/to/file2 ...]
```

Where the first argument is the path and name of the ROM file that should be created and the remaining arguments are paths to the files that should be packaged into the ROM file.

The generated ROM image is 256KB in size and it includes the necessary IRQ auto-vector generation data that the Amiga interrupt hardware logic expects.

## Diskimage

Diskimage is used to create a SerenaFS formatted disk image. Note that the disk image size is currently fixed at 64KB and that the disk block size is fixed at 512 bytes.

The disk image tool expects a path to a directory on the host system as input. The directory at this path represents the root folder of the disk image that should be created. Diskimage first creates an empty disk image file, formats it with SerenaFS and then recursively clones all directories and files from the host file system into the SerenaFS disk image. However, hidden files and system files are not cloned.

You create a disk image by executing the following command:

```
diskimage create path/to/host_directory path/to/dmg
```

Where the first argument tells diskimage that it should create a new disk image file. The second argument is the path to the directory in the host file system that represents the SerenaFS root directory and the third argument is the path to where the disk image file should be written.

Note that diskimage always creates a ROM-style disk image at this time. This means that the output file is a raw dump of the SerenaFS volume without any form of disk specific encoding. It is not a ADF style image.

## Keymap

You use the keymap tool to create key maps for the Serena HID (human interface devices) system. A key map maps a USB standard key code to the character or string that should be delivered on a key press. Key maps allow you to specify separate mappings for key presses without a key modifier active and key presses with one or more modifier keys pressed at the same time.

Key maps are text files that describe a key mapping with the help of a domain specific language. Here is an example of a command that maps the USB key code 0x0004 (hexadecimal) to the lower- and upper-case 'A' character:

```
key(0x0004, 'a', 'A', 0, 0)
```
The first argument of the "key" command represents a standard USB key code. The second argument is the character that should be generated if the key is pressed without any modifier keys being pressed at the same time. The third argument is the character that should be generated if the key is pressed while just the Shift key is down. The fourth and fifth argument is the character that should be generated if the key is pressed together with the Alt key and the Alt plus Shift keys, respectively.

The character that is generated if the user presses the key together with the Control key is the character with the top 3 bits cleared.

You can specify a character literally by enclosing it in single quotes or you can specify it as a decimal number, hexadecimal number (with a leading 0x) or an octal number (with a leading 0).

Character and string literals support escape sequences to allow you to specify certain characters that would otherwise be awkward to express in a literal. An escape sequence starts with a backlash and is followed by a specific character or a number sequence. The following escape sequences are supported at this time:

* \\n generate a newline character
* \\r generate a carriage return character
* \\b generate a backspace character
* \\t generate a tab character
* \\e generate an escape character
* \\' generate a single quote character
* \\" generate a double-quote character
* \\\\ generate a backslash character

Here is an example of how to define a mapping of the single quote character:

```
key(0x0034, '\'', '"', 0, 0)
```

And here is an example of how to use a hexadecimal number to define a key mapping:

```
key(0x0028, 0x0a, 0x0a, 0, 0)
```

A key may also be mapped to a string. Note that the length of a string is very limited and may not exceed 16 bytes. Here is an example of a key that is mapped to a string:

```
key(0x0050, "\e[D")
```

This mapping binds the USB key code that represents a left cursor key to the escape sequence that a VT100 terminal is expected to send for a left cursor key.

Note that key mappings which map a key to a string do not support modifier keys at this time. The same string is generated whether a modifier key is pressed at the same time or not. Also pressing the Control key while pressing a key that is bound to a string will not change the string that is generated for the key press.

Compile a key map text file to a binary keymap file by invoking the keymap tool like this:

```
keymap compile path/to/key.keys
```

Where the first argument is a path to a key map text file with the extension "keys". This file is compiled to a keymap file at the same location but with the extension "keymap". Alternatively, a key map file may be compiled to a C source code file by explicitly specifying the output format:

```
keymap compile --format=C path/to/key.keys
```

An existing binary key map file can be decompiled and its contents listed with the following command:

```
keymap decompile path/to/keymaps_file
```
