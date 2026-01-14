/**
 * PS/2 Keyboard Driver
 * 
 * Handles keyboard input via IRQ1 (vector 33)
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// Keyboard I/O ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

// Keyboard status register bits
#define KEYBOARD_STATUS_OUTPUT_FULL  0x01
#define KEYBOARD_STATUS_INPUT_FULL   0x02

// Special key codes
#define KEY_ESCAPE      0x01
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LALT        0x38
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_NUMLOCK     0x45
#define KEY_SCROLLLOCK  0x46
#define KEY_HOME        0x47
#define KEY_UP          0x48
#define KEY_PAGEUP      0x49
#define KEY_LEFT        0x4B
#define KEY_RIGHT       0x4D
#define KEY_END         0x4F
#define KEY_DOWN        0x50
#define KEY_PAGEDOWN    0x51
#define KEY_INSERT      0x52
#define KEY_DELETE      0x53

// Key buffer size
#define KEY_BUFFER_SIZE 64

// Modifier key flags
#define MOD_SHIFT   0x01
#define MOD_CTRL    0x02
#define MOD_ALT     0x04
#define MOD_CAPS    0x08

// Special key character codes (returned in key buffer)
#define SPECIAL_KEY_UP     0x80
#define SPECIAL_KEY_DOWN   0x81
#define SPECIAL_KEY_LEFT   0x82
#define SPECIAL_KEY_RIGHT  0x83
#define SPECIAL_KEY_ESC    0x1B

// Key event structure
typedef struct {
    char ascii;         // ASCII character (0 if special key)
    uint8_t scancode;   // Raw scancode
    uint8_t modifiers;  // Modifier key state
    bool released;      // true if key release event
} key_event_t;

// Function prototypes
void keyboard_init(void);
bool keyboard_has_key(void);
char keyboard_get_char(void);
bool keyboard_get_event(key_event_t *event);
uint8_t keyboard_get_modifiers(void);

// IRQ handler (called from assembly)
void keyboard_irq_handler(void);

#endif // KEYBOARD_H
