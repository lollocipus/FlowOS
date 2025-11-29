#include "fat32.h"
#include "ata.h"
#include "vfs.h"
#include "heap.h"

// Extern logging
extern void log_info(const char* msg);

// Helper function to convert uint32 to string
static void uitoa_simple(uint32_t value, char* buffer) {
    char tmp[12];
    int idx = 0;
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    while (value > 0) {
        tmp[idx++] = '0' + (value % 10);
        value /= 10;
    }
    for (int i = 0; i < idx; i++) {
        buffer[i] = tmp[idx - i - 1];
    }
    buffer[idx] = '\0';
}

// Global FAT32 state
static fat_bpb_t bpb;
static uint32_t fat_start_sector;
static uint32_t data_start_sector;
static uint32_t root_cluster;

// Helper to read a cluster
static bool read_cluster(uint32_t cluster, uint8_t* buffer) {
    uint32_t lba = data_start_sector + (cluster - 2) * bpb.sectors_per_cluster;
    return ata_read_sectors(0, lba, bpb.sectors_per_cluster, buffer);
}

// Helper to get next cluster from FAT
static uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / bpb.bytes_per_sector);
    uint32_t ent_offset = fat_offset % bpb.bytes_per_sector;
    
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    if (!ata_read_sectors(0, fat_sector, 1, buffer)) {
        kfree(buffer);
        return 0x0FFFFFFF;
    }
    
    uint32_t next_cluster = *(uint32_t*)&buffer[ent_offset];
    next_cluster &= 0x0FFFFFFF;
    
    kfree(buffer);
    return next_cluster;
}

static uint32_t fat32_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t cluster = node->inode;
    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    
    // Skip clusters to reach offset
    uint32_t current_offset = 0;
    while (offset >= cluster_size) {
        cluster = get_next_cluster(cluster);
        if (cluster >= 0x0FFFFFF8) return 0; // End of chain
        offset -= cluster_size;
    }
    
    uint32_t read_size = 0;
    uint8_t* cluster_buffer = (uint8_t*)kmalloc(cluster_size);
    
    while (size > 0) {
        if (!read_cluster(cluster, cluster_buffer)) {
            break;
        }
        
        uint32_t chunk_size = cluster_size - offset;
        if (chunk_size > size) chunk_size = size;
        
        for (uint32_t i = 0; i < chunk_size; i++) {
            buffer[read_size + i] = cluster_buffer[offset + i];
        }
        
        read_size += chunk_size;
        size -= chunk_size;
        offset = 0; // Next clusters start at 0
        
        if (size > 0) {
            cluster = get_next_cluster(cluster);
            if (cluster >= 0x0FFFFFF8) break;
        }
    }
    
    kfree(cluster_buffer);
    return read_size;
}

// VFS Callbacks
static struct dirent* fat32_readdir(fs_node_t* node, uint32_t index) {
    uint32_t cluster = node->inode;
    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t dir_entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);
    
    // Debug
    if (index == 0) {
        log_info("FAT32: readdir called");
        char buf[32];
        uitoa_simple(cluster, buf);
        log_info("Root cluster:");
        log_info(buf);
        uitoa_simple(cluster_size, buf);
        log_info("Cluster size:");
        log_info(buf);
    }
    
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    if (!buffer) {
        log_info("FAT32: readdir OOM");
        return 0;
    }
    
    uint32_t current_index = 0;
    
    while (cluster < 0x0FFFFFF8) {
        if (!read_cluster(cluster, buffer)) {
            log_info("FAT32: read_cluster failed");
            kfree(buffer);
            break;
        }
        
        fat_dir_entry_t* entries = (fat_dir_entry_t*)buffer;
        
        for (uint32_t i = 0; i < dir_entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) { // End of dir
                kfree(buffer);
                return 0;
            }
            if (entries[i].name[0] == 0xE5) continue; // Deleted
            if (entries[i].attr & ATTR_LONG_NAME) continue; // Skip LFN for now
            
            if (current_index == index) {
                struct dirent* dirent = (struct dirent*)kmalloc(sizeof(struct dirent));
                
                // Copy name
                int j = 0;
                for (int k = 0; k < 8; k++) {
                    if (entries[i].name[k] != ' ') dirent->name[j++] = entries[i].name[k];
                }
                if (entries[i].name[8] != ' ') {
                    dirent->name[j++] = '.';
                    for (int k = 8; k < 11; k++) {
                        if (entries[i].name[k] != ' ') dirent->name[j++] = entries[i].name[k];
                    }
                }
                dirent->name[j] = 0;
                
                dirent->inode = (entries[i].fst_clus_hi << 16) | entries[i].fst_clus_lo;
                kfree(buffer);
                return dirent;
            }
            current_index++;
        }
        
        cluster = get_next_cluster(cluster);
    }
    
    kfree(buffer);
    return 0;
}

static fs_node_t* fat32_finddir(fs_node_t* node, char* name) {
    uint32_t cluster = node->inode;
    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t dir_entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);
    
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    
    while (cluster < 0x0FFFFFF8) {
        if (!read_cluster(cluster, buffer)) break;
        
        fat_dir_entry_t* entries = (fat_dir_entry_t*)buffer;
        
        for (uint32_t i = 0; i < dir_entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) {
                kfree(buffer);
                return 0;
            }
            if (entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr & ATTR_LONG_NAME) continue;
            
            char entry_name[13];
            int j = 0;
            for (int k = 0; k < 8; k++) {
                if (entries[i].name[k] != ' ') entry_name[j++] = entries[i].name[k];
            }
            if (entries[i].name[8] != ' ') {
                entry_name[j++] = '.';
                for (int k = 8; k < 11; k++) {
                    if (entries[i].name[k] != ' ') entry_name[j++] = entries[i].name[k];
                }
            }
            entry_name[j] = 0;
            
            // Simple string compare
            int match = 1;
            for (int k = 0; name[k] != 0 || entry_name[k] != 0; k++) {
                if (name[k] != entry_name[k]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                fs_node_t* file_node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
                file_node->inode = (entries[i].fst_clus_hi << 16) | entries[i].fst_clus_lo;
                file_node->length = entries[i].file_size;
                
                if (entries[i].attr & ATTR_DIRECTORY) {
                    file_node->flags = FS_DIRECTORY;
                    file_node->readdir = fat32_readdir;
                    file_node->finddir = fat32_finddir;
                } else {
                    file_node->flags = FS_FILE;
                    file_node->read = fat32_read;
                }
                
                kfree(buffer);
                return file_node;
            }
        }
        
        cluster = get_next_cluster(cluster);
    }
    
    kfree(buffer);
    return 0;
}

void fat32_init(void) {
    log_info("FAT32: Initializing...");
    
    uint8_t* sector0 = (uint8_t*)kmalloc(512);
    if (!ata_read_sectors(0, 0, 1, sector0)) {
        log_info("FAT32: Failed to read boot sector");
        kfree(sector0);
        return;
    }

    fat_bpb_t* bpb_ptr = (fat_bpb_t*)sector0;
    bpb = *bpb_ptr;
    kfree(sector0);

    // Validate BPB to prevent divide-by-zero
    if (bpb.bytes_per_sector == 0 || bpb.sectors_per_cluster == 0) {
        log_info("FAT32: Invalid BPB (not a FAT32 volume)");
        return;
    }
    
    if (bpb.fat_size_32 == 0 || bpb.num_fats == 0) {
        log_info("FAT32: Invalid FAT32 parameters");
        return;
    }

    // Calculate offsets
    fat_start_sector = bpb.reserved_sectors;
    data_start_sector = bpb.reserved_sectors + (bpb.num_fats * bpb.fat_size_32);
    root_cluster = bpb.root_cluster;

    log_info("FAT32: Volume found");

    // Setup root node
    fs_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    fs_root->inode = root_cluster;
    fs_root->flags = FS_DIRECTORY;
    fs_root->readdir = fat32_readdir;
    fs_root->finddir = fat32_finddir;
}
