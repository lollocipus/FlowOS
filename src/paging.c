#include "paging.h"
#include "pmm.h"
#include "idt.h"

static uint32_t kernel_pd[1024] __attribute__((aligned(4096)));
static uint32_t kernel_page_tables[256][1024] __attribute__((aligned(4096)));  // First 1GB
static uint32_t kernel_pd_phys;
static uint32_t current_pd_phys;

extern void paging_enable(uint32_t page_directory_addr);

static void page_fault_handler(struct registers* regs) {
    uint32_t fault_addr;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_addr));

    uint32_t err = regs->err_code;

    if (err & 0x1) {
        // Protection violation
        goto panic;
    }

    if (err & 0x8) {
        // Reserved bit set
        goto panic;
    }

    // Demand paging
    uint32_t page_addr = fault_addr & 0xFFFFF000;
    uint32_t flags = PAGE_PRESENT | PAGE_WRITE;
    if (err & 0x4) flags |= PAGE_USER;

    uint32_t phys = pmm_alloc_page();
    if (!phys) {
        // OOM - halt for now
        goto panic;
    }

    paging_map_page(page_addr, phys, flags);
    return;

panic:
    cli();
    hlt();
}

void paging_init(void) {
    // Clear kernel page directory
    for (int i = 0; i < 1024; i++) {
        kernel_pd[i] = 0;
    }

    // Identity map first 1GB (kernel space)
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 1024; j++) {
            uint32_t addr = (i * 1024 + j) * PAGE_SIZE;
            kernel_page_tables[i][j] = addr | PAGE_PRESENT | PAGE_WRITE;
        }
        kernel_pd[i] = ((uint32_t)&kernel_page_tables[i]) | PAGE_PRESENT | PAGE_WRITE;
    }

    kernel_pd_phys = (uint32_t)&kernel_pd;
    current_pd_phys = kernel_pd_phys;

    // Register page fault handler
    register_interrupt_handler(14, page_fault_handler);

    // Enable paging
    paging_enable((uint32_t)&kernel_pd);
}

void paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    uint32_t* pd = (uint32_t*)current_pd_phys;
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    // Check if page table exists
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        // Allocate a new page table
        uint32_t pt_phys = pmm_alloc_page();
        if (!pt_phys) return;

        // Clear the new page table
        uint32_t* pt = (uint32_t*)pt_phys;
        for (int i = 0; i < 1024; i++) {
            pt[i] = 0;
        }

        pd[pd_index] = pt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    // Get page table address
    uint32_t* page_table = (uint32_t*)(pd[pd_index] & 0xFFFFF000);

    // Map the page
    page_table[pt_index] = (physical_addr & 0xFFFFF000) | flags | PAGE_PRESENT;

    // Invalidate TLB entry
    __asm__ __volatile__("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void paging_unmap_page(uint32_t virtual_addr) {
    uint32_t* pd = (uint32_t*)current_pd_phys;
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    uint32_t pd_entry = pd[pd_index];
    if (!(pd_entry & PAGE_PRESENT)) {
        return;
    }

    uint32_t* page_table = (uint32_t*)(pd_entry & 0xFFFFF000);
    page_table[pt_index] = 0;

    // Invalidate TLB entry
    __asm__ __volatile__("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

uint32_t paging_get_physical(uint32_t virtual_addr) {
    uint32_t* pd = (uint32_t*)current_pd_phys;
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    uint32_t offset = virtual_addr & 0xFFF;

    uint32_t pd_entry = pd[pd_index];
    if (!(pd_entry & PAGE_PRESENT)) {
        return 0;
    }

    uint32_t* page_table = (uint32_t*)(pd_entry & 0xFFFFF000);
    
    uint32_t pt_entry = page_table[pt_index];
    if (!(pt_entry & PAGE_PRESENT)) {
        return 0;
    }

    return (pt_entry & 0xFFFFF000) + offset;
}

uint32_t paging_kernel_pd_phys(void) {
    return kernel_pd_phys;
}

uint32_t paging_clone_pd(void) {
    uint32_t new_pd_phys = pmm_alloc_page();
    if (!new_pd_phys) return 0;

    uint32_t* new_pd = (uint32_t*)new_pd_phys;
    uint32_t* kern_pd = (uint32_t*)kernel_pd_phys;
    for (int i = 0; i < 1024; i++) {
        new_pd[i] = kern_pd[i];
    }
    return new_pd_phys;
}

void paging_switch(uint32_t pd_phys) {
    current_pd_phys = pd_phys;
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
}
