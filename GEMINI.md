# Project Structure

This document outlines the directory and file structure of the FlowOS project.

```
FlowOS/
├── linker.ld
├── Makefile
├── README.md
├── build/
│   └── isofiles/
│       └── boot/
│           └── grub/
│               └── grub.cfg
└── src/
    ├── boot.asm
    └── kernel.c
```

## File Descriptions

*   **`linker.ld`**: Linker script for the project, defining memory layout and section placement.
*   **`Makefile`**: Contains build instructions and rules for compiling the project.
*   **`README.md`**: General information about the project.
*   **`build/`**: Directory for build output.
    *   **`isofiles/`**: Contains files destined for the ISO image.
        *   **`boot/`**: Boot-related files.
            *   **`grub/`**: GRUB bootloader configuration files.
                *   **`grub.cfg`**: GRUB configuration file.
*   **`src/`**: Source code directory.
    *   **`boot.asm`**: Assembly code for the boot sector.
    *   **`kernel.c`**: C source code for the operating system kernel.
