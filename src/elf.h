#ifndef ELF_H
#define ELF_H

#include "types.h"

#define ELF_MAGIC 0x464C457F  // "\x7FELF"

// ELF file types
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4

// ELF machine types
#define EM_386    3

// Program header types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4

// Program header flags
#define PF_X 0x1  // Execute
#define PF_W 0x2  // Write
#define PF_R 0x4  // Read

typedef struct {
    uint32_t magic;
    uint8_t  elf_class;      // 1 = 32-bit, 2 = 64-bit
    uint8_t  endianness;     // 1 = little, 2 = big
    uint8_t  version;
    uint8_t  os_abi;
    uint8_t  padding[8];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint32_t entry;          // Entry point virtual address
    uint32_t phoff;          // Program header table offset
    uint32_t shoff;          // Section header table offset
    uint32_t flags;
    uint16_t ehsize;         // ELF header size
    uint16_t phentsize;      // Program header entry size
    uint16_t phnum;          // Number of program header entries
    uint16_t shentsize;      // Section header entry size
    uint16_t shnum;          // Number of section header entries
    uint16_t shstrndx;       // Section header string table index
} __attribute__((packed)) elf_header_t;

typedef struct {
    uint32_t type;
    uint32_t offset;         // Offset in file
    uint32_t vaddr;          // Virtual address
    uint32_t paddr;          // Physical address (ignored)
    uint32_t filesz;         // Size in file
    uint32_t memsz;          // Size in memory
    uint32_t flags;          // Segment flags
    uint32_t align;          // Alignment
} __attribute__((packed)) elf_program_header_t;

int elf_exec(const char* path);

#endif
