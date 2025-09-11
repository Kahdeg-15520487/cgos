#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>
#include <stdint.h>

// QEMU debugcon port
#define DEBUGCON_PORT 0xe9

// Debug log levels
typedef enum {
    DEBUG_LEVEL_ERROR = 0,
    DEBUG_LEVEL_WARN  = 1,
    DEBUG_LEVEL_INFO  = 2,
    DEBUG_LEVEL_DEBUG = 3,
    DEBUG_LEVEL_TRACE = 4
} debug_level_t;

// Initialize the debug console
void debug_init(void);

// Write a single character to debugcon
void debug_putchar(char c);

// Write a string to debugcon
void debug_puts(const char *str);

// Printf-style debug output
void debug_printf(const char *format, ...);

// Debug output with level
void debug_log(debug_level_t level, const char *format, ...);

// Convenience macros for different log levels
#define DEBUG_ERROR(fmt, ...) debug_log(DEBUG_LEVEL_ERROR, "[ERROR] " fmt, ##__VA_ARGS__)
#define DEBUG_WARN(fmt, ...)  debug_log(DEBUG_LEVEL_WARN,  "[WARN]  " fmt, ##__VA_ARGS__)
#define DEBUG_INFO(fmt, ...)  debug_log(DEBUG_LEVEL_INFO,  "[INFO]  " fmt, ##__VA_ARGS__)
#define DEBUG_DEBUG(fmt, ...) debug_log(DEBUG_LEVEL_DEBUG, "[DEBUG] " fmt, ##__VA_ARGS__)
#define DEBUG_TRACE(fmt, ...) debug_log(DEBUG_LEVEL_TRACE, "[TRACE] " fmt, ##__VA_ARGS__)

// Hex dump utility
void debug_hexdump(const void *data, size_t len, const char *prefix);

#endif // DEBUG_H
