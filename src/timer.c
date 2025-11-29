#include "timer.h"
#include "idt.h"
#include "pic.h"
#include "process.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

static volatile uint32_t ticks = 0;
static volatile bool preemption_enabled = false;
static uint32_t time_slice = 10;  // Ticks per time slice
static uint32_t slice_remaining = 0;

static void timer_callback(struct registers* regs) {
    (void)regs;
    ticks++;

    // Preemptive scheduling
    if (preemption_enabled) {
        if (slice_remaining > 0) {
            slice_remaining--;
        }
        
        if (slice_remaining == 0) {
            slice_remaining = time_slice;
            // Trigger reschedule
            process_t* current = process_get_current();
            if (current && current->state == PROCESS_STATE_RUNNING) {
                process_yield();
            }
        }
    }
}

void timer_init(uint32_t frequency) {
    register_interrupt_handler(32, timer_callback);

    uint32_t divisor = 1193180 / frequency;

    // Send command byte
    outb(PIT_COMMAND, 0x36);

    // Send frequency divisor
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(PIT_CHANNEL0, low);
    outb(PIT_CHANNEL0, high);

    // Unmask timer interrupt
    pic_clear_mask(IRQ_TIMER);
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

void timer_wait(uint32_t wait_ticks) {
    uint32_t end = ticks + wait_ticks;
    while (ticks < end) {
        __asm__ __volatile__("hlt");
    }
}

void timer_enable_preemption(void) {
    slice_remaining = time_slice;
    preemption_enabled = true;
}

void timer_disable_preemption(void) {
    preemption_enabled = false;
}
