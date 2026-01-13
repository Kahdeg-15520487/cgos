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

// Simple signed long to string conversion
static void debug_ltoa(long value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    long tmp_value;

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

// Simple unsigned long to string conversion
static void debug_ultoa(unsigned long value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    unsigned long tmp_value;

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

// Get string length
static int debug_strlen(const char *str) {
    int len = 0;
    while (*str++) len++;
    return len;
}

// Print with padding
static void debug_print_padded(const char *str, int width, char pad_char, int left_align) {
    int len = debug_strlen(str);
    int padding = (width > len) ? (width - len) : 0;
    
    if (!left_align) {
        while (padding-- > 0) debug_putchar(pad_char);
    }
    debug_puts(str);
    if (left_align) {
        while (padding-- > 0) debug_putchar(' ');
    }
}

// Convert to uppercase in-place
static void debug_to_upper(char *str) {
    for (; *str; str++) {
        if (*str >= 'a' && *str <= 'f') {
            *str = *str - 'a' + 'A';
        }
    }
}

// Comprehensive printf-style formatting
// Supports: %d, %i, %u, %x, %X, %p, %s, %c, %%
// Modifiers: l (long), ll (long long), z (size_t)
// Width: %8d, %08x, etc.
// Flags: - (left align), 0 (zero pad)
void debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[32];
    const char *ptr = format;
    
    while (*ptr) {
        if (*ptr != '%') {
            debug_putchar(*ptr++);
            continue;
        }
        
        ptr++; // Skip '%'
        if (!*ptr) break;
        
        // Parse flags
        int left_align = 0;
        int zero_pad = 0;
        
        while (*ptr == '-' || *ptr == '0' || *ptr == '+' || *ptr == ' ' || *ptr == '#') {
            if (*ptr == '-') left_align = 1;
            if (*ptr == '0') zero_pad = 1;
            ptr++;
        }
        
        // Parse width
        int width = 0;
        while (*ptr >= '0' && *ptr <= '9') {
            width = width * 10 + (*ptr - '0');
            ptr++;
        }
        
        // Parse length modifier
        int is_long = 0;      // 1 = long, 2 = long long
        int is_size_t = 0;
        
        if (*ptr == 'l') {
            ptr++;
            is_long = 1;
            if (*ptr == 'l') {
                ptr++;
                is_long = 2;
            }
        } else if (*ptr == 'z') {
            ptr++;
            is_size_t = 1;
        } else if (*ptr == 'h') {
            ptr++; // Ignore 'h' modifier (short)
            if (*ptr == 'h') ptr++; // Ignore 'hh' modifier (char)
        }
        
        char pad_char = (zero_pad && !left_align) ? '0' : ' ';
        
        switch (*ptr) {
            case 'd':
            case 'i': {
                if (is_long == 2) {
                    long long val = va_arg(args, long long);
                    debug_ltoa((long)val, buffer, 10);
                } else if (is_long == 1 || is_size_t) {
                    long val = va_arg(args, long);
                    debug_ltoa(val, buffer, 10);
                } else {
                    int val = va_arg(args, int);
                    debug_ltoa((long)val, buffer, 10);
                }
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'u': {
                if (is_long == 2) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    debug_ultoa((unsigned long)val, buffer, 10);
                } else if (is_long == 1 || is_size_t) {
                    unsigned long val = va_arg(args, unsigned long);
                    debug_ultoa(val, buffer, 10);
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_ultoa((unsigned long)val, buffer, 10);
                }
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'x': {
                if (is_long == 2) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    debug_ultoa((unsigned long)val, buffer, 16);
                } else if (is_long == 1 || is_size_t) {
                    unsigned long val = va_arg(args, unsigned long);
                    debug_ultoa(val, buffer, 16);
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_ultoa((unsigned long)val, buffer, 16);
                }
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'X': {
                if (is_long == 2) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    debug_ultoa((unsigned long)val, buffer, 16);
                } else if (is_long == 1 || is_size_t) {
                    unsigned long val = va_arg(args, unsigned long);
                    debug_ultoa(val, buffer, 16);
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_ultoa((unsigned long)val, buffer, 16);
                }
                debug_to_upper(buffer);
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'p': {
                void *val = va_arg(args, void*);
                debug_puts("0x");
                debug_ultoa((unsigned long)val, buffer, 16);
                debug_print_padded(buffer, width > 2 ? width - 2 : 0, '0', 0);
                break;
            }
            case 's': {
                const char *val = va_arg(args, const char*);
                if (!val) val = "(null)";
                debug_print_padded(val, width, ' ', left_align);
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
                // Unknown format specifier, print as-is
                debug_putchar('%');
                debug_putchar(*ptr);
                break;
            }
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
        if (*ptr != '%') {
            debug_putchar(*ptr++);
            continue;
        }
        
        ptr++; // Skip '%'
        if (!*ptr) break;
        
        // Parse flags
        int left_align = 0;
        int zero_pad = 0;
        
        while (*ptr == '-' || *ptr == '0' || *ptr == '+' || *ptr == ' ' || *ptr == '#') {
            if (*ptr == '-') left_align = 1;
            if (*ptr == '0') zero_pad = 1;
            ptr++;
        }
        
        // Parse width
        int width = 0;
        while (*ptr >= '0' && *ptr <= '9') {
            width = width * 10 + (*ptr - '0');
            ptr++;
        }
        
        // Parse length modifier
        int is_long = 0;      // 1 = long, 2 = long long
        int is_size_t = 0;
        
        if (*ptr == 'l') {
            ptr++;
            is_long = 1;
            if (*ptr == 'l') {
                ptr++;
                is_long = 2;
            }
        } else if (*ptr == 'z') {
            ptr++;
            is_size_t = 1;
        } else if (*ptr == 'h') {
            ptr++; // Ignore 'h' modifier (short)
            if (*ptr == 'h') ptr++; // Ignore 'hh' modifier (char)
        }
        
        char pad_char = (zero_pad && !left_align) ? '0' : ' ';
        
        switch (*ptr) {
            case 'd':
            case 'i': {
                if (is_long == 2) {
                    long long val = va_arg(args, long long);
                    debug_ltoa((long)val, buffer, 10);
                } else if (is_long == 1 || is_size_t) {
                    long val = va_arg(args, long);
                    debug_ltoa(val, buffer, 10);
                } else {
                    int val = va_arg(args, int);
                    debug_ltoa((long)val, buffer, 10);
                }
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'u': {
                if (is_long == 2) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    debug_ultoa((unsigned long)val, buffer, 10);
                } else if (is_long == 1 || is_size_t) {
                    unsigned long val = va_arg(args, unsigned long);
                    debug_ultoa(val, buffer, 10);
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_ultoa((unsigned long)val, buffer, 10);
                }
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'x': {
                if (is_long == 2) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    debug_ultoa((unsigned long)val, buffer, 16);
                } else if (is_long == 1 || is_size_t) {
                    unsigned long val = va_arg(args, unsigned long);
                    debug_ultoa(val, buffer, 16);
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_ultoa((unsigned long)val, buffer, 16);
                }
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'X': {
                if (is_long == 2) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    debug_ultoa((unsigned long)val, buffer, 16);
                } else if (is_long == 1 || is_size_t) {
                    unsigned long val = va_arg(args, unsigned long);
                    debug_ultoa(val, buffer, 16);
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    debug_ultoa((unsigned long)val, buffer, 16);
                }
                debug_to_upper(buffer);
                debug_print_padded(buffer, width, pad_char, left_align);
                break;
            }
            case 'p': {
                void *val = va_arg(args, void*);
                debug_puts("0x");
                debug_ultoa((unsigned long)val, buffer, 16);
                debug_print_padded(buffer, width > 2 ? width - 2 : 0, '0', 0);
                break;
            }
            case 's': {
                const char *val = va_arg(args, const char*);
                if (!val) val = "(null)";
                debug_print_padded(val, width, ' ', left_align);
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
                // Unknown format specifier, print as-is
                debug_putchar('%');
                debug_putchar(*ptr);
                break;
            }
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
