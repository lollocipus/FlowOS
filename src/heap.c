#include "heap.h"
#include "types.h"
#include "pmm.h"
#include "paging.h"

// Simple block-based heap allocator

struct heap_block {
    uint32_t size;       // Size of this block (including header)
    uint32_t is_free;    // Is this block free?
    struct heap_block* next;
    struct heap_block* prev;
};

#define BLOCK_HEADER_SIZE sizeof(struct heap_block)
#define MIN_BLOCK_SIZE (BLOCK_HEADER_SIZE + 16)

static struct heap_block* heap_start = NULL;
static struct heap_block* heap_end = NULL;
static uint32_t heap_size = 0;
static uint32_t heap_used = 0;
static uint32_t heap_virt_start = 0;

void heap_init(uint32_t start, uint32_t size) {
    heap_virt_start = start;
    heap_start = (struct heap_block*)start;
    heap_size = size;
    heap_used = BLOCK_HEADER_SIZE;

    // Initialize first block as one large free block
    heap_start->size = size;
    heap_start->is_free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    heap_end = heap_start;
}

static struct heap_block* find_free_block(size_t size) {
    struct heap_block* current = heap_start;
    
    while (current) {
        if (current->is_free && current->size >= size + BLOCK_HEADER_SIZE) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

static void split_block(struct heap_block* block, size_t size) {
    size_t total_needed = size + BLOCK_HEADER_SIZE;
    
    // Only split if remaining space is large enough
    if (block->size >= total_needed + MIN_BLOCK_SIZE) {
        struct heap_block* new_block = (struct heap_block*)((uint8_t*)block + total_needed);
        
        new_block->size = block->size - total_needed;
        new_block->is_free = 1;
        new_block->next = block->next;
        new_block->prev = block;
        
        if (block->next) {
            block->next->prev = new_block;
        }
        
        block->size = total_needed;
        block->next = new_block;
        
        if (block == heap_end) {
            heap_end = new_block;
        }
    }
}

static void merge_free_blocks(struct heap_block* block) {
    // Merge with next block if free
    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
        
        if (block->next) {
            block->next->prev = block;
        } else {
            heap_end = block;
        }
    }
    
    // Merge with previous block if free
    if (block->prev && block->prev->is_free) {
        block->prev->size += block->size;
        block->prev->next = block->next;
        
        if (block->next) {
            block->next->prev = block->prev;
        } else {
            heap_end = block->prev;
        }
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    struct heap_block* block = find_free_block(size);
    
    if (!block) {
        // Expand heap
        uint32_t phys = pmm_alloc_page();
        if (!phys) return NULL;
        
        uint32_t new_virt = heap_virt_start + heap_size;
        paging_map_page(new_virt, phys, PAGE_PRESENT | PAGE_WRITE);
        
        heap_size += PAGE_SIZE;
        
        // Append new free block
        struct heap_block* new_block = (struct heap_block*)new_virt;
        new_block->size = PAGE_SIZE;
        new_block->is_free = 1;
        new_block->next = NULL;
        new_block->prev = heap_end;
        if (heap_end) heap_end->next = new_block;
        heap_end = new_block;
        
        // Retry allocation
        block = find_free_block(size);
    }
    
    if (!block) {
        return NULL;
    }
    
    split_block(block, size);
    block->is_free = 0;
    heap_used += block->size;
    
    // Return pointer to usable memory (after header)
    return (void*)((uint8_t*)block + BLOCK_HEADER_SIZE);
}

void* kmalloc_aligned(size_t size, size_t alignment) {
    // Allocate extra space for alignment
    size_t total = size + alignment - 1 + sizeof(void*);
    void* ptr = kmalloc(total);
    
    if (!ptr) return NULL;
    
    // Calculate aligned address
    uintptr_t addr = (uintptr_t)ptr + sizeof(void*);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    
    // Store original pointer before aligned address
    ((void**)aligned)[-1] = ptr;
    
    return (void*)aligned;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    // Get block header
    struct heap_block* block = (struct heap_block*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    
    // Validate block is within heap
    if ((uint8_t*)block < (uint8_t*)heap_start || 
        (uint8_t*)block >= (uint8_t*)heap_start + heap_size) {
        return;  // Invalid pointer
    }
    
    if (block->is_free) {
        return;  // Already freed
    }
    
    heap_used -= block->size;
    block->is_free = 1;
    
    merge_free_blocks(block);
}

size_t heap_get_used(void) {
    return heap_used;
}

size_t heap_get_free(void) {
    return heap_size - heap_used;
}
