#include "timer.h"

#include "idt.h"

#include <stdint.h>

#define PIT_COMMAND  0x43
#define PIT_CHANNEL0 0x40
#define PIT_BASE_FREQ 1193182

static volatile uint32_t tick_count = 0;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void timer_callback(registers_t *r)
{
    (void)r;
    tick_count++;
}

/* Program the PIT channel 0 in one-shot/rate-generator mode. */
void timer_init(uint32_t frequency_hz)
{
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;

    outb(PIT_COMMAND, 0x36);                 /* channel 0, lobyte/hibyte, mode 3 */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_register(0, timer_callback);
}

uint32_t timer_ticks(void)
{
    return tick_count;
}
