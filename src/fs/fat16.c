/**
 * FAT16 Filesystem Implementation
 */

#include "fat16.h"
#include "../drivers/ata.h"
#include "../memory/memory.h"
#include "../debug/debug.h"

// Filesystem state
static fat16_fs_t fs;

// Sector buffer
static uint8_t sector_buffer[512];

// FAT cache (we cache part of the FAT for performance)
static uint16_t fat_cache[256];  // 512 bytes = 256 entries
static uint32_t fat_cache_sector = 0xFFFFFFFF;

// Helper: Read a sector
static int read_sector(uint32_t lba, void *buffer) {
    return ata_read_sectors(fs.drive, lba, 1, buffer);
}

// Helper: Write a sector
static int write_sector(uint32_t lba, const void *buffer) {
    return ata_write_sectors(fs.drive, lba, 1, buffer);
}

// Helper: Convert cluster number to first sector
static uint32_t cluster_to_sector(uint16_t cluster) {
    return fs.data_start_sector + ((cluster - 2) * fs.sectors_per_cluster);
}

// Helper: Read FAT entry for a cluster
static uint16_t fat_read_entry(uint16_t cluster) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = (fat_offset % 512) / 2;
    
    // Check if we need to reload the cache
    if (fat_sector != fat_cache_sector) {
        if (read_sector(fat_sector, fat_cache) < 0) {
            return FAT16_BAD_CLUSTER;
        }
        fat_cache_sector = fat_sector;
    }
    
    return fat_cache[entry_offset];
}

// Helper: Write FAT entry
static int fat_write_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fs.fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = (fat_offset % 512) / 2;
    
    // Read the FAT sector
    if (read_sector(fat_sector, sector_buffer) < 0) {
        return -1;
    }
    
    // Update the entry
    uint16_t *fat = (uint16_t *)sector_buffer;
    fat[entry_offset] = value;
    
    // Write back to all FATs
    for (int i = 0; i < fs.num_fats; i++) {
        if (write_sector(fat_sector + (i * fs.fat_size), sector_buffer) < 0) {
            return -1;
        }
    }
    
    // Invalidate cache
    fat_cache_sector = 0xFFFFFFFF;
    
    return 0;
}

// Helper: Find a free cluster
static uint16_t fat_find_free_cluster(void) {
    for (uint16_t cluster = 2; cluster < fs.total_clusters + 2; cluster++) {
        if (fat_read_entry(cluster) == FAT16_FREE) {
            return cluster;
        }
    }
    return 0;  // No free clusters
}

// Helper: Compare 8.3 filename
static bool name_matches(const fat16_dir_entry_t *entry, const char *name) {
    char entry_name[13];
    int j = 0;
    
    // Copy name part (trim spaces)
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        entry_name[j++] = entry->name[i];
    }
    
    // Add extension if present
    if (entry->ext[0] != ' ') {
        entry_name[j++] = '.';
        for (int i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            entry_name[j++] = entry->ext[i];
        }
    }
    entry_name[j] = '\0';
    
    // Case-insensitive compare
    const char *a = entry_name;
    const char *b = name;
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

// Helper: Convert name to 8.3 format
static void name_to_83(const char *name, uint8_t *name83) {
    // Fill with spaces
    for (int i = 0; i < 11; i++) {
        name83[i] = ' ';
    }
    
    // Copy name part
    int i = 0;
    while (*name && *name != '.' && i < 8) {
        char c = *name++;
        if (c >= 'a' && c <= 'z') c -= 32;  // Uppercase
        name83[i++] = c;
    }
    
    // Skip to extension
    while (*name && *name != '.') name++;
    if (*name == '.') name++;
    
    // Copy extension
    i = 0;
    while (*name && i < 3) {
        char c = *name++;
        if (c >= 'a' && c <= 'z') c -= 32;
        name83[8 + i++] = c;
    }
}

int fat16_mount(int drive) {
    if (fs.mounted) {
        fat16_unmount();
    }
    
    DEBUG_INFO("FAT16: Mounting drive %d...\n", drive);
    
    // Check drive exists
    if (!ata_drive_present(drive)) {
        DEBUG_INFO("FAT16: Drive not present\n");
        return -1;
    }
    
    fs.drive = drive;
    
    // Read boot sector
    if (read_sector(0, sector_buffer) < 0) {
        DEBUG_INFO("FAT16: Failed to read boot sector\n");
        return -1;
    }
    
    fat16_bpb_t *bpb = (fat16_bpb_t *)sector_buffer;
    
    // Validate signature
    if (sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) {
        DEBUG_INFO("FAT16: Invalid boot signature\n");
        return -1;
    }
    
    // Check for FAT16
    if (bpb->bytes_per_sector != 512) {
        DEBUG_INFO("FAT16: Unsupported sector size\n");
        return -1;
    }
    
    // Store BPB values
    fs.bytes_per_sector = bpb->bytes_per_sector;
    fs.sectors_per_cluster = bpb->sectors_per_cluster;
    fs.reserved_sectors = bpb->reserved_sectors;
    fs.num_fats = bpb->num_fats;
    fs.root_entry_count = bpb->root_entry_count;
    fs.fat_size = bpb->fat_size_16;
    fs.total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    
    // Calculate derived values
    fs.fat_start_sector = fs.reserved_sectors;
    fs.root_dir_start = fs.fat_start_sector + (fs.num_fats * fs.fat_size);
    fs.root_dir_sectors = (fs.root_entry_count * 32 + 511) / 512;
    fs.data_start_sector = fs.root_dir_start + fs.root_dir_sectors;
    fs.total_clusters = (fs.total_sectors - fs.data_start_sector) / fs.sectors_per_cluster;
    
    // Validate FAT type
    if (fs.total_clusters < 4085 || fs.total_clusters >= 65525) {
        DEBUG_INFO("FAT16: Invalid cluster count (not FAT16)\n");
        return -1;
    }
    
    fs.mounted = true;
    fat_cache_sector = 0xFFFFFFFF;
    
    DEBUG_INFO("FAT16: Mounted successfully\n");
    DEBUG_INFO("  Clusters: %u, Cluster size: %u bytes\n", 
        fs.total_clusters, fs.sectors_per_cluster * 512);
    
    return 0;
}

void fat16_unmount(void) {
    fs.mounted = false;
    fat_cache_sector = 0xFFFFFFFF;
}

bool fat16_is_mounted(void) {
    return fs.mounted;
}

fat16_fs_t *fat16_get_fs(void) {
    return &fs;
}

int fat16_list_root(void (*callback)(const char *name, uint32_t size, bool is_dir)) {
    if (!fs.mounted) return -1;
    
    // Read root directory entries
    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        if (read_sector(fs.root_dir_start + i, sector_buffer) < 0) {
            return -1;
        }
        
        fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector_buffer;
        for (int j = 0; j < 16; j++) {  // 16 entries per sector
            fat16_dir_entry_t *entry = &entries[j];
            
            // End of directory
            if (entry->name[0] == 0x00) {
                return 0;
            }
            
            // Skip deleted entries
            if (entry->name[0] == 0xE5) continue;
            
            // Skip LFN and volume labels
            if (entry->attr == FAT_ATTR_LFN) continue;
            if (entry->attr & FAT_ATTR_VOLUME_ID) continue;
            
            // Build filename
            char name[13];
            int k = 0;
            for (int n = 0; n < 8 && entry->name[n] != ' '; n++) {
                name[k++] = entry->name[n];
            }
            if (entry->ext[0] != ' ') {
                name[k++] = '.';
                for (int n = 0; n < 3 && entry->ext[n] != ' '; n++) {
                    name[k++] = entry->ext[n];
                }
            }
            name[k] = '\0';
            
            bool is_dir = (entry->attr & FAT_ATTR_DIRECTORY) != 0;
            callback(name, entry->file_size, is_dir);
        }
    }
    
    return 0;
}

int fat16_find_file(const char *name, fat16_dir_entry_t *out_entry) {
    if (!fs.mounted) return -1;
    
    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        if (read_sector(fs.root_dir_start + i, sector_buffer) < 0) {
            return -1;
        }
        
        fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector_buffer;
        for (int j = 0; j < 16; j++) {
            fat16_dir_entry_t *entry = &entries[j];
            
            if (entry->name[0] == 0x00) return -1;  // End
            if (entry->name[0] == 0xE5) continue;    // Deleted
            if (entry->attr == FAT_ATTR_LFN) continue;
            if (entry->attr & FAT_ATTR_VOLUME_ID) continue;
            
            if (name_matches(entry, name)) {
                if (out_entry) {
                    memcpy(out_entry, entry, sizeof(fat16_dir_entry_t));
                }
                return 0;
            }
        }
    }
    
    return -1;  // Not found
}

int fat16_read_file(const char *name, void *buffer, size_t max_size) {
    if (!fs.mounted) return -1;
    
    fat16_dir_entry_t entry;
    if (fat16_find_file(name, &entry) < 0) {
        return -1;  // File not found
    }
    
    if (entry.attr & FAT_ATTR_DIRECTORY) {
        return -1;  // Can't read directories
    }
    
    size_t size = entry.file_size;
    if (size > max_size) size = max_size;
    
    uint16_t cluster = entry.cluster_lo;
    uint8_t *dst = (uint8_t *)buffer;
    size_t remaining = size;
    
    while (remaining > 0 && cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
        // Read all sectors in this cluster
        uint32_t sector = cluster_to_sector(cluster);
        for (int i = 0; i < fs.sectors_per_cluster && remaining > 0; i++) {
            if (read_sector(sector + i, sector_buffer) < 0) {
                return -1;
            }
            
            size_t copy = remaining > 512 ? 512 : remaining;
            memcpy(dst, sector_buffer, copy);
            dst += copy;
            remaining -= copy;
        }
        
        // Get next cluster
        cluster = fat_read_entry(cluster);
    }
    
    return size;
}

int fat16_create_file(const char *name) {
    if (!fs.mounted) return -1;
    
    // Check if file already exists
    if (fat16_find_file(name, NULL) == 0) {
        return -1;  // Already exists
    }
    
    // Find a free directory entry
    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        if (read_sector(fs.root_dir_start + i, sector_buffer) < 0) {
            return -1;
        }
        
        fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector_buffer;
        for (int j = 0; j < 16; j++) {
            fat16_dir_entry_t *entry = &entries[j];
            
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5) {
                // Found free entry
                memset(entry, 0, sizeof(fat16_dir_entry_t));
                name_to_83(name, entry->name);
                entry->attr = FAT_ATTR_ARCHIVE;
                entry->cluster_lo = 0;
                entry->file_size = 0;
                
                // Write back
                if (write_sector(fs.root_dir_start + i, sector_buffer) < 0) {
                    return -1;
                }
                
                return 0;
            }
        }
    }
    
    return -1;  // No free entries
}

int fat16_write_file(const char *name, const void *data, size_t size) {
    if (!fs.mounted) return -1;
    
    fat16_dir_entry_t entry;
    int entry_sector = -1, entry_index = -1;
    
    // Find the file
    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        if (read_sector(fs.root_dir_start + i, sector_buffer) < 0) {
            return -1;
        }
        
        fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector_buffer;
        for (int j = 0; j < 16; j++) {
            if (entries[j].name[0] == 0x00) goto not_found;
            if (entries[j].name[0] == 0xE5) continue;
            if (entries[j].attr == FAT_ATTR_LFN) continue;
            if (entries[j].attr & FAT_ATTR_VOLUME_ID) continue;
            
            if (name_matches(&entries[j], name)) {
                memcpy(&entry, &entries[j], sizeof(entry));
                entry_sector = fs.root_dir_start + i;
                entry_index = j;
                goto found;
            }
        }
    }
    
not_found:
    return -1;

found:
    // Free existing clusters
    if (entry.cluster_lo >= 2) {
        uint16_t cluster = entry.cluster_lo;
        while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
            uint16_t next = fat_read_entry(cluster);
            fat_write_entry(cluster, FAT16_FREE);
            cluster = next;
        }
    }
    
    // Allocate new clusters if needed
    const uint8_t *src = (const uint8_t *)data;
    size_t remaining = size;
    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;
    
    while (remaining > 0) {
        uint16_t cluster = fat_find_free_cluster();
        if (cluster == 0) {
            return -1;  // Disk full
        }
        
        if (first_cluster == 0) {
            first_cluster = cluster;
        }
        
        if (prev_cluster != 0) {
            fat_write_entry(prev_cluster, cluster);
        }
        fat_write_entry(cluster, FAT16_END_OF_CHAIN);
        
        // Write data to this cluster
        uint32_t sector = cluster_to_sector(cluster);
        for (int i = 0; i < fs.sectors_per_cluster && remaining > 0; i++) {
            memset(sector_buffer, 0, 512);
            size_t copy = remaining > 512 ? 512 : remaining;
            memcpy(sector_buffer, src, copy);
            
            if (write_sector(sector + i, sector_buffer) < 0) {
                return -1;
            }
            
            src += copy;
            remaining -= copy;
        }
        
        prev_cluster = cluster;
    }
    
    // Update directory entry
    if (read_sector(entry_sector, sector_buffer) < 0) {
        return -1;
    }
    
    fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector_buffer;
    entries[entry_index].cluster_lo = first_cluster;
    entries[entry_index].file_size = size;
    
    if (write_sector(entry_sector, sector_buffer) < 0) {
        return -1;
    }
    
    return size;
}

int fat16_delete_file(const char *name) {
    if (!fs.mounted) return -1;
    
    // Find the file
    for (uint32_t i = 0; i < fs.root_dir_sectors; i++) {
        if (read_sector(fs.root_dir_start + i, sector_buffer) < 0) {
            return -1;
        }
        
        fat16_dir_entry_t *entries = (fat16_dir_entry_t *)sector_buffer;
        for (int j = 0; j < 16; j++) {
            fat16_dir_entry_t *entry = &entries[j];
            
            if (entry->name[0] == 0x00) return -1;
            if (entry->name[0] == 0xE5) continue;
            if (entry->attr == FAT_ATTR_LFN) continue;
            
            if (name_matches(entry, name)) {
                // Free clusters
                uint16_t cluster = entry->cluster_lo;
                while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
                    uint16_t next = fat_read_entry(cluster);
                    fat_write_entry(cluster, FAT16_FREE);
                    cluster = next;
                }
                
                // Mark entry as deleted
                entry->name[0] = 0xE5;
                
                return write_sector(fs.root_dir_start + i, sector_buffer);
            }
        }
    }
    
    return -1;
}

int fat16_format(int drive, const char *volume_label) {
    DEBUG_INFO("FAT16: Formatting drive %d...\n", drive);
    
    // Check drive exists
    if (!ata_drive_present(drive)) {
        DEBUG_INFO("FAT16: Drive not present\n");
        return -1;
    }
    
    ata_drive_t *drv = ata_get_drive_info(drive);
    if (!drv) return -1;
    
    uint32_t total_sectors = drv->size_sectors;
    if (total_sectors < 8192) {  // Need at least 4MB
        DEBUG_INFO("FAT16: Drive too small\n");
        return -1;
    }
    
    // FAT16 parameters for 32MB disk
    // Sectors per cluster: 4 (2KB clusters) for small disks
    uint8_t sectors_per_cluster = 4;
    uint16_t reserved_sectors = 1;
    uint8_t num_fats = 2;
    uint16_t root_entry_count = 512;  // Standard
    
    // Calculate FAT size
    uint32_t root_dir_sectors = (root_entry_count * 32 + 511) / 512;  // 32 sectors
    uint32_t data_sectors = total_sectors - reserved_sectors - root_dir_sectors;
    uint32_t clusters = data_sectors / sectors_per_cluster;
    
    // FAT16 needs 2 bytes per cluster
    uint16_t fat_size = (clusters * 2 + 511) / 512;
    
    // Recalculate with FAT included
    data_sectors = total_sectors - reserved_sectors - (num_fats * fat_size) - root_dir_sectors;
    clusters = data_sectors / sectors_per_cluster;
    
    // Verify FAT16 range (4085 - 65524 clusters)
    if (clusters < 4085 || clusters >= 65525) {
        DEBUG_INFO("FAT16: Invalid cluster count %u\n", clusters);
        return -1;
    }
    
    DEBUG_INFO("FAT16: %u clusters, FAT size %u sectors\n", clusters, fat_size);
    
    // Create boot sector
    memset(sector_buffer, 0, 512);
    fat16_bpb_t *bpb = (fat16_bpb_t *)sector_buffer;
    
    // Jump instruction
    bpb->jump[0] = 0xEB;
    bpb->jump[1] = 0x3C;
    bpb->jump[2] = 0x90;
    
    // OEM name
    memcpy(bpb->oem_name, "CGOS    ", 8);
    
    // BPB
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = sectors_per_cluster;
    bpb->reserved_sectors = reserved_sectors;
    bpb->num_fats = num_fats;
    bpb->root_entry_count = root_entry_count;
    bpb->total_sectors_16 = total_sectors <= 65535 ? total_sectors : 0;
    bpb->total_sectors_32 = total_sectors > 65535 ? total_sectors : 0;
    bpb->media_type = 0xF8;  // Fixed disk
    bpb->fat_size_16 = fat_size;
    bpb->sectors_per_track = 63;
    bpb->num_heads = 16;
    bpb->hidden_sectors = 0;
    
    // Extended BPB
    bpb->drive_number = 0x80;
    bpb->boot_sig = 0x29;
    bpb->volume_id = 0x12345678;  // Random-ish
    
    // Volume label
    if (volume_label) {
        int i;
        for (i = 0; i < 11 && volume_label[i]; i++) {
            char c = volume_label[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            bpb->volume_label[i] = c;
        }
        for (; i < 11; i++) {
            bpb->volume_label[i] = ' ';
        }
    } else {
        memcpy(bpb->volume_label, "NO NAME    ", 11);
    }
    
    memcpy(bpb->fs_type, "FAT16   ", 8);
    
    // Boot signature
    sector_buffer[510] = 0x55;
    sector_buffer[511] = 0xAA;
    
    // Write boot sector
    if (ata_write_sectors(drive, 0, 1, sector_buffer) < 0) {
        DEBUG_INFO("FAT16: Failed to write boot sector\n");
        return -1;
    }
    
    // Initialize FAT (first two entries are reserved)
    memset(sector_buffer, 0, 512);
    uint16_t *fat = (uint16_t *)sector_buffer;
    fat[0] = 0xFFF8;  // Media type
    fat[1] = 0xFFFF;  // End of chain marker
    
    // Write first FAT sector for each FAT copy
    for (int f = 0; f < num_fats; f++) {
        uint32_t fat_start = reserved_sectors + (f * fat_size);
        if (ata_write_sectors(drive, fat_start, 1, sector_buffer) < 0) {
            DEBUG_INFO("FAT16: Failed to write FAT\n");
            return -1;
        }
        
        // Clear remaining FAT sectors
        memset(sector_buffer, 0, 512);
        for (uint32_t s = 1; s < fat_size; s++) {
            if (ata_write_sectors(drive, fat_start + s, 1, sector_buffer) < 0) {
                return -1;
            }
        }
    }
    
    // Clear root directory
    memset(sector_buffer, 0, 512);
    uint32_t root_start = reserved_sectors + (num_fats * fat_size);
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (ata_write_sectors(drive, root_start + s, 1, sector_buffer) < 0) {
            DEBUG_INFO("FAT16: Failed to write root dir\n");
            return -1;
        }
    }
    
    // Add volume label entry if specified
    if (volume_label) {
        fat16_dir_entry_t *entry = (fat16_dir_entry_t *)sector_buffer;
        memset(entry, 0, sizeof(fat16_dir_entry_t));
        
        int i;
        for (i = 0; i < 11 && volume_label[i]; i++) {
            char c = volume_label[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            entry->name[i] = c;
        }
        for (; i < 8; i++) entry->name[i] = ' ';
        for (i = 0; i < 3; i++) entry->ext[i] = ' ';
        
        entry->attr = FAT_ATTR_VOLUME_ID;
        
        if (ata_write_sectors(drive, root_start, 1, sector_buffer) < 0) {
            return -1;
        }
    }
    
    DEBUG_INFO("FAT16: Format complete\n");
    return 0;
}
