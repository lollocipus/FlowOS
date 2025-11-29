#include "gdt.h"

#define GDT_ENTRIES 6

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gp;
static tss_entry_t tss_entry;

extern void gdt_flush(uint32_t);
extern void tss_flush(void);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

    gdt[num].access = access;
}

void write_tss(int32_t num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t) &tss_entry;
    uint32_t limit = base + sizeof(tss_entry);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    // Ensure the descriptor is initially zero
    // memset(&tss_entry, 0, sizeof(tss_entry)); 
    // Manual memset since we don't have string.h included here easily
    uint8_t* p = (uint8_t*)&tss_entry;
    for(uint32_t i = 0; i < sizeof(tss_entry); i++) p[i] = 0;

    tss_entry.ss0  = ss0;
    tss_entry.esp0 = esp0;
    tss_entry.cs   = 0x0b;
    tss_entry.ss = 0x13;
    tss_entry.ds = 0x13;
    tss_entry.es = 0x13;
    tss_entry.fs = 0x13;
    tss_entry.gs = 0x13;
}

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gp.base  = (uint32_t)&gdt;

    // Null segment
    gdt_set_gate(0, 0, 0, 0, 0);

    // Kernel code segment: base=0, limit=4GB, 4KB granularity, 32-bit, code
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // Kernel data segment: base=0, limit=4GB, 4KB granularity, 32-bit, data
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // User code segment: base=0, limit=4GB, ring 3
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // User data segment: base=0, limit=4GB, ring 3
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // TSS segment
    write_tss(5, 0x10, 0x0);

    gdt_flush((uint32_t)&gp);
    
    // Load TSS
    __asm__ __volatile__("ltr %%ax" : : "a" (0x2B));
}
