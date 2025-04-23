#ifndef GRAPHIC_H
#define GRAPHIC_H

#include <stdint.h>
#include <stdarg.h>
#include <limine.h>

// setup
void setup_graphic(volatile struct limine_framebuffer_request *framebuffer_request);

// string functions
void draw_char(int x, int y, char c, uint32_t color);
void draw_string(int x, int y, const char *str, uint32_t color);
void kprintf(int x,int y, const char *format, ...);

// primitive functions
void clear_screen(uint32_t color);
void draw_pixel(int x, int y, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, int thickness, uint32_t color);
void draw_rect(int x, int y, int width, int height, int thickness, uint32_t color, bool filled);
void draw_circle(int x, int y, int radius, int thickness, uint32_t color, bool filled);
void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int thickness, uint32_t color, bool filled);

#endif // GRAPHIC_H