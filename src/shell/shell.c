/**
 * Simple Shell Implementation for CGOS
 */

#include "shell.h"
#include "../drivers/keyboard.h"
#include "../timer/timer.h"
#include "../memory/pmm.h"
#include "../pci/pci.h"
#include "../network/netdev.h"
#include "../network/network.h"
#include "../network/dhcp.h"
#include "../network/arp.h"
#include "../graphic/graphic.h"
#include "../debug/debug.h"

// Command buffer
static char cmd_buffer[SHELL_BUFFER_SIZE];
static int cmd_pos = 0;

// Screen position for shell output
static int shell_x = 10;
static int shell_y = 50;
static const int shell_start_y = 50;  // Start near top of screen
static const int line_height = 15;
static const int max_y = 750;

// Forward declarations
static void shell_execute(const char *cmd);
static int shell_strcmp(const char *s1, const char *s2);
static int shell_strncmp(const char *s1, const char *s2, int n);
static void shell_clear_line(void);

// Simple string comparison
static int shell_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int shell_strncmp(const char *s1, const char *s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void shell_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            shell_x = 10;
            shell_y += line_height;
            if (shell_y >= max_y) {
                shell_y = shell_start_y;
                // Clear old line
                draw_rect(10, shell_y, 780, line_height, 0, 0x6495ED, true);
            }
        } else {
            draw_char(shell_x, shell_y, *str, 0xFFFFFF);
            shell_x += 8;
        }
        str++;
    }
}

void shell_println(const char *str) {
    shell_print(str);
    shell_print("\n");
}

static void shell_clear_line(void) {
    draw_rect(10, shell_y, 780, line_height, 0, 0x6495ED, true);
    shell_x = 10;
}

static void shell_prompt(void) {
    shell_print(SHELL_PROMPT);
}

static void shell_newline(void) {
    shell_x = 10;
    shell_y += line_height;
    if (shell_y >= max_y) {
        shell_y = shell_start_y;
    }
    shell_clear_line();
}

// Command handlers
static void cmd_help(void) {
    shell_println("Available commands:");
    shell_println("  help    - Show this help");
    shell_println("  clear   - Clear screen");
    shell_println("  mem     - Show memory stats");
    shell_println("  pci     - List PCI devices");
    shell_println("  net     - Show network info");
    shell_println("  arp     - Show ARP table");
    shell_println("  uptime  - Show system uptime");
}

static void cmd_clear(void) {
    // Clear shell area
    draw_rect(0, shell_start_y - 20, 800, 400, 0, 0x6495ED, true);
    shell_x = 10;
    shell_y = shell_start_y;
}

static void cmd_mem(void) {
    shell_println("Memory Statistics:");
    // Get memory stats from PMM
    size_t total = physical_get_total_memory();
    size_t used = physical_get_used_memory();
    size_t free = total - used;
    
    char buf[64];
    // Simple number formatting
    shell_print("  Total: ");
    // Quick KB conversion
    uint32_t total_kb = (uint32_t)(total / 1024);
    uint32_t used_kb = (uint32_t)(used / 1024);
    uint32_t free_kb = (uint32_t)(free / 1024);
    
    kprintf_to_buffer(buf, sizeof(buf), "%u KB", total_kb);
    shell_println(buf);
    
    shell_print("  Used:  ");
    kprintf_to_buffer(buf, sizeof(buf), "%u KB", used_kb);
    shell_println(buf);
    
    shell_print("  Free:  ");
    kprintf_to_buffer(buf, sizeof(buf), "%u KB", free_kb);
    shell_println(buf);
}

static void cmd_pci(void) {
    shell_println("PCI Devices:");
    int count = pci_get_device_count();
    char buf[80];
    
    for (int i = 0; i < count && i < 6; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (dev) {
            kprintf_to_buffer(buf, sizeof(buf), "  %d:%d.%d - %04x:%04x class=%02x",
                dev->bus, dev->device, dev->function,
                dev->vendor_id, dev->device_id, dev->class_code);
            shell_println(buf);
        }
    }
}

static void cmd_net(void) {
    shell_println("Network Interfaces:");
    char buf[80];
    
    for (int i = 0; i < MAX_NETWORK_INTERFACES; i++) {
        network_interface_t *iface = network_get_interface(i);
        if (iface && iface->active) {
            kprintf_to_buffer(buf, sizeof(buf), "  %s: %02x:%02x:%02x:%02x:%02x:%02x",
                iface->name,
                iface->mac_address[0], iface->mac_address[1], iface->mac_address[2],
                iface->mac_address[3], iface->mac_address[4], iface->mac_address[5]);
            shell_println(buf);
            
            kprintf_to_buffer(buf, sizeof(buf), "    IP: %d.%d.%d.%d",
                (iface->ip_address >> 24) & 0xFF,
                (iface->ip_address >> 16) & 0xFF,
                (iface->ip_address >> 8) & 0xFF,
                iface->ip_address & 0xFF);
            shell_println(buf);
        }
    }
}

static void cmd_arp(void) {
    shell_println("ARP Table:");
    shell_println("  (ARP entries stored in memory)");
    // ARP table iteration would need an iterator API
    // For now, just confirm it's working
    arp_print_table();  // This outputs to debug console
}

static void cmd_uptime(void) {
    uint32_t seconds = timer_get_seconds();
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    
    char buf[64];
    kprintf_to_buffer(buf, sizeof(buf), "Uptime: %u:%02u:%02u", hours, minutes, seconds);
    shell_println(buf);
}

static void shell_execute(const char *cmd) {
    // Skip leading whitespace
    while (*cmd == ' ') cmd++;
    
    // Empty command
    if (*cmd == '\0') return;
    
    // Match commands
    if (shell_strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (shell_strcmp(cmd, "clear") == 0) {
        cmd_clear();
    } else if (shell_strcmp(cmd, "mem") == 0) {
        cmd_mem();
    } else if (shell_strcmp(cmd, "pci") == 0) {
        cmd_pci();
    } else if (shell_strcmp(cmd, "net") == 0) {
        cmd_net();
    } else if (shell_strcmp(cmd, "arp") == 0) {
        cmd_arp();
    } else if (shell_strcmp(cmd, "uptime") == 0) {
        cmd_uptime();
    } else {
        shell_print("Unknown command: ");
        shell_println(cmd);
        shell_println("Type 'help' for available commands");
    }
}

void shell_process_char(char c) {
    if (c == '\n' || c == '\r') {
        // Execute command
        cmd_buffer[cmd_pos] = '\0';
        shell_newline();
        shell_execute(cmd_buffer);
        cmd_pos = 0;
        shell_prompt();
    } else if (c == '\b') {
        // Backspace - erase previous character
        if (cmd_pos > 0) {
            cmd_pos--;
            shell_x -= 8;
            // Erase character by drawing filled rectangle over it
            draw_rect(shell_x, shell_y, 8, 15, 0, 0x6495ED, true);
        }
    } else if (c >= 32 && c < 127) {
        // Printable character
        if (cmd_pos < SHELL_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_pos++] = c;
            draw_char(shell_x, shell_y, c, 0xFFFFFF);
            shell_x += 8;
        }
    }
}

void shell_init(void) {
    cmd_pos = 0;
    shell_x = 10;
    shell_y = shell_start_y;
    
    // Clear entire screen for clean shell display
    clear_screen(0x6495ED);  // Royal blue background
    
    // Draw shell header bar
    draw_rect(0, 0, 800, 30, 0, 0x4169E1, true);
    draw_string(10, 8, "CGOS Shell - Type 'help' for commands", 0xFFFFFF);
    
    shell_prompt();
}

void shell_run(void) {
    // Get eth0 interface for DHCP polling
    network_interface_t *eth = network_get_interface(1);  // eth0
    dhcp_client_t *dhcp = eth ? dhcp_get_client(eth) : NULL;
    
    while (1) {
        // Process keyboard input
        if (keyboard_has_key()) {
            char c = keyboard_get_char();
            shell_process_char(c);
        }
        
        // Process network packets (keep DHCP working)
        network_process_packets();
        if (dhcp) {
            dhcp_client_update(dhcp);
        }
        
        // Small delay to reduce CPU usage
        __asm__ volatile("pause");
    }
}
