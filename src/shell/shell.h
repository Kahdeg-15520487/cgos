/**
 * Simple Shell for CGOS
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include <stdbool.h>

// Shell command buffer size
#define SHELL_BUFFER_SIZE 256

// Shell prompt
#define SHELL_PROMPT "cgos> "

// Function prototypes
void shell_init(void);
void shell_run(void);
void shell_process_char(unsigned char c);
void shell_print(const char *str);
void shell_println(const char *str);

#endif // SHELL_H
