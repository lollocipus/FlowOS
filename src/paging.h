#ifndef PAGING_H
#define PAGING_H

#include "types.h"
#include "pmm.h"

#define PAGE_PRESENT   0x001
#define PAGE_WRITE     0x002
#define PAGE_USER      0x004
#define PAGE_4MB       0x080

typedef uint32_t* page_directory_t;
typedef uint32_t* page_table_t;

void paging_init(void);
void paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void paging_unmap_page(uint32_t virtual_addr);
uint32_t paging_get_physical(uint32_t virtual_addr);
uint32_t paging_kernel_pd_phys(void);
uint32_t paging_clone_pd(void);
void paging_switch(uint32_t pd_phys);

#endif
