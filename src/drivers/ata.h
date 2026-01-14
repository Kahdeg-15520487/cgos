/**
 * ATA/IDE Driver for CGOS
 * 
 * PIO mode driver for primary IDE channel
 */

#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ATA I/O Ports (Primary Channel)
#define ATA_PRIMARY_DATA       0x1F0
#define ATA_PRIMARY_ERROR      0x1F1
#define ATA_PRIMARY_FEATURES   0x1F1
#define ATA_PRIMARY_SECCOUNT   0x1F2
#define ATA_PRIMARY_LBA_LO     0x1F3
#define ATA_PRIMARY_LBA_MID    0x1F4
#define ATA_PRIMARY_LBA_HI     0x1F5
#define ATA_PRIMARY_DRIVE      0x1F6
#define ATA_PRIMARY_STATUS     0x1F7
#define ATA_PRIMARY_COMMAND    0x1F7
#define ATA_PRIMARY_CONTROL    0x3F6

// ATA Commands
#define ATA_CMD_READ_SECTORS   0x20
#define ATA_CMD_WRITE_SECTORS  0x30
#define ATA_CMD_IDENTIFY       0xEC
#define ATA_CMD_FLUSH          0xE7

// ATA Status Register bits
#define ATA_SR_BSY   0x80  // Busy
#define ATA_SR_DRDY  0x40  // Drive ready
#define ATA_SR_DF    0x20  // Drive fault
#define ATA_SR_DSC   0x10  // Drive seek complete
#define ATA_SR_DRQ   0x08  // Data request ready
#define ATA_SR_CORR  0x04  // Corrected data
#define ATA_SR_IDX   0x02  // Index
#define ATA_SR_ERR   0x01  // Error

// Drive selection
#define ATA_DRIVE_MASTER 0x00
#define ATA_DRIVE_SLAVE  0x10

// Sector size
#define ATA_SECTOR_SIZE 512

// Drive info structure
typedef struct {
    bool present;
    bool is_ata;
    uint32_t size_sectors;
    char model[41];
    char serial[21];
} ata_drive_t;

// Function prototypes
int ata_init(void);
bool ata_drive_present(int drive);
int ata_read_sectors(int drive, uint32_t lba, uint8_t count, void *buffer);
int ata_write_sectors(int drive, uint32_t lba, uint8_t count, const void *buffer);
ata_drive_t *ata_get_drive_info(int drive);

#endif // ATA_H
