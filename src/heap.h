#ifndef HEAP_H
#define HEAP_H

#include "types.h"
#include "pmm.h"

void heap_init(uint32_t start, uint32_t size);
void* kmalloc(size_t size);
void* kmalloc_aligned(size_t size, size_t alignment);
void kfree(void* ptr);
size_t heap_get_used(void);
size_t heap_get_free(void);

#endif
