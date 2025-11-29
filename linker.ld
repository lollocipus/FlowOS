ENTRY(_start)

SECTIONS {
    . = 0x100000; /* Kernel base address at 1MB */

    .multiboot_header ALIGN(0x1000) : AT(ADDR(.multiboot_header)) {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(0x1000) {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(0x1000) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(0x1000) {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(0x1000) {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _end = .;
}
