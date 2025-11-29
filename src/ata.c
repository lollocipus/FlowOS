#include "ata.h"
#include "types.h"

#define MAX_DRIVES 4

static struct ata_drive drives[MAX_DRIVES];
static uint8_t drive_count = 0;

static void ata_wait_bsy(uint16_t io_base) {
    while (inb(io_base + ATA_REG_STATUS) & ATA_SR_BSY);
}

static void ata_wait_drq(uint16_t io_base) {
    while (!(inb(io_base + ATA_REG_STATUS) & ATA_SR_DRQ));
}

static bool ata_wait_ready(uint16_t io_base) {
    uint8_t status;
    int timeout = 100000;
    
    while (timeout--) {
        status = inb(io_base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return false;
        if (status & ATA_SR_DF) return false;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return true;
    }
    return false;
}

static void ata_soft_reset(uint16_t ctrl_base) {
    outb(ctrl_base, 0x04);  // Set SRST bit
    io_wait();
    io_wait();
    io_wait();
    io_wait();
    outb(ctrl_base, 0x00);  // Clear SRST bit
    io_wait();
}

static bool ata_identify(uint16_t io_base, uint16_t ctrl_base, uint8_t drive_sel, struct ata_drive* drive) {
    // Select drive
    outb(io_base + ATA_REG_DRIVE, 0xA0 | drive_sel);
    io_wait();
    
    // Clear sector count and LBA registers
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LO, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HI, 0);
    
    // Send IDENTIFY command
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();
    
    // Check if drive exists
    uint8_t status = inb(io_base + ATA_REG_STATUS);
    if (status == 0) return false;  // No drive
    
    // Wait for BSY to clear
    ata_wait_bsy(io_base);
    
    // Check for ATAPI
    uint8_t lba_mid = inb(io_base + ATA_REG_LBA_MID);
    uint8_t lba_hi = inb(io_base + ATA_REG_LBA_HI);
    
    if (lba_mid != 0 || lba_hi != 0) {
        // This is ATAPI or SATA, skip for now
        drive->is_atapi = true;
        return false;
    }
    
    // Wait for DRQ or ERR
    while (1) {
        status = inb(io_base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return false;
        if (status & ATA_SR_DRQ) break;
    }
    
    // Read identification data
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(io_base + ATA_REG_DATA);
    }
    
    // Extract drive info
    drive->present = true;
    drive->is_atapi = false;
    drive->io_base = io_base;
    drive->ctrl_base = ctrl_base;
    drive->drive_select = drive_sel;
    
    // Total sectors (LBA28)
    drive->sectors = (identify_data[61] << 16) | identify_data[60];
    
    // Model string (bytes are swapped in each word)
    for (int i = 0; i < 20; i++) {
        drive->model[i * 2] = (identify_data[27 + i] >> 8) & 0xFF;
        drive->model[i * 2 + 1] = identify_data[27 + i] & 0xFF;
    }
    drive->model[40] = '\0';
    
    // Trim trailing spaces
    for (int i = 39; i >= 0 && drive->model[i] == ' '; i--) {
        drive->model[i] = '\0';
    }
    
    // Serial number
    for (int i = 0; i < 10; i++) {
        drive->serial[i * 2] = (identify_data[10 + i] >> 8) & 0xFF;
        drive->serial[i * 2 + 1] = identify_data[10 + i] & 0xFF;
    }
    drive->serial[20] = '\0';
    
    return true;
}

void ata_init(void) {
    drive_count = 0;
    
    // Clear drive info
    for (int i = 0; i < MAX_DRIVES; i++) {
        drives[i].present = false;
    }
    
    // Check primary master
    if (ata_identify(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, ATA_MASTER, &drives[0])) {
        drive_count++;
    }
    
    // Check primary slave
    if (ata_identify(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, ATA_SLAVE, &drives[1])) {
        drive_count++;
    }
    
    // Check secondary master
    if (ata_identify(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, ATA_MASTER, &drives[2])) {
        drive_count++;
    }
    
    // Check secondary slave
    if (ata_identify(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, ATA_SLAVE, &drives[3])) {
        drive_count++;
    }
}

bool ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void* buffer) {
    if (drive >= MAX_DRIVES || !drives[drive].present) {
        return false;
    }
    
    struct ata_drive* d = &drives[drive];
    uint16_t* buf = (uint16_t*)buffer;
    
    // Wait for drive to be ready
    ata_wait_bsy(d->io_base);
    
    // Select drive and set LBA mode
    outb(d->io_base + ATA_REG_DRIVE, 0xE0 | d->drive_select | ((lba >> 24) & 0x0F));
    
    // Set sector count and LBA
    outb(d->io_base + ATA_REG_SECCOUNT, count);
    outb(d->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(d->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(d->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    
    // Send read command
    outb(d->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    // Read sectors
    for (int s = 0; s < count; s++) {
        if (!ata_wait_ready(d->io_base)) {
            return false;
        }
        
        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(d->io_base + ATA_REG_DATA);
        }
    }
    
    return true;
}

bool ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void* buffer) {
    if (drive >= MAX_DRIVES || !drives[drive].present) {
        return false;
    }
    
    struct ata_drive* d = &drives[drive];
    const uint16_t* buf = (const uint16_t*)buffer;
    
    // Wait for drive to be ready
    ata_wait_bsy(d->io_base);
    
    // Select drive and set LBA mode
    outb(d->io_base + ATA_REG_DRIVE, 0xE0 | d->drive_select | ((lba >> 24) & 0x0F));
    
    // Set sector count and LBA
    outb(d->io_base + ATA_REG_SECCOUNT, count);
    outb(d->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(d->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(d->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    
    // Send write command
    outb(d->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    // Write sectors
    for (int s = 0; s < count; s++) {
        ata_wait_drq(d->io_base);
        
        // Write 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(d->io_base + ATA_REG_DATA, buf[s * 256 + i]);
        }
    }
    
    // Flush cache
    outb(d->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH);
    ata_wait_bsy(d->io_base);
    
    return true;
}

struct ata_drive* ata_get_drive(uint8_t drive) {
    if (drive >= MAX_DRIVES) return NULL;
    return drives[drive].present ? &drives[drive] : NULL;
}

uint8_t ata_get_drive_count(void) {
    return drive_count;
}
