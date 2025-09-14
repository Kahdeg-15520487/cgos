#include "debug.h"
#include <stdarg.h>

// Port I/O functions
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Current debug level (can be modified at runtime)
static debug_level_t current_debug_level = DEBUG_LEVEL_INFO;

void debug_init(void) {
    // Test if debugcon is available by writing a test character
    // and checking if we can read it back (some implementations support this)
    debug_puts("Debug console initialized\n");
}

void debug_putchar(char c) {
    outb(DEBUGCON_PORT, (uint8_t)c);
}

void debug_puts(const char *str) {
    if (!str) return;
    
    while (*str) {
        debug_putchar(*str);
        str++;
    }
}

// Simple integer to string conversion
static void debug_itoa(int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int tmp_value;

    // Handle negative numbers for decimal base
    if (value < 0 && base == 10) {
        value = -value;
        *ptr++ = '-';
    }

    // Convert to string (in reverse order)
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    // Reverse the string
    ptr1 = str;
    if (*ptr1 == '-') ptr1++; // Skip negative sign
    
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

// Simple unsigned integer to string conversion
static void debug_utoa(unsigned int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    unsigned int tmp_value;

    // Convert to string (in reverse order)
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    // Reverse the string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

// Simple printf-style formatting
void debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[32];
    const char *ptr = format;
    
    while (*ptr) {
        if (*ptr == '%' && *(ptr + 1)) {
            ptr++; // Skip '%'
            
            switch (*ptr) {
                case 'd': {
                    int val = va_arg(args, int);
                    debug_itoa(val, buffer, 10);
                    debug_puts(buffer);
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_utoa(val, buffer, 10);
                    debug_puts(buffer);
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_utoa(val, buffer, 16);
                    debug_puts(buffer);
                    break;
                }
                case 'X': {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_utoa(val, buffer, 16);
                    // Convert to uppercase
                    for (char *p = buffer; *p; p++) {
                        if (*p >= 'a' && *p <= 'f') {
                            *p = *p - 'a' + 'A';
                        }
                    }
                    debug_puts(buffer);
                    break;
                }
                case 'p': {
                    void *val = va_arg(args, void*);
                    debug_puts("0x");
                    debug_utoa((uintptr_t)val, buffer, 16);
                    debug_puts(buffer);
                    break;
                }
                case 's': {
                    const char *val = va_arg(args, const char*);
                    debug_puts(val ? val : "(null)");
                    break;
                }
                case 'c': {
                    char val = (char)va_arg(args, int);
                    debug_putchar(val);
                    break;
                }
                case '%': {
                    debug_putchar('%');
                    break;
                }
                default: {
                    // Unknown format specifier, just print it
                    debug_putchar('%');
                    debug_putchar(*ptr);
                    break;
                }
            }
        } else {
            debug_putchar(*ptr);
        }
        ptr++;
    }
    
    va_end(args);
}

void debug_log(debug_level_t level, const char *format, ...) {
    // Only output if the level is at or below current debug level
    if (level > current_debug_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[32];
    const char *ptr = format;
    
    while (*ptr) {
        if (*ptr == '%' && *(ptr + 1)) {
            ptr++; // Skip '%'
            
            switch (*ptr) {
                case 'd': {
                    int val = va_arg(args, int);
                    debug_itoa(val, buffer, 10);
                    debug_puts(buffer);
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_utoa(val, buffer, 10);
                    debug_puts(buffer);
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_utoa(val, buffer, 16);
                    debug_puts(buffer);
                    break;
                }
                case 'X': {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_utoa(val, buffer, 16);
                    // Convert to uppercase
                    for (char *p = buffer; *p; p++) {
                        if (*p >= 'a' && *p <= 'f') {
                            *p = *p - 'a' + 'A';
                        }
                    }
                    debug_puts(buffer);
                    break;
                }
                case 'p': {
                    void *val = va_arg(args, void*);
                    debug_puts("0x");
                    debug_utoa((uintptr_t)val, buffer, 16);
                    debug_puts(buffer);
                    break;
                }
                case 's': {
                    const char *val = va_arg(args, const char*);
                    debug_puts(val ? val : "(null)");
                    break;
                }
                case 'c': {
                    char val = (char)va_arg(args, int);
                    debug_putchar(val);
                    break;
                }
                case '%': {
                    debug_putchar('%');
                    break;
                }
                default: {
                    // Unknown format specifier, just print it
                    debug_putchar('%');
                    debug_putchar(*ptr);
                    break;
                }
            }
        } else {
            debug_putchar(*ptr);
        }
        ptr++;
    }
    
    va_end(args);
}

void debug_hexdump(const void *data, size_t len, const char *prefix) {
    const uint8_t *bytes = (const uint8_t *)data;
    
    for (size_t i = 0; i < len; i += 16) {
        if (prefix) {
            debug_puts(prefix);
        }
        
        debug_printf("%08x: ", (unsigned int)i);
        
        // Print hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                debug_printf("%02x ", bytes[i + j]);
            } else {
                debug_puts("   ");
            }
            
            if (j == 7) {
                debug_putchar(' ');
            }
        }
        
        debug_puts(" |");
        
        // Print ASCII representation
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            uint8_t byte = bytes[i + j];
            if (byte >= 32 && byte <= 126) {
                debug_putchar((char)byte);
            } else {
                debug_putchar('.');
            }
        }
        
        debug_puts("|\n");
    }
}