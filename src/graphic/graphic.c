#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "graphic.h"

struct limine_framebuffer *framebuffer;
uint64_t width;
uint64_t height;
uint64_t current_line = 0;
uint64_t current_column = 0;
uint64_t current_color = 0xFFFFFF; // Default color is white
uint64_t current_bg_color = 0x000000; // Default background color is black
uint64_t current_fg_color = 0xFFFFFF; // Default foreground color is white
uint64_t current_cursor_x = 0;
uint64_t current_cursor_y = 0;
uint64_t current_cursor_color = 0xFFFFFF; // Default cursor color is white
uint64_t max_line = 0;
uint64_t max_column = 0;

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

void setup_graphic(volatile struct limine_framebuffer_request *framebuffer_request) {
    // Ensure we got a framebuffer.
    if (framebuffer_request->response == NULL
     || framebuffer_request->response->framebuffer_count < 1) {
        // Handle error: no framebuffer available
        return;
    }
    
    // Fetch the first framebuffer.
    framebuffer = framebuffer_request->response->framebuffers[0];
    
    width = framebuffer->width;
    height = (framebuffer->height);
}

// Function to draw a character on the screen
void draw_char( int x, int y, char c, uint32_t color) {
    const uint8_t *glyph = font[(unsigned char)c];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (glyph[i] & (1 << (7 - j))) { // Reverse the bit order
                ((uint32_t*)framebuffer->address)[(y + i) * (framebuffer->pitch / 4) + (x + j)] = color;
            }
        }
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
    // Convert to string
    do {
        *ptr++ = "0123456789abcdef"[value % base];
        value /= base;
    } while (value);
    // Null-terminate the string
    *ptr = '\0';
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
                    char buffer[32] = {0}; // Clear buffer
                    itoa(value, buffer, 10);
                    char *buf_ptr = buffer;
                    while (*buf_ptr) {
                        *s++ = *buf_ptr++;
                    }
                    break;
                }
                case 'x': {
                    int value = va_arg(args, int);
                    char buffer[32] = {0}; // Clear buffer
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
                case 'l': {
                    format++;
                    if (*format == 'x') {
                        unsigned long value = va_arg(args, unsigned long);
                        char buffer[32] = {0}; // Clear buffer
                        itoa(value, buffer, 16);
                        char *buf_ptr = buffer;
                        while (*buf_ptr) {
                            *s++ = *buf_ptr++;
                        }
                    } else {
                        *s++ = '%';
                        *s++ = 'l';
                        *s++ = *format;
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

// Function to draw a string on the screen
void draw_string(int x, int y, const char *str, uint32_t color) {
    while (*str) {
        draw_char(x, y, *str++, color);
        x += 8; // Move to the next character position
    }
}

// Custom kprintf function to print formatted strings to the screen
void kprintf(int x, int y, const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    draw_string(x, y, buffer, 0xFFFFFF); // White color
}

int abs(int x) {
    return x < 0 ? -x : x;
}

void draw_line(int x0, int y0, int x1, int y1, int thickness, uint32_t color) {
    // Compute absolute differences and drawing direction
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    // Calculate half thickness (for centering the thickness around the line)
    int half_thickness = thickness / 2;
    
    // Bresenham's line algorithm with thickness
    while (true) {
        // Draw a square of pixels at the current point (x0,y0)
        for (int i = -half_thickness; i <= half_thickness; i++) {
            for (int j = -half_thickness; j <= half_thickness; j++) {
                // Only draw pixels that are within the thickness radius
                // This creates a more circular brush for better appearance
                if (i*i + j*j <= half_thickness*half_thickness + half_thickness) {
                    draw_pixel(x0 + i, y0 + j, color);
                }
            }
        }
        
        // Check if we've reached the end point
        if (x0 == x1 && y0 == y1) 
            break;
        
        // Calculate next point using Bresenham's algorithm
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

void clear_screen(uint32_t color) {
    uint32_t *fb_ptr = (uint32_t *)framebuffer->address;
    for (uint64_t i = 0; i < (width * height); i++) {
        fb_ptr[i] = color;
    }
}

void draw_rect(int x, int y, int w, int h, int thickness, uint32_t color, bool filled) {
    if (filled) {
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                draw_pixel(x + j, y + i, color);
            }
        }
    } else {
        for (int t = 0; t < thickness; t++) 
        {
            // Top border
            draw_line(x, y - t, x + w - 1, y - t, 1, color);
            // Bottom border
            draw_line(x, y + h - 1 + t, x + w - 1, y + h - 1 + t, 1, color);
            // Left border
            draw_line(x - t, y, x - t, y + h - 1, 1, color);
            // Right border
            draw_line(x + w - 1 + t, y, x + w - 1 + t, y + h - 1, 1, color);
        }
    }
}

void draw_circle(int x, int y, int radius, int thickness, uint32_t color, bool filled) {
    int x0 = 0;
    int y0 = radius;
    int d = 1 - radius;

    if (filled) {
        while (x0 <= y0) {
            for (int i = -y0; i <= y0; i++) {
                for (int t = -thickness / 2; t <= thickness / 2; t++) {
                    draw_pixel(x + x0 + t, y + i, color);
                    draw_pixel(x - x0 + t, y + i, color);
                }
            }
            if (d < 0) {
                d += 2 * x0 + 3;
            } else {
                d += 2 * (x0 - y0) + 5;
                y0--;
            }
            x0++;
        }
    } else {
        while (x0 <= y0) {
            for (int t = -thickness / 2; t <= thickness / 2; t++) {
                draw_pixel(x + x0 + t, y + y0, color);
                draw_pixel(x - x0 + t, y + y0, color);
                draw_pixel(x + x0 + t, y - y0, color);
                draw_pixel(x - x0 + t, y - y0, color);
                draw_pixel(x + y0 + t, y + x0, color);
                draw_pixel(x - y0 + t, y + x0, color);
                draw_pixel(x + y0 + t, y - x0, color);
                draw_pixel(x - y0 + t, y - x0, color);
            }
            if (d < 0) {
                d += 2 * x0 + 3;
            } else {
                d += 2 * (x0 - y0) + 5;
                y0--;
            }
            x0++;
        }
    }
}

void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int thickness, uint32_t color, bool filled) {
    if (filled) {
        // Fill the triangle using a scanline algorithm
        // int minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
        // int maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
        // int minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
        // int maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

        // for (int y = minY; y <= maxY; y++) {
        //     for (int x = minX; x <= maxX; x++) {
        //         // Check if the pixel is inside the triangle using barycentric coordinates
        //         float alpha = ((float)(y1 - y2) * (x - x2) + (float)(x2 - x1) * (y - y2)) /
        //                       ((float)(y1 - y2) * (x0 - x2) + (float)(x2 - x1) * (y0 - y2));
        //         float beta = ((float)(y2 - y0) * (x - x2) + (float)(x0 - x1) * (y - y2)) /
        //                      ((float)(y1 - y2) * (x0 - x2) + (float)(x2 - x1) * (y0 - y2));
        //         float gamma = 1.0f - alpha - beta;

        //         if (alpha >= 0 && beta >= 0 && gamma >= 0) {
        //             draw_pixel(x, y, color);
        //         }
        //     }
        // }
    } else {
        draw_line(x0, y0, x1, y1, thickness, color);
        draw_line(x1, y1, x2, y2, thickness, color);
        draw_line(x2, y2, x0, y0, thickness, color);
    }
}

void draw_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        ((uint32_t*)framebuffer->address)[y * (framebuffer->pitch / 4) + x] = color;
    }
}