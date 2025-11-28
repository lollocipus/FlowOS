# FlowOS

A new lightweight and customizable operating system.

## Description

This is the initial "Hello, World" kernel for FlowOS. It demonstrates the basic boot process using GRUB and a minimal kernel written in C and Assembly.

## Prerequisites

To build and run FlowOS, you will need the following tools. On Windows, it is highly recommended to use [Windows Subsystem for Linux (WSL)](https://learn.microsoft.com/en-us/windows/wsl/install) to create a Linux environment where these tools are easily available.

*   `make`: The build automation tool.
*   `nasm`: The Netwide Assembler for compiling the boot code.
*   `gcc`: The GNU Compiler Collection for compiling the C kernel.
*   `ld`: The GNU Linker.
*   `grub-mkrescue`: A tool from the GRUB project to create a bootable ISO.
*   `qemu`: An emulator to run the operating system without needing a physical machine.

On a Debian-based Linux distribution (like Ubuntu), you can install these with:
`sudo apt-get update && sudo apt-get install build-essential nasm qemu-system-x86 grub-common grub-pc-bin`

## Building and Running

To build the kernel, create the bootable ISO, and run it in QEMU, simply run:

```bash
make run
```

You should see a QEMU window appear with the text "FlowOS is booting...".

## Cleaning

To remove all build files and the generated ISO, run:

```bash
make clean
```
