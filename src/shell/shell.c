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
#include "../network/icmp.h"
#include "../fs/fat16.h"
#include "../acpi/acpi.h"
#include "../drivers/ata.h"
#include "../graphic/graphic.h"
#include "../debug/debug.h"
#include "../sched/thread.h"

// Command buffer
static char cmd_buffer[SHELL_BUFFER_SIZE];
static int cmd_pos = 0;

// Command history
#define HISTORY_SIZE 16
static char history[HISTORY_SIZE][SHELL_BUFFER_SIZE];
static int history_count = 0;
static int history_pos = 0;  // Current position when navigating

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
    shell_println("  ping    - Ping an IP address");
    shell_println("  ls      - List files");
    shell_println("  cat     - Display file contents");
    shell_println("  shutdown- Power off");
    shell_println("  reboot  - Restart system");
    shell_println("  disk    - List disk drives");
    shell_println("  format  - Format a drive with FAT16");
    shell_println("  write   - Write text to file");
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

// List directory callback
static void ls_callback(const char *name, uint32_t size, bool is_dir) {
    char buf[80];
    if (is_dir) {
        kprintf_to_buffer(buf, sizeof(buf), "  [DIR] %s", name);
    } else {
        kprintf_to_buffer(buf, sizeof(buf), "  %s (%u bytes)", name, size);
    }
    shell_println(buf);
}

static void cmd_ls(void) {
    if (!fat16_is_mounted()) {
        shell_println("No filesystem mounted");
        return;
    }
    
    shell_println("Files:");
    if (fat16_list_root(ls_callback) < 0) {
        shell_println("Error reading directory");
    }
}

static void cmd_cat(const char *args) {
    // Skip whitespace
    while (*args == ' ') args++;
    
    if (*args == '\0') {
        shell_println("Usage: cat <filename>");
        return;
    }
    
    if (!fat16_is_mounted()) {
        shell_println("No filesystem mounted");
        return;
    }
    
    // Read file (max 4KB to display)
    static char file_buf[4096];
    int size = fat16_read_file(args, file_buf, sizeof(file_buf) - 1);
    
    if (size < 0) {
        shell_println("File not found");
        return;
    }
    
    file_buf[size] = '\0';
    
    // Print file contents line by line
    char *ptr = file_buf;
    char line[256];
    int line_pos = 0;
    
    while (*ptr) {
        if (*ptr == '\n' || *ptr == '\r') {
            line[line_pos] = '\0';
            if (line_pos > 0) {
                shell_println(line);
            }
            line_pos = 0;
            if (*ptr == '\r' && *(ptr + 1) == '\n') ptr++;
        } else if (line_pos < 255) {
            line[line_pos++] = *ptr;
        }
        ptr++;
    }
    
    // Print remaining line
    if (line_pos > 0) {
        line[line_pos] = '\0';
        shell_println(line);
    }
}

static void cmd_write(const char *args) {
    // Skip whitespace
    while (*args == ' ') args++;
    
    if (*args == '\0') {
        shell_println("Usage: write <filename> <text>");
        shell_println("Example: write test.txt Hello World");
        return;
    }
    
    if (!fat16_is_mounted()) {
        shell_println("No filesystem mounted");
        return;
    }
    
    // Parse filename (until space), skip quotes
    char filename[13];
    int i = 0;
    
    // Skip opening quote if present
    if (*args == '\'' || *args == '"') args++;
    
    while (*args && *args != ' ' && *args != '\'' && *args != '"' && i < 12) {
        filename[i++] = *args++;
    }
    filename[i] = '\0';
    
    // Skip closing quote and space
    while (*args == '\'' || *args == '"' || *args == ' ') args++;
    
    if (*args == '\0') {
        shell_println("Usage: write <filename> <text>");
        return;
    }
    
    // Parse content - strip surrounding quotes if present
    const char *content = args;
    size_t len = 0;
    
    // Skip opening quote for content
    if (*content == '\'' || *content == '"') {
        char quote_char = *content;
        content++;
        
        // Find end of quoted content
        const char *p = content;
        while (*p && *p != quote_char) {
            len++;
            p++;
        }
    } else {
        // No quotes - use entire remaining string
        const char *p = content;
        while (*p++) len++;
    }
    
    // Create file if it doesn't exist
    if (fat16_find_file(filename, NULL) < 0) {
        if (fat16_create_file(filename) < 0) {
            shell_println("Failed to create file");
            return;
        }
    }
    
    if (fat16_write_file(filename, content, len) < 0) {
        shell_println("Failed to write file");
        return;
    }
    
    char buf[64];
    kprintf_to_buffer(buf, sizeof(buf), "Wrote %u bytes to %s", (uint32_t)len, filename);
    shell_println(buf);
}

static void cmd_disk(void) {
    shell_println("ATA Drives:");
    char buf[80];
    bool found = false;
    
    for (int i = 0; i < 2; i++) {
        ata_drive_t *drive = ata_get_drive_info(i);
        if (drive && drive->present) {
            found = true;
            uint32_t size_mb = drive->size_sectors / 2048;
            kprintf_to_buffer(buf, sizeof(buf), "  Drive %d (%s): %s (%u MB)",
                i, i == 0 ? "Master" : "Slave",
                drive->model, size_mb);
            shell_println(buf);
        }
    }
    
    if (!found) {
        shell_println("  No drives detected");
    }
    
    // Show mounted filesystem
    if (fat16_is_mounted()) {
        shell_println("Mounted: FAT16 filesystem");
    } else {
        shell_println("No filesystem mounted");
    }
}

static void cmd_format(const char *args) {
    // Skip whitespace
    while (*args == ' ') args++;
    
    // Parse drive number
    int drive = -1;
    if (*args >= '0' && *args <= '1') {
        drive = *args - '0';
    } else {
        // Auto-detect first available drive
        for (int i = 0; i < 2; i++) {
            if (ata_drive_present(i)) {
                drive = i;
                break;
            }
        }
    }
    
    if (drive < 0) {
        shell_println("Usage: format [0|1]");
        shell_println("No drives available");
        return;
    }
    
    char buf[64];
    kprintf_to_buffer(buf, sizeof(buf), "Formatting drive %d with FAT16...", drive);
    shell_println(buf);
    
    // Unmount if needed
    fat16_unmount();
    
    if (fat16_format(drive, "CGOS") == 0) {
        shell_println("Format complete!");
        
        // Try to mount it
        if (fat16_mount(drive) == 0) {
            shell_println("Filesystem mounted");
        }
    } else {
        shell_println("Format failed");
    }
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

// Simple IP address parser (e.g., "10.0.2.2")
static uint32_t parse_ip(const char *str) {
    uint32_t ip = 0;
    int octet = 0;
    int count = 0;
    
    while (*str && count < 4) {
        if (*str >= '0' && *str <= '9') {
            octet = octet * 10 + (*str - '0');
        } else if (*str == '.') {
            ip = (ip << 8) | (octet & 0xFF);
            octet = 0;
            count++;
        } else {
            break;
        }
        str++;
    }
    
    // Last octet
    if (count == 3) {
        ip = (ip << 8) | (octet & 0xFF);
    }
    
    return ip;
}

static void cmd_ping(const char *args) {
    // Skip "ping "
    while (*args == ' ') args++;
    
    if (*args == '\0') {
        shell_println("Usage: ping <ip>");
        shell_println("Example: ping 10.0.2.2");
        return;
    }
    
    uint32_t dest_ip = parse_ip(args);
    if (dest_ip == 0) {
        shell_println("Invalid IP address");
        return;
    }
    
    char buf[80];
    kprintf_to_buffer(buf, sizeof(buf), "Pinging %d.%d.%d.%d...",
        (dest_ip >> 24) & 0xFF, (dest_ip >> 16) & 0xFF,
        (dest_ip >> 8) & 0xFF, dest_ip & 0xFF);
    shell_println(buf);
    
    // Get eth0 interface
    network_interface_t *iface = network_get_interface(1);
    if (!iface) {
        shell_println("No network interface");
        return;
    }
    
    // Ping 4 times
    ping_result_t result;
    icmp_ping(iface, dest_ip, 4, &result);
    
    // Show results
    shell_println("");
    kprintf_to_buffer(buf, sizeof(buf), "Sent: %d, Received: %d",
        result.sent, result.received);
    shell_println(buf);
    
    if (result.received > 0) {
        uint32_t avg = result.total_time / result.received;
        kprintf_to_buffer(buf, sizeof(buf), "RTT: min=%u avg=%u max=%u ms",
            result.min_time, avg, result.max_time);
        shell_println(buf);
    } else {
        shell_println("No reply received");
    }
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
    } else if (shell_strncmp(cmd, "ping ", 5) == 0 || shell_strcmp(cmd, "ping") == 0) {
        cmd_ping(cmd + 4);
    } else if (shell_strcmp(cmd, "ls") == 0) {
        cmd_ls();
    } else if (shell_strcmp(cmd, "cat") == 0 || shell_strncmp(cmd, "cat ", 4) == 0) {
        cmd_cat(cmd + 3);
    } else if (shell_strcmp(cmd, "write") == 0 || shell_strncmp(cmd, "write ", 6) == 0) {
        cmd_write(cmd + 5);
    } else if (shell_strcmp(cmd, "shutdown") == 0) {
        shell_println("Shutting down...");
        acpi_shutdown();
    } else if (shell_strcmp(cmd, "reboot") == 0) {
        shell_println("Rebooting...");
        acpi_reboot();
    } else if (shell_strcmp(cmd, "disk") == 0) {
        cmd_disk();
    } else if (shell_strcmp(cmd, "format") == 0 || shell_strncmp(cmd, "format ", 7) == 0) {
        cmd_format(cmd + 6);
    } else {
        shell_print("Unknown command: ");
        shell_println(cmd);
        shell_println("Type 'help' for available commands");
    }
}

// Helper to clear and redraw current command line
static void shell_redraw_cmd(void) {
    // Clear from prompt to end of line
    int prompt_len = 6;  // "cgos> "
    shell_x = 10 + prompt_len * 8;
    draw_rect(shell_x, shell_y, 780 - shell_x, line_height, 0, 0x6495ED, true);
    
    // Redraw command
    for (int i = 0; i < cmd_pos; i++) {
        draw_char(shell_x, shell_y, cmd_buffer[i], 0xFFFFFF);
        shell_x += 8;
    }
}

// Helper to copy string
static void shell_strcpy(char *dest, const char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Helper for string length
static int shell_strlen(const char *s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

void shell_process_char(unsigned char c) {
    if (c == '\n' || c == '\r') {
        // Execute command
        cmd_buffer[cmd_pos] = '\0';
        
        // Save non-empty commands to history
        if (cmd_pos > 0) {
            // Shift history if full
            if (history_count < HISTORY_SIZE) {
                shell_strcpy(history[history_count], cmd_buffer);
                history_count++;
            } else {
                // Shift all entries up
                for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                    shell_strcpy(history[i], history[i + 1]);
                }
                shell_strcpy(history[HISTORY_SIZE - 1], cmd_buffer);
            }
        }
        history_pos = history_count;  // Reset to end
        
        shell_newline();
        shell_execute(cmd_buffer);
        cmd_pos = 0;
        shell_prompt();
    } else if (c == '\b') {
        // Backspace - erase previous character
        if (cmd_pos > 0) {
            cmd_pos--;
            shell_x -= 8;
            draw_rect(shell_x, shell_y, 8, 15, 0, 0x6495ED, true);
        }
    } else if (c == SPECIAL_KEY_ESC) {
        // ESC - clear current command
        cmd_pos = 0;
        cmd_buffer[0] = '\0';
        shell_redraw_cmd();
    } else if (c == SPECIAL_KEY_UP) {
        // Up arrow - previous history
        if (history_pos > 0) {
            history_pos--;
            shell_strcpy(cmd_buffer, history[history_pos]);
            cmd_pos = shell_strlen(cmd_buffer);
            shell_redraw_cmd();
        }
    } else if (c == SPECIAL_KEY_DOWN) {
        // Down arrow - next history
        if (history_pos < history_count - 1) {
            history_pos++;
            shell_strcpy(cmd_buffer, history[history_pos]);
            cmd_pos = shell_strlen(cmd_buffer);
            shell_redraw_cmd();
        } else if (history_pos < history_count) {
            // At end, clear command
            history_pos = history_count;
            cmd_pos = 0;
            cmd_buffer[0] = '\0';
            shell_redraw_cmd();
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
    
    static uint32_t loop_count = 0;
    
    DEBUG_INFO("Shell loop starting...\n");
    
    while (1) {
        loop_count++;
        
        // Debug: check RFLAGS every 100 iterations
        if (loop_count % 100 == 0) {
            uint64_t rflags;
            __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
            DEBUG_INFO("Shell loop %u: IF=%d, has_key=%d\n", 
                       loop_count, (int)((rflags >> 9) & 1), keyboard_has_key());
        }
        
        // Process keyboard input
        if (keyboard_has_key()) {
            char c = keyboard_get_char();
            shell_process_char(c);
        } else {
            // No key available - yield CPU to other threads
            // This makes the shell appear I/O-bound (low CPU usage)
            // and prevents priority demotion
            thread_yield();
        }
        
        // Process network packets (keep DHCP working)
        network_process_packets();
        if (dhcp) {
            dhcp_client_update(dhcp);
        }
    }
}
