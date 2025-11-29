#include "pmm.h"

#define BITMAP_SIZE 32768  // Supports up to 512MB of RAM (32768 * 32 * 4KB)

static uint32_t bitmap[BITMAP_SIZE];
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;

// External symbols from linker
extern uint32_t _end;

static inline void bitmap_set(uint32_t page) {
    bitmap[page / 32] |= (1 << (page % 32));
}

static inline void bitmap_clear(uint32_t page) {
    bitmap[page / 32] &= ~(1 << (page % 32));
}

static inline bool bitmap_test(uint32_t page) {
    return bitmap[page / 32] & (1 << (page % 32));
}

void pmm_init(struct multiboot_info* mboot) {
    // Mark all memory as used initially
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        bitmap[i] = 0xFFFFFFFF;
    }

    // Get kernel end address (aligned to page boundary)
    uint32_t kernel_end = ((uint32_t)&_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Parse multiboot memory map
    if (mboot->flags & (1 << 6)) {
        struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)mboot->mmap_addr;
        uint32_t mmap_end = mboot->mmap_addr + mboot->mmap_length;

        while ((uint32_t)mmap < mmap_end) {
            // Type 1 = available memory
            if (mmap->type == 1) {
                uint64_t addr = mmap->addr;
                uint64_t len = mmap->len;

                // Only use memory below 4GB for now (32-bit)
                if (addr < 0x100000000ULL) {
                    if (addr + len > 0x100000000ULL) {
                        len = 0x100000000ULL - addr;
                    }

                    uint32_t start_page = (uint32_t)((addr + PAGE_SIZE - 1) / PAGE_SIZE);
                    uint32_t end_page = (uint32_t)((addr + len) / PAGE_SIZE);

                    for (uint32_t page = start_page; page < end_page; page++) {
                        // Don't free memory below 1MB or kernel memory
                        uint32_t page_addr = page * PAGE_SIZE;
                        if (page_addr >= 0x100000 && page_addr >= kernel_end) {
                            bitmap_clear(page);
                            total_pages++;
                        }
                    }
                }
            }

            mmap = (struct multiboot_mmap_entry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    } else {
        // Fallback: use mem_upper from multiboot
        uint32_t mem_size = (mboot->mem_upper + 1024) * 1024;  // Convert to bytes
        uint32_t start_page = kernel_end / PAGE_SIZE;
        uint32_t end_page = mem_size / PAGE_SIZE;

        for (uint32_t page = start_page; page < end_page && page < BITMAP_SIZE * 32; page++) {
            bitmap_clear(page);
            total_pages++;
        }
    }

    used_pages = 0;
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        if (bitmap[i] != 0xFFFFFFFF) {
            for (uint32_t j = 0; j < 32; j++) {
                if (!(bitmap[i] & (1 << j))) {
                    uint32_t page = i * 32 + j;
                    bitmap_set(page);
                    used_pages++;
                    return page * PAGE_SIZE;
                }
            }
        }
    }
    return 0;  // Out of memory
}

void pmm_free_page(uint32_t addr) {
    uint32_t page = addr / PAGE_SIZE;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}

uint32_t pmm_get_free_pages(void) {
    return total_pages - used_pages;
}

uint32_t pmm_get_total_pages(void) {
    return total_pages;
}

uint32_t pmm_alloc_pages(uint32_t n) {
    if (n == 0) return 0;
    uint32_t total_avail = BITMAP_SIZE * 32;
    for (uint32_t start = 0; start <= total_avail - n; ++start) {
        bool can_alloc = true;
        for (uint32_t k = 0; k < n; ++k) {
            if (bitmap_test(start + k)) {
                can_alloc = false;
                break;
            }
        }
        if (can_alloc) {
            for (uint32_t k = 0; k < n; ++k) {
                bitmap_set(start + k);
            }
            used_pages += n;
            return start * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_pages(uint32_t addr, uint32_t n) {
    if (n == 0) return;
    uint32_t page = addr / PAGE_SIZE;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t p = page + i;
        if (p < BITMAP_SIZE * 32 && bitmap_test(p)) {
            bitmap_clear(p);
            used_pages--;
        }
    }
}
