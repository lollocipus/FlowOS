#include "elf.h"
#include "vfs.h"
#include "heap.h"
#include "paging.h"
#include "process.h"

extern void log_info(const char* msg);

// Switch to user mode and execute at entry point
extern void enter_usermode(uint32_t entry, uint32_t stack);

int elf_exec(const char* path) {
    log_info("ELF: Loading executable...");
    
    // Find the file
    fs_node_t* file = vfs_finddir(fs_root, (char*)path);
    if (!file) {
        log_info("ELF: File not found");
        return -1;
    }
    
    // Read ELF header
    elf_header_t header;
    if (vfs_read(file, 0, sizeof(elf_header_t), (uint8_t*)&header) != sizeof(elf_header_t)) {
        log_info("ELF: Failed to read header");
        return -1;
    }
    
    // Validate ELF magic
    if (header.magic != ELF_MAGIC) {
        log_info("ELF: Invalid magic number");
        return -1;
    }
    
    // Validate architecture
    if (header.machine != EM_386) {
        log_info("ELF: Not i386 executable");
        return -1;
    }
    
    // Validate type
    if (header.type != ET_EXEC) {
        log_info("ELF: Not executable file");
        return -1;
    }
    
    log_info("ELF: Valid executable detected");
    
    // Read program headers
    elf_program_header_t* pheaders = (elf_program_header_t*)kmalloc(header.phnum * sizeof(elf_program_header_t));
    if (!pheaders) {
        log_info("ELF: Out of memory");
        return -1;
    }
    
    if (vfs_read(file, header.phoff, header.phnum * sizeof(elf_program_header_t), (uint8_t*)pheaders) 
        != header.phnum * sizeof(elf_program_header_t)) {
        log_info("ELF: Failed to read program headers");
        kfree(pheaders);
        return -1;
    }
    
    // Load segments into memory
    for (int i = 0; i < header.phnum; i++) {
        if (pheaders[i].type != PT_LOAD) continue;
        
        uint32_t vaddr = pheaders[i].vaddr;
        uint32_t memsz = pheaders[i].memsz;
        uint32_t filesz = pheaders[i].filesz;
        uint32_t offset = pheaders[i].offset;
        
        // Calculate page-aligned addresses
        uint32_t page_start = vaddr & 0xFFFFF000;
        uint32_t page_end = (vaddr + memsz + 0xFFF) & 0xFFFFF000;
        uint32_t num_pages = (page_end - page_start) / PAGE_SIZE;
        
        // Allocate and map pages with USER permission
        for (uint32_t j = 0; j < num_pages; j++) {
            uint32_t page_vaddr = page_start + (j * PAGE_SIZE);
            uint32_t page_paddr = pmm_alloc_page();
            if (!page_paddr) {
                log_info("ELF: Out of physical memory");
                kfree(pheaders);
                return -1;
            }
            
            // Map with user permission
            uint32_t flags = PAGE_PRESENT | PAGE_USER;
            if (pheaders[i].flags & PF_W) flags |= PAGE_WRITE;
            
            paging_map_page(page_vaddr, page_paddr, flags);
        }
        
        // Read segment data
        if (filesz > 0) {
            vfs_read(file, offset, filesz, (uint8_t*)vaddr);
        }
        
        // Zero out BSS (uninitialized data)
        if (memsz > filesz) {
            uint8_t* bss = (uint8_t*)(vaddr + filesz);
            for (uint32_t j = 0; j < memsz - filesz; j++) {
                bss[j] = 0;
            }
        }
    }
    
    kfree(pheaders);
    
    // Allocate user stack (8KB)
    uint32_t stack_base = 0xC0000000;  // Stack at 3GB
    uint32_t stack_pages = 2;
    for (uint32_t i = 0; i < stack_pages; i++) {
        uint32_t page_paddr = pmm_alloc_page();
        if (!page_paddr) {
            log_info("ELF: Out of memory for stack");
            return -1;
        }
        paging_map_page(stack_base - (i + 1) * PAGE_SIZE, page_paddr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    
    uint32_t stack_top = stack_base;
    
    log_info("ELF: Jumping to userspace...");
    
    // Jump to user mode
    enter_usermode(header.entry, stack_top);
    
    // Should never return
    return 0;
}
