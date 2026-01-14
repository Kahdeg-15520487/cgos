/**
 * ATA/IDE Driver Implementation
 * 
 * PIO mode driver for primary IDE channel
 */

#include "ata.h"
#include "../pci/pci.h"
#include "../debug/debug.h"

// Drive information
static ata_drive_t drives[2];  // Master and slave

// Port I/O functions (from pci.h)
extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
extern uint16_t inw(uint16_t port);
extern void outw(uint16_t port, uint16_t value);

// Wait for drive to be ready
static int ata_wait_ready(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return 0;
        }
    }
    return -1;  // Timeout
}

// Wait for DRQ (data request)
static int ata_wait_drq(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_ERR) {
            return -1;  // Error
        }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return 0;
        }
    }
    return -1;  // Timeout
}

// Select drive (master or slave)
static void ata_select_drive(int drive) {
    uint8_t val = 0xA0 | ((drive & 1) << 4);
    outb(ATA_PRIMARY_DRIVE, val);
    // Small delay for drive select
    for (int i = 0; i < 15; i++) {
        inb(ATA_PRIMARY_STATUS);
    }
}

// Identify drive
static bool ata_identify(int drive) {
    ata_select_drive(drive);
    
    // Send IDENTIFY command
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    
    // Check if drive exists
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        return false;  // No drive
    }
    
    // Wait for BSY to clear
    if (ata_wait_ready() < 0) {
        return false;
    }
    
    // Check for ATAPI (not supported)
    uint8_t lba_mid = inb(ATA_PRIMARY_LBA_MID);
    uint8_t lba_hi = inb(ATA_PRIMARY_LBA_HI);
    if (lba_mid != 0 || lba_hi != 0) {
        drives[drive].is_ata = false;
        return false;  // ATAPI device, not supported
    }
    
    // Wait for data
    if (ata_wait_drq() < 0) {
        return false;
    }
    
    // Read identify data
    uint16_t identify[256];
    for (int i = 0; i < 256; i++) {
        identify[i] = inw(ATA_PRIMARY_DATA);
    }
    
    // Parse identify data
    drives[drive].present = true;
    drives[drive].is_ata = true;
    
    // Total sectors (LBA28)
    drives[drive].size_sectors = identify[60] | ((uint32_t)identify[61] << 16);
    
    // Model name (words 27-46)
    for (int i = 0; i < 20; i++) {
        drives[drive].model[i * 2] = (identify[27 + i] >> 8) & 0xFF;
        drives[drive].model[i * 2 + 1] = identify[27 + i] & 0xFF;
    }
    drives[drive].model[40] = '\0';
    
    // Trim trailing spaces
    for (int i = 39; i >= 0 && drives[drive].model[i] == ' '; i--) {
        drives[drive].model[i] = '\0';
    }
    
    // Serial number (words 10-19)
    for (int i = 0; i < 10; i++) {
        drives[drive].serial[i * 2] = (identify[10 + i] >> 8) & 0xFF;
        drives[drive].serial[i * 2 + 1] = identify[10 + i] & 0xFF;
    }
    drives[drive].serial[20] = '\0';
    
    return true;
}

int ata_init(void) {
    DEBUG_INFO("Initializing ATA driver...\n");
    
    // Disable interrupts on primary channel
    outb(ATA_PRIMARY_CONTROL, 0x02);
    
    // Initialize drive structures
    for (int i = 0; i < 2; i++) {
        drives[i].present = false;
        drives[i].is_ata = false;
        drives[i].size_sectors = 0;
        drives[i].model[0] = '\0';
        drives[i].serial[0] = '\0';
    }
    
    // Detect drives
    for (int drive = 0; drive < 2; drive++) {
        if (ata_identify(drive)) {
            uint32_t size_mb = drives[drive].size_sectors / 2048;
            DEBUG_INFO("ATA %s: %s (%u MB)\n",
                drive == 0 ? "Master" : "Slave",
                drives[drive].model,
                size_mb);
        }
    }
    
    // Check if any drives found
    if (!drives[0].present && !drives[1].present) {
        DEBUG_INFO("No ATA drives detected\n");
        return -1;
    }
    
    DEBUG_INFO("ATA driver initialized\n");
    return 0;
}

bool ata_drive_present(int drive) {
    if (drive < 0 || drive > 1) return false;
    return drives[drive].present && drives[drive].is_ata;
}

ata_drive_t *ata_get_drive_info(int drive) {
    if (drive < 0 || drive > 1) return NULL;
    return &drives[drive];
}

int ata_read_sectors(int drive, uint32_t lba, uint8_t count, void *buffer) {
    if (!ata_drive_present(drive)) return -1;
    if (count == 0) return 0;
    
    // Wait for drive ready
    if (ata_wait_ready() < 0) return -1;
    
    // Select drive with LBA mode
    uint8_t drv_sel = 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F);
    outb(ATA_PRIMARY_DRIVE, drv_sel);
    
    // Send read command
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HI, (lba >> 16) & 0xFF);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    
    // Read sectors
    uint16_t *buf = (uint16_t *)buffer;
    for (int sector = 0; sector < count; sector++) {
        // Wait for data
        if (ata_wait_drq() < 0) return -1;
        
        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            *buf++ = inw(ATA_PRIMARY_DATA);
        }
    }
    
    return count;
}

int ata_write_sectors(int drive, uint32_t lba, uint8_t count, const void *buffer) {
    if (!ata_drive_present(drive)) return -1;
    if (count == 0) return 0;
    
    // Wait for drive ready
    if (ata_wait_ready() < 0) return -1;
    
    // Select drive with LBA mode
    uint8_t drv_sel = 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F);
    outb(ATA_PRIMARY_DRIVE, drv_sel);
    
    // Send write command
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HI, (lba >> 16) & 0xFF);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    // Write sectors
    const uint16_t *buf = (const uint16_t *)buffer;
    for (int sector = 0; sector < count; sector++) {
        // Wait for DRQ
        if (ata_wait_drq() < 0) return -1;
        
        // Write 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_DATA, *buf++);
        }
    }
    
    // Flush cache
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_FLUSH);
    ata_wait_ready();
    
    return count;
}
