#include "teyboard.h"

#include "idt.h"

#include <stddef.h>
#include <stdint.h>

#define KEYBOARD_DATA   0x60
#define KEYBOARD_STATUS 0x64

#define KEY_BUFFER_SIZE 256

/* Scancode set 1, unshifted (US layout). */
static const char scancode_lower[128] = {
    0,   0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    0x08, '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x',
    'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* Scancode set 1, shifted (US layout). */
static const char scancode_shifted[128] = {
    0,   0x1B, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    0x08, '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X',
    'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define SCAN_LSHIFT 0x2A
#define SCAN_RSHIFT 0x36
#define SCAN_CAPS   0x3A

static volatile char key_buffer[KEY_BUFFER_SIZE];
static volatile size_t key_head = 0;
static volatile size_t key_tail = 0;
static int shift_pressed = 0;
static int caps_on = 0;

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void key_buffer_push(char c)
{
    size_t next = (key_tail + 1) % KEY_BUFFER_SIZE;
    if (next == key_head)
        return;                            /* buffer full, drop the key */
    key_buffer[key_tail] = c;
    key_tail = next;
}

static void teyboard_callback(registers_t *r)
{
    uint8_t scancode;
    char c;

    (void)r;
    scancode = inb(KEYBOARD_DATA);

    if (scancode == 0xE0)
        return;                            /* extended prefix (arrows etc.) */

    if (scancode & 0x80) {                 /* break code */
        uint8_t key = scancode & 0x7F;
        if (key == SCAN_LSHIFT || key == SCAN_RSHIFT)
            shift_pressed = 0;
        return;
    }

    switch (scancode) {
    case SCAN_LSHIFT:
    case SCAN_RSHIFT:
        shift_pressed = 1;
        return;
    case SCAN_CAPS:
        caps_on = !caps_on;
        return;
    default:
        break;
    }

    if (scancode >= 128)
        return;
    c = shift_pressed ? scancode_shifted[scancode] : scancode_lower[scancode];
    if (c >= 'a' && c <= 'z' && caps_on)
        c = (char)(c - 'a' + 'A');
    if (c == 0)
        return;

    key_buffer_push(c);
}

void teyboard_init(void)
{
    /* flush any stale scancodes */
    while (inb(KEYBOARD_STATUS) & 0x01)
        (void)inb(KEYBOARD_DATA);

    /* request keyboard to enable scanning, then eat the ACK */
    while (inb(KEYBOARD_STATUS) & 0x02)
        ;
    outb(KEYBOARD_DATA, 0xF4);
    while (!(inb(KEYBOARD_STATUS) & 0x01))
        ;
    (void)inb(KEYBOARD_DATA);

    irq_register(1, teyboard_callback);
}

/* Returns -1 if no key is pending, otherwise the next character. */
int teyboard_getchar(void)
{
    if (key_head == key_tail)
        return -1;
    char c = key_buffer[key_head];
    key_head = (key_head + 1) % KEY_BUFFER_SIZE;
    return c;
}
