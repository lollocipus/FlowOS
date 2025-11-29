#ifndef PMM_H
#define PMM_H

#include "types.h"

#define PAGE_SIZE 4096

// Multiboot memory map entry
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

// Multiboot info structure (partial)
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed));

void pmm_init(struct multiboot_info* mboot);
uint32_t pmm_alloc_page(void);
void pmm_free_page(uint32_t addr);
uint32_t pmm_get_free_pages(void);
uint32_t pmm_get_total_pages(void);
uint32_t pmm_alloc_pages(uint32_t n);
void pmm_free_pages(uint32_t addr, uint32_t n);

#endif
