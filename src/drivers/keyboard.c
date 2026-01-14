/**
 * PS/2 Keyboard Driver Implementation
 */

#include "keyboard.h"
#include "../pci/pci.h"
#include "../timer/timer.h"
#include "../debug/debug.h"

// Scancode Set 1 to ASCII translation table (lowercase)
static const char scancode_to_ascii[] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' '
};

// Scancode Set 1 to ASCII translation table (uppercase/shifted)
static const char scancode_to_ascii_shift[] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' '
};

// Circular key buffer
static char key_buffer[KEY_BUFFER_SIZE];
static volatile int buffer_head = 0;
static volatile int buffer_tail = 0;

// Modifier key state
static volatile uint8_t modifier_state = 0;

// Buffer a character
static void buffer_put(char c) {
    int next = (buffer_head + 1) % KEY_BUFFER_SIZE;
    if (next != buffer_tail) {
        key_buffer[buffer_head] = c;
        buffer_head = next;
    }
}

// Get character from buffer
static char buffer_get(void) {
    if (buffer_tail == buffer_head) {
        return 0;
    }
    char c = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}

void keyboard_init(void) {
    DEBUG_INFO("Initializing keyboard driver...\n");
    
    // Clear the keyboard buffer
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_FULL) {
        inb(KEYBOARD_DATA_PORT);
    }
    
    // Unmask keyboard IRQ (IRQ1)
    pic_clear_mask(IRQ_KEYBOARD);
    
    DEBUG_INFO("Keyboard driver initialized\n");
}

bool keyboard_has_key(void) {
    return buffer_head != buffer_tail;
}

char keyboard_get_char(void) {
    while (!keyboard_has_key()) {
        __asm__ volatile("pause");
    }
    return buffer_get();
}

uint8_t keyboard_get_modifiers(void) {
    return modifier_state;
}

// Keyboard interrupt handler
void keyboard_irq_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;
    
    // Handle modifier keys
    switch (key) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            if (released) {
                modifier_state &= ~MOD_SHIFT;
            } else {
                modifier_state |= MOD_SHIFT;
            }
            goto done;
            
        case KEY_LCTRL:
            if (released) {
                modifier_state &= ~MOD_CTRL;
            } else {
                modifier_state |= MOD_CTRL;
            }
            goto done;
            
        case KEY_LALT:
            if (released) {
                modifier_state &= ~MOD_ALT;
            } else {
                modifier_state |= MOD_ALT;
            }
            goto done;
            
        case KEY_CAPSLOCK:
            if (!released) {
                modifier_state ^= MOD_CAPS;
            }
            goto done;
    }
    
    // Only process key presses, not releases
    if (released) {
        goto done;
    }
    
    // Translate scancode to ASCII
    char ascii = 0;
    if (key < sizeof(scancode_to_ascii)) {
        bool shift = (modifier_state & MOD_SHIFT) != 0;
        bool caps = (modifier_state & MOD_CAPS) != 0;
        
        // Caps lock only affects letters
        if (caps && key >= 0x10 && key <= 0x32) {
            shift = !shift;
        }
        
        if (shift) {
            ascii = scancode_to_ascii_shift[key];
        } else {
            ascii = scancode_to_ascii[key];
        }
    }
    
    // Buffer the character
    if (ascii != 0) {
        buffer_put(ascii);
    }

done:
    // Send EOI to PIC
    pic_send_eoi(IRQ_KEYBOARD);
}
