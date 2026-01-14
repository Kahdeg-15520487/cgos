/**
 * FAT16 Filesystem Driver for CGOS
 */

#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// FAT16 Boot Sector / BPB structure
typedef struct __attribute__((packed)) {
    uint8_t  jump[3];           // Jump instruction
    uint8_t  oem_name[8];       // OEM name
    uint16_t bytes_per_sector;  // Usually 512
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;  // Usually 1
    uint8_t  num_fats;          // Usually 2
    uint16_t root_entry_count;  // For FAT16: 512
    uint16_t total_sectors_16;  // If 0, use total_sectors_32
    uint8_t  media_type;
    uint16_t fat_size_16;       // Sectors per FAT
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    // Extended BPB
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;          // Should be 0x29
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];        // "FAT16   "
} fat16_bpb_t;

// Directory entry (32 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  name[8];           // Filename (space padded)
    uint8_t  ext[3];            // Extension
    uint8_t  attr;              // File attributes
    uint8_t  reserved;
    uint8_t  create_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_hi;        // Always 0 for FAT16
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_lo;        // Starting cluster
    uint32_t file_size;
} fat16_dir_entry_t;

// File attributes
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F  // Long filename entry

// FAT16 cluster values
#define FAT16_FREE          0x0000
#define FAT16_RESERVED      0x0001
#define FAT16_BAD_CLUSTER   0xFFF7
#define FAT16_END_OF_CHAIN  0xFFF8  // 0xFFF8-0xFFFF = end of chain

// FAT16 filesystem state
typedef struct {
    bool mounted;
    int drive;
    
    // BPB values
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t fat_size;
    uint32_t total_sectors;
    
    // Calculated values
    uint32_t fat_start_sector;
    uint32_t root_dir_start;
    uint32_t root_dir_sectors;
    uint32_t data_start_sector;
    uint32_t total_clusters;
} fat16_fs_t;

// Simple file handle
typedef struct {
    bool in_use;
    uint16_t start_cluster;
    uint32_t file_size;
    uint32_t position;
    char name[13];  // 8.3 format
} fat16_file_t;

// Function prototypes
int fat16_mount(int drive);
void fat16_unmount(void);
bool fat16_is_mounted(void);

// Directory operations
int fat16_list_root(void (*callback)(const char *name, uint32_t size, bool is_dir));
int fat16_find_file(const char *name, fat16_dir_entry_t *entry);

// File operations
int fat16_read_file(const char *name, void *buffer, size_t max_size);
int fat16_write_file(const char *name, const void *data, size_t size);
int fat16_create_file(const char *name);
int fat16_delete_file(const char *name);

// Get filesystem info
fat16_fs_t *fat16_get_fs(void);

// Format a drive with FAT16
int fat16_format(int drive, const char *volume_label);

#endif // FAT16_H
