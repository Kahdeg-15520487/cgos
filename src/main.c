#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <stdarg.h>
#include <stdio.h>

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

int abs(int x) {
    return x < 0 ? -x : x;
}

void draw_line(struct limine_framebuffer *framebuffer, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        ((uint32_t*)framebuffer->address)[y0 * (framebuffer->pitch / 4) + x0] = color;

        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// The following functions define a portable implementation of rand and srand.

static unsigned long int next = 1;  // NB: "unsigned long int" is assumed to be 32 bits wide

int rand(void)  // RAND_MAX assumed to be 32767
{
    next = next * 1103515245 + 12345;
    return (unsigned int) (next / 65536) % 32768;
}

void srand(unsigned int seed)
{
    next = seed;
}

// Simple 8x8 font
static const uint8_t font[128][8] = {
    // Define the font here or include a font header file
    [0 ... 127] = {0x00}, // Default all characters to empty

    // Define a simple 8x8 font for ASCII characters
    ['A'] = {0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00},
    ['B'] = {0x7C, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x7C, 0x00},
    ['C'] = {0x3C, 0x42, 0x40, 0x40, 0x40, 0x42, 0x3C, 0x00},
    ['D'] = {0x78, 0x44, 0x42, 0x42, 0x42, 0x44, 0x78, 0x00},
    ['E'] = {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7E, 0x00},
    ['F'] = {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x00},
    ['G'] = {0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C, 0x00},
    ['H'] = {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00},
    ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['J'] = {0x1E, 0x08, 0x08, 0x08, 0x08, 0x48, 0x30, 0x00},
    ['K'] = {0x42, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x00},
    ['L'] = {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00},
    ['M'] = {0x42, 0x66, 0x5A, 0x5A, 0x42, 0x42, 0x42, 0x00},
    ['N'] = {0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x42, 0x00},
    ['O'] = {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['P'] = {0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x00},
    ['Q'] = {0x3C, 0x42, 0x42, 0x42, 0x4A, 0x44, 0x3A, 0x00},
    ['R'] = {0x7C, 0x42, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x00},
    ['S'] = {0x3C, 0x42, 0x40, 0x3C, 0x02, 0x42, 0x3C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['V'] = {0x42, 0x42, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00},
    ['W'] = {0x42, 0x42, 0x42, 0x5A, 0x5A, 0x66, 0x42, 0x00},
    ['X'] = {0x42, 0x42, 0x24, 0x18, 0x24, 0x42, 0x42, 0x00},
    ['Y'] = {0x42, 0x42, 0x42, 0x24, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x02, 0x04, 0x18, 0x20, 0x40, 0x7E, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['0'] = {0x3C, 0x42, 0x46, 0x4A, 0x52, 0x62, 0x3C, 0x00},
    ['1'] = {0x18, 0x28, 0x48, 0x08, 0x08, 0x08, 0x3E, 0x00},
    ['2'] = {0x3C, 0x42, 0x02, 0x1C, 0x20, 0x40, 0x7E, 0x00},
    ['3'] = {0x3C, 0x42, 0x02, 0x1C, 0x02, 0x42, 0x3C, 0x00},
    ['4'] = {0x08, 0x18, 0x28, 0x48, 0x7E, 0x08, 0x08, 0x00},
    ['5'] = {0x7E, 0x40, 0x7C, 0x02, 0x02, 0x42, 0x3C, 0x00},
    ['6'] = {0x3C, 0x40, 0x7C, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['7'] = {0x7E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00},
    ['8'] = {0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00},
    ['9'] = {0x3C, 0x42, 0x42, 0x3E, 0x02, 0x42, 0x3C, 0x00},
    [':'] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
    [';'] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x10},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    ['='] = {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    ['+'] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    ['*'] = {0x00, 0x18, 0x7E, 0x3C, 0x7E, 0x18, 0x00, 0x00},
    ['/'] = {0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00},
    ['\\'] = {0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00},
    ['['] = {0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1C, 0x00},
    [']'] = {0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38, 0x00},
    ['{'] = {0x0E, 0x08, 0x08, 0x30, 0x08, 0x08, 0x0E, 0x00},
    ['}'] = {0x70, 0x10, 0x10, 0x0C, 0x10, 0x10, 0x70, 0x00},
    ['('] = {0x0C, 0x10, 0x20, 0x20, 0x20, 0x10, 0x0C, 0x00},
    [')'] = {0x30, 0x08, 0x04, 0x04, 0x04, 0x08, 0x30, 0x00},
    ['<'] = {0x00, 0x0C, 0x30, 0xC0, 0x30, 0x0C, 0x00, 0x00},
    ['>'] = {0x00, 0x30, 0x0C, 0x03, 0x0C, 0x30, 0x00, 0x00},
    ['?'] = {0x3C, 0x42, 0x02, 0x0C, 0x10, 0x00, 0x10, 0x00},
    ['@'] = {0x3C, 0x42, 0x5A, 0x5A, 0x5C, 0x40, 0x3E, 0x00},
    ['#'] = {0x14, 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x14, 0x00},
    ['$'] = {0x08, 0x3E, 0x48, 0x3C, 0x12, 0x7C, 0x10, 0x00},
    ['%'] = {0x62, 0x64, 0x08, 0x10, 0x26, 0x46, 0x00, 0x00},
    ['^'] = {0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['&'] = {0x30, 0x48, 0x30, 0x4A, 0x44, 0x3A, 0x00, 0x00},
    ['"'] = {0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['\''] = {0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['`'] = {0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['~'] = {0x00, 0x00, 0x34, 0x4C, 0x00, 0x00, 0x00, 0x00},
    ['a'] = {0x00, 0x00, 0x3C, 0x02, 0x3E, 0x42, 0x3E, 0x00},
    ['b'] = {0x40, 0x40, 0x5C, 0x62, 0x42, 0x62, 0x5C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x42, 0x40, 0x42, 0x3C, 0x00},
    ['d'] = {0x02, 0x02, 0x3A, 0x46, 0x42, 0x46, 0x3A, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x42, 0x7E, 0x40, 0x3C, 0x00},
    ['f'] = {0x0C, 0x10, 0x3E, 0x10, 0x10, 0x10, 0x10, 0x00},
    ['g'] = {0x00, 0x00, 0x3A, 0x46, 0x46, 0x3A, 0x02, 0x3C},
    ['h'] = {0x40, 0x40, 0x5C, 0x62, 0x42, 0x42, 0x42, 0x00},
    ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j'] = {0x06, 0x00, 0x0E, 0x06, 0x06, 0x46, 0x46, 0x3C},
    ['k'] = {0x40, 0x40, 0x46, 0x48, 0x70, 0x48, 0x46, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m'] = {0x00, 0x00, 0x66, 0x5A, 0x5A, 0x42, 0x42, 0x00},
    ['n'] = {0x00, 0x00, 0x5C, 0x62, 0x42, 0x42, 0x42, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x5C, 0x62, 0x62, 0x5C, 0x40, 0x40},
    ['q'] = {0x00, 0x00, 0x3A, 0x46, 0x46, 0x3A, 0x02, 0x02},
    ['r'] = {0x00, 0x00, 0x5C, 0x62, 0x40, 0x40, 0x40, 0x00},
    ['s'] = {0x00, 0x00, 0x3E, 0x40, 0x3C, 0x02, 0x7C, 0x00},
    ['t'] = {0x10, 0x10, 0x3E, 0x10, 0x10, 0x10, 0x0C, 0x00},
    ['u'] = {0x00, 0x00, 0x42, 0x42, 0x42, 0x46, 0x3A, 0x00},
    ['v'] = {0x00, 0x00, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00},
    ['w'] = {0x00, 0x00, 0x42, 0x42, 0x5A, 0x5A, 0x24, 0x00},
    ['x'] = {0x00, 0x00, 0x42, 0x24, 0x18, 0x24, 0x42, 0x00},
    ['y'] = {0x00, 0x00, 0x42, 0x42, 0x46, 0x3A, 0x02, 0x3C},
    ['z'] = {0x00, 0x00, 0x7E, 0x04, 0x18, 0x20, 0x7E, 0x00}
};

// Function to draw a character on the screen
void draw_char(struct limine_framebuffer *framebuffer, int x, int y, char c, uint32_t color) {
    const uint8_t *glyph = font[(unsigned char)c];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (glyph[i] & (1 << (7 - j))) { // Reverse the bit order
                ((uint32_t*)framebuffer->address)[(y + i) * (framebuffer->pitch / 4) + (x + j)] = color;
            }
        }
    }
}

// Function to draw a string on the screen
void draw_string(struct limine_framebuffer *framebuffer, int x, int y, const char *str, uint32_t color) {
    while (*str) {
        draw_char(framebuffer, x, y, *str++, color);
        x += 8; // Move to the next character position
    }
}

// Helper function to convert an integer to a string
static void itoa(int value, char *str, int base) {
    char *rc;
    char *ptr;
    char *low;
    // Set '-' for negative decimals.
    if (value < 0 && base == 10) {
        *str++ = '-';
        value = -value;
    }
    rc = ptr = str;
    // Set end of string
    *ptr = '\0';
    // Convert to string
    do {
        *ptr++ = "0123456789abcdef"[value % base];
        value /= base;
    } while (value);
    // Reverse the string
    low = rc;
    --ptr;
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
}

// vsprintf function to handle formatting
static int vsprintf(char *str, const char *format, va_list args) {
    char *s = str;
    for (; *format; format++) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    int value = va_arg(args, int);
                    char buffer[32];
                    itoa(value, buffer, 10);
                    char *buf_ptr = buffer;
                    while (*buf_ptr) {
                        *s++ = *buf_ptr++;
                    }
                    break;
                }
                case 'x': {
                    int value = va_arg(args, int);
                    char buffer[32];
                    itoa(value, buffer, 16);
                    char *buf_ptr = buffer;
                    while (*buf_ptr) {
                        *s++ = *buf_ptr++;
                    }
                    break;
                }
                case 's': {
                    char *value = va_arg(args, char *);
                    while (*value) {
                        *s++ = *value++;
                    }
                    break;
                }
                default:
                    *s++ = '%';
                    *s++ = *format;
                    break;
            }
        } else {
            *s++ = *format;
        }
    }
    *s = '\0';
    return s - str;
}

// Custom printf function to print formatted strings to the screen
void kprintf(struct limine_framebuffer *framebuffer, int x, int y, const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    draw_string(framebuffer, x, y, buffer, 0xFFFFFF); // White color
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
    
    // Initialize random number generator
    srand(__TIME__[7] + __TIME__[6] * 10 + __TIME__[4] * 60 + __TIME__[3] * 600 + __TIME__[1] * 3600 + __TIME__[0] * 36000); // Seed with compile time value based on __TIME__

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    uint64_t width = framebuffer->width;
    uint64_t height = framebuffer->height;

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 100; i++) {
        uint32_t random_color = ((rand() & 0xFF) << 16) | ((rand() & 0xFF) << 8) | (rand() & 0xFF); // Generate a random color
        for (size_t j = 0; j < 10; j++) { // Increase the line width to 10 pixels
            volatile uint32_t *fb_ptr = framebuffer->address;
            fb_ptr[(i * (framebuffer->pitch / 4)) + (i + j)] = random_color;
        }
    }

    // Draw "Hello, World!" on the screen
    draw_string(framebuffer, 10, 10, "Hello, World!", 0xff00000); // Red color
    draw_string(framebuffer, 10, 30, "Hello, World!", 0x00ff00); // Green color
    draw_string(framebuffer, 10, 50, "Hello, World!", 0x0000ff); // Blue color

    // Example usage of kprintf
    kprintf(framebuffer, 10, 70, "width: %d, height: %d.", width, height);

    draw_line(framebuffer, 20, 20, 799, 799, 0xFFFFFF);

    // We're done, just hang...
    hcf();
}