// FAT manipulation functions

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

#include "fat.h"
#include "safe.h"

const int DIR_ENTRY_SIZE = 64;

const int ROOTDIR = 1;
const int DEFAULT_PERMISSIONS = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // octal 0644

const int LASTBLOCK = 0xFFFF; // -1 16bit
const int BITS_PER_BYTE = 8;
const int BYTE_SIZE = 1 << BITS_PER_BYTE; // 256

// uint8_t type
const int FILETYPE_UNKNOWN =    0;
const int FILETYPE_FILE =       1;
const int FILETYPE_DIRECTORY =  2;
const int FILETYPE_LINK =       4;
// char name[32]
const int FILENAME_ENDDIR =     0;
const int FILENAME_DEL_UNUSED = 1;
const int FILENAME_DEL_INUSE =  2;
// uint8_t perm
const int FILEPERM_NONE =       0;
const int FILEPERM_RD =         0b100;
const int FILEPERM_WR =         0b010;
const int FILEPERM_EX =         0b001;

// helper functions

/**
 * get the address in memory of the specified block index
 * @param fat filesystem
 * @param block_idx block index
 * @return the address/index in memory of `block_idx`
*/
int mem_idx(uint16_t* fat, int block_idx) {
    uint16_t metadata = fat[0];
    int fat_blocks = FAT_BLOCKS(metadata);
    int block_size = BLOCK_SIZE(metadata);
    return (block_size*fat_blocks + block_size*(block_idx-1));
}

/**
 * search for an open block (where value is `0`)
 * @param fat filesystem
 * @return the block index on success, `0` on failure
*/
int get_free_block(uint16_t* fat) {
    uint16_t metadata = fat[0];
    int n_blocks = FAT_BLOCKS(metadata);
    int block_size = BLOCK_SIZE(metadata);

    int size = n_blocks * block_size;
    for (int i = 1; i < size; i++) {
        if (fat[i] == 0) return i;
    }
    return 0;
}

/**
 * traverse down the fat chain, marking them all as deleted
 * @param fat filesystem
 * @param head the first index of the chain
 * @return none
*/
void delete_chain(uint16_t* fat, int head) {
    uint16_t metadata = fat[0];
    int n_blocks = FAT_BLOCKS(metadata);
    int block_size = BLOCK_SIZE(metadata);

    int curr = head;
    while (curr != LASTBLOCK) {
        // fprintf(stderr, "marking as free: %d\n", curr); // DEBUG: show cleared block
        int next = fat[curr];
        fat[curr] = 0;
        curr = next;
        safe_msync(fat, n_blocks * block_size, MS_SYNC);
    }
}

/**
 * allocates data as a FAT chain
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param curr_block first index of the chain to build
 * @param data what to copy to memory
 * @param n_bytes length of `data`
 * @return none
*/
void build_chain(uint16_t* fat, int fs_fd, int curr_block, char* data, int n_bytes) {
    if (n_bytes == 0) return;
    
    uint16_t metadata = fat[0];
    int n_blocks = FAT_BLOCKS(metadata);
    int block_size = BLOCK_SIZE(metadata);

    fat[curr_block] = LASTBLOCK;
    safe_msync(fat, n_blocks * block_size, MS_SYNC);
    if (n_bytes <= block_size) { // data fits in a single block
        // fprintf(stderr, "final block: %d %d\n", curr, mem_idx(fat, curr)); // DEBUG: show current block in data region & overall mem idx
        safe_lseek(fs_fd, mem_idx(fat, curr_block), SEEK_SET);
        safe_write(fs_fd, data, n_bytes);
    } else { // need multiple blocks
        int next = get_free_block(fat);
        build_chain(fat, fs_fd, next, &data[block_size], n_bytes - block_size);

        fat[curr_block] = next;
        safe_msync(fat, n_blocks * block_size, MS_SYNC);

        // fprintf(stderr, "block: %d\n", curr_block); // debug: show first chain block
        safe_lseek(fs_fd, mem_idx(fat, curr_block), SEEK_SET);
        safe_write(fs_fd, data, block_size);
    }
}

/**
 * fill a FAT chain with data
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param head the first index of the chain
 * @param buffer what to read from
 * @param buffer_size length of `buffer`
 * @return the number of bytes written
*/
int fill_chain(uint16_t* fat, int fs_fd, int head, int chain_size, char* buffer, int buffer_size) {
    if (head == LASTBLOCK) {
        fprintf(stderr, "fill_chain: invalid head block\n");
        exit(EXIT_FAILURE);
    }
    if (buffer_size == 0) return 0;
    
    uint16_t metadata = fat[0];
    int block_size = BLOCK_SIZE(metadata);

    if (chain_size <= block_size) { // data fits in a single block
        if (chain_size == block_size) {
            return 0;
        } else if (block_size - chain_size <= buffer_size) { // buffer has more bytes than remaining space
            int bytes_written = block_size - chain_size;
            safe_lseek(fs_fd, mem_idx(fat, head) + chain_size, SEEK_SET);
            safe_write(fs_fd, buffer, bytes_written);
            return bytes_written;
        } else { // buffer has less bytes so write them all
            safe_lseek(fs_fd, mem_idx(fat, head) + chain_size, SEEK_SET);
            safe_write(fs_fd, buffer, buffer_size);
            return buffer_size;
        }
    } else { // need multiple blocks
        return fill_chain(fat, fs_fd, fat[head], chain_size - block_size, buffer, buffer_size);
    }
}

/**
 * add a new empty file to the directory, allocating new blocks as necessary
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param dirhead the first index of the directory chain (`ROOTDIR` or `1` for the root dir)
 * @param filename the file to add
 * @return none
*/
void add_file(uint16_t* fat, int fs_fd, int dir_head, const char* filename) {
    uint16_t metadata = fat[0];
    int n_blocks = FAT_BLOCKS(metadata);
    int block_size = BLOCK_SIZE(metadata);

    int curr_block = dir_head;
    while (true) {
        safe_lseek(fs_fd, mem_idx(fat, curr_block), SEEK_SET);
        dir_entry_t entry;
        for (int i = 0; i < block_size / DIR_ENTRY_SIZE; i++) { // i = entry idx
            safe_read(fs_fd, &entry, DIR_ENTRY_SIZE);
            if (entry.name[0] == FILENAME_ENDDIR || entry.name[0] == FILENAME_DEL_UNUSED) { 
                strcpy(entry.name, filename);
                entry.mtime = time(0);
                entry.size = 0;
                entry.type = FILETYPE_FILE;
                entry.perm = (FILEPERM_RD | FILEPERM_WR);
                entry.firstBlock = -1;
                safe_lseek(fs_fd, mem_idx(fat, curr_block) + i * DIR_ENTRY_SIZE, SEEK_SET);
                safe_write(fs_fd, &entry, DIR_ENTRY_SIZE);
                return;
            }
        } 
        if (fat[curr_block] == LASTBLOCK) break;
        else curr_block = fat[curr_block];
    }

    // not enough space in the current chain so allocate a new link
    // fprintf(stderr, "out of space!\nlast: %d\n", curr_block); // DEBUG: show previous last dir block
    int new_block = get_free_block(fat);
    // fprintf(stderr, "new: %d\n", new_block); // DEBUG: show newly allocated dir block

    fat[curr_block] = new_block;
    safe_msync(fat, n_blocks * block_size, MS_SYNC);

    fat[new_block] = LASTBLOCK;
    safe_msync(fat, n_blocks * block_size, MS_SYNC);
    
    // zero out the new block
    char* zeros = malloc(block_size);
    for (int i = 0; i < block_size; i++) {
        zeros[i] = '\0';
    }
    safe_lseek(fs_fd, mem_idx(fat, new_block), SEEK_SET);
    safe_write(fs_fd, zeros, block_size);
    free(zeros);

    dir_entry_t entry;
    strcpy(entry.name, filename);
    entry.mtime = time(0);
    entry.size = 0;
    entry.type = FILETYPE_FILE;
    entry.perm = (FILEPERM_RD | FILEPERM_WR);
    entry.firstBlock = -1;

    // write the new directory entry
    safe_lseek(fs_fd, mem_idx(fat, new_block), SEEK_SET);
    safe_write(fs_fd, &entry, DIR_ENTRY_SIZE);
}

/**
 * seek and write a file block to memory
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param location point in memory
 * @param entry the file block
 * @return none
*/
void write_file(uint16_t* fat, int fs_fd, point_t location, dir_entry_t entry) {
    int block = location.first;
    int index = location.second;
    safe_lseek(fs_fd, mem_idx(fat, block) + index * DIR_ENTRY_SIZE, SEEK_SET);
    safe_write(fs_fd, &entry, DIR_ENTRY_SIZE);
}

/**
 * Reads a FAT chain from the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param head Index of the first block in the chain.
 * @param buffer Buffer to store the read data.
 * @param chain_bytes Total number of bytes to read from the chain.
 * @return None.
 */
void read_chain(uint16_t* fat, int fs_fd, int head, char* buffer, int chain_bytes) {
    if (head == LASTBLOCK) return;
    if (chain_bytes == 0) return;
    
    uint16_t metadata = fat[0];
    int block_size = BLOCK_SIZE(metadata);

    if (chain_bytes <= block_size) {
        safe_lseek(fs_fd, mem_idx(fat, head), SEEK_SET);
        safe_read(fs_fd, buffer, chain_bytes);
    } else {
        safe_lseek(fs_fd, mem_idx(fat, head), SEEK_SET);
        safe_read(fs_fd, buffer, block_size);
        read_chain(fat, fs_fd, fat[head], &buffer[block_size], chain_bytes - block_size);
    }
}

/**
 * Finds a file or directory in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param dir_head Index of the first block in the directory.
 * @param filename Name of the file or directory to find.
 * @param loc Pointer to a point_t structure to store the location (block index and entry index).
 * @param ret Pointer to a dir_entry_t structure to store the found directory entry.
 * @return Returns true if the file or directory is found; otherwise, false.
 */
bool find_file(uint16_t* fat, int fs_fd, int dir_head, const char* filename, point_t* loc, dir_entry_t* ret) {
    uint16_t metadata = fat[0];
    int block_size = BLOCK_SIZE(metadata);

    int curr_block = dir_head;
    while (curr_block != LASTBLOCK) {
        safe_lseek(fs_fd, mem_idx(fat, curr_block), SEEK_SET);
        dir_entry_t entry;
        for (int i = 0; i < block_size / DIR_ENTRY_SIZE; i++) {
            safe_read(fs_fd, &entry, DIR_ENTRY_SIZE);
            if (entry.name[0] < FILENAME_DEL_INUSE) continue; // end of dir, or deleted entry & file
            if (strcmp(filename, entry.name) == 0) {
                if (loc == NULL || ret == NULL) return true;
                loc->first = curr_block;
                loc->second = i;
                *ret = entry;
                return true;
            }
        } 
        curr_block = fat[curr_block];
    }
    return false;
}

/**
 * Checks if a string is a valid filename.
 * @param str The string to be checked.
 * @return Returns true if the string is a valid filename; otherwise, false.
 */
bool valid_filename(char* str) {
    if (strlen(str) >= 32) {
        fprintf(stderr, "invalid filename, 31 characters limit\n");
        return false;
    }
    for (int i = 0; i < strlen(str); i++) {
        char c = str[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-')) {
            fprintf(stderr, "filename must consist of [A-Za-z0-9._-], invalid character '%c'\n", c);
            return false;
        }
    }
    return true;
}

/**
 * Retrieves metadata information from the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param n_blocks Pointer to an integer to store the number of blocks in the filesystem.
 * @param block_size Pointer to an integer to store the block size in bytes.
 * @return None.
 */
void fs_getmeta(uint16_t* fat, int fs_fd, int* n_blocks, int* block_size) {
    uint16_t metadata;
    safe_lseek(fs_fd, 0, SEEK_SET);
    safe_read(fs_fd, &metadata, 2);
    *n_blocks = FAT_BLOCKS(metadata);
    *block_size = BLOCK_SIZE(metadata);
}

/**
 * Mounts a file system.
 * @param fs_name The name of the file system to mount.
 * @param fat Pointer to FAT.
 * @return Returns the file descriptor of the mounted file system on success. On failure, returns -1.
 */
int fs_mount(char* fs_name, uint16_t** fat) {
    int fs_fd = safe_open(fs_name, O_RDWR, DEFAULT_PERMISSIONS); // permissions ignored because no O_CREAT
    int n_blocks;
    int block_size;
    fs_getmeta(*fat, fs_fd, &n_blocks, &block_size);

    *fat = safe_mmap(NULL, n_blocks * block_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    return fs_fd;
}

/**
 * Unmounts a file system.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @return None.
 */
void fs_unmount(uint16_t** fat, int fs_fd) {
    int n_blocks;
    int block_size;
    fs_getmeta(*fat, fs_fd, &n_blocks, &block_size);

    safe_munmap(*fat, n_blocks * block_size);
    safe_close(fs_fd);
}

/**
 * Creates or updates a file in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param target Name of the file to create or update.
 * @return Returns true if a new file is created; otherwise, false if an existing file is updated.
 */
bool fs_touch(uint16_t* fat, int fs_fd, const char* target) {
    point_t location;
    dir_entry_t entry;
    bool found = find_file(fat, fs_fd, ROOTDIR, target, &location, &entry);
    if (found) { // update timestamp
        entry.mtime = time(0);
        write_file(fat, fs_fd, location, entry);
        return false;
    } else { // create file
        add_file(fat, fs_fd, ROOTDIR, target);
        return true;
    }
}

/**
 * Moves or renames a file in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param old_name Name of the file to move or rename.
 * @param new_name New name or path for the file.
 * @return Returns true if the file is successfully moved or renamed; otherwise, false.
 */
bool fs_mv(uint16_t* fat, int fs_fd, const char* old_name, const char* new_name) {
    point_t location;
    dir_entry_t entry;
    bool found = find_file(fat, fs_fd, ROOTDIR, old_name, &location, &entry);
    if (!found) return false;
    if (find_file(fat, fs_fd, ROOTDIR, new_name, NULL, NULL)) return false; // new_name already exists

    strcpy(entry.name, new_name);
    entry.mtime = time(0);

    write_file(fat, fs_fd, location, entry);
    return true;
}

/**
 * Marks a file or directory as deleted in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param target Name of the file or directory to mark as deleted.
 * @return Returns true if the file or directory is successfully marked as deleted; otherwise, false.
 */
bool fs_mark_deleted(uint16_t* fat, int fs_fd, const char* target) {
    point_t location;
    dir_entry_t entry;
    bool found = find_file(fat, fs_fd, ROOTDIR, target, &location, &entry);
    if (!found) return false;

    entry.name[0] = FILENAME_DEL_INUSE;
    entry.mtime = time(0);

    write_file(fat, fs_fd, location, entry);
    return true;
}

/**
 * Removes (deletes) a file or directory from the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param target Name of the file or directory to remove.
 * @return Returns true if the file or directory is successfully removed; otherwise, false.
 */
bool fs_rm(uint16_t* fat, int fs_fd, const char* target) {
    point_t location;
    dir_entry_t entry;
    bool found = find_file(fat, fs_fd, ROOTDIR, target, &location, &entry);
    if (!found) return false;

    entry.name[0] = FILENAME_DEL_UNUSED;
    delete_chain(fat, entry.firstBlock);

    write_file(fat, fs_fd, location, entry);
    return true;
}

/**
 * Concatenates input strings or files and outputs the result to the terminal or a file.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param input_mode Mode for input: 0 for input string, >0 for input files.
 * @param output_mode Mode for output: 0 for stdout, 1 for file overwrite, 2 for file append.
 * @param input_str Input string to concatenate (used when input_mode is 0).
 * @param input_files Array of input file names (used when input_mode is >0).
 * @param output_file Name of the output file (used when output_mode is 1 or 2).
 * @return Returns the concatenated output string if output_mode is 0; otherwise, returns NULL.
 */
char* fs_cat(uint16_t* fat, int fs_fd, int input_mode, int output_mode,
            char* input_str, char* input_files[], char* output_file) {
    int output_size; // bytes of output, excluding null terminator
    char* output; // output to terminal or file
    if (input_mode == 0) { // read & copy input_str to output
        output_size = strlen(input_str);
        output = malloc(output_size+1); // +1 for null terminator
        strcpy(output, input_str);
    } else { // input from files
        output_size = 0;
        for (int f = 0; f < input_mode; f++) {
            char* target = input_files[f];
            point_t location;
            dir_entry_t entry;
            find_file(fat, fs_fd, ROOTDIR, target, &location, &entry);
            output_size += entry.size;
        }

        output = malloc(output_size+1); // +1 for null terminator

        int position = 0;
        for (int f = 0; f < input_mode; f++) {
            char* target = input_files[f];
            point_t location;
            dir_entry_t entry;
            find_file(fat, fs_fd, ROOTDIR, target, &location, &entry);

            read_chain(fat, fs_fd, entry.firstBlock, &output[position], entry.size);
            position += entry.size;
        }
    }

    output[output_size] = '\0'; // add correct null terminator
    if (output_mode == 0) { // output to stdout
        return output;
        // fprintf(stderr, "%s\n", output);
    } else if (output_mode == 1) { // output to file, overwrite
        point_t location;
        dir_entry_t entry;
        bool found = find_file(fat, fs_fd, ROOTDIR, output_file, &location, &entry);

        if (!found) { // create file
            add_file(fat, fs_fd, ROOTDIR, output_file);
            find_file(fat, fs_fd, ROOTDIR, output_file, &location, &entry);
        }
        delete_chain(fat, entry.firstBlock); // overwrite contents
        int new_head = get_free_block(fat);
        build_chain(fat, fs_fd, new_head, output, output_size+1); // +1 to include null terminator
        entry.firstBlock = new_head;
        entry.mtime = time(0);
        entry.size = output_size;

        write_file(fat, fs_fd, location, entry);
    } else { // output to file, append mode
        point_t location;
        dir_entry_t entry;
        bool found = find_file(fat, fs_fd, ROOTDIR, output_file, &location, &entry);
        
        if (!found) { // create file
            add_file(fat, fs_fd, ROOTDIR, output_file);
            find_file(fat, fs_fd, ROOTDIR, output_file, &location, &entry);
        }
        if (entry.firstBlock == LASTBLOCK) {
            int new_head = get_free_block(fat);
            build_chain(fat, fs_fd, new_head, output, output_size);
            entry.firstBlock = new_head;
            entry.mtime = time(0);
            entry.size = output_size;

            write_file(fat, fs_fd, location, entry);
        } else { // build new chains
            int buffer_offset = fill_chain(fat, fs_fd, entry.firstBlock, entry.size, output, output_size);
            if (buffer_offset == output_size) {
                entry.mtime = time(0);
                entry.size = entry.size + output_size;

                write_file(fat, fs_fd, location, entry);
            } else {
                char* shifted = &output[buffer_offset];
                int new_head = get_free_block(fat);
                build_chain(fat, fs_fd, new_head, shifted, output_size - buffer_offset);
                
                // link the chains
                int last = entry.firstBlock;
                while (fat[last] != LASTBLOCK) {
                    last = fat[last];
                }
                fat[last] = new_head;
                int n_blocks;
                int block_size;
                fs_getmeta(fat, fs_fd, &n_blocks, &block_size);
                safe_msync(fat, n_blocks * block_size, MS_SYNC);

                entry.mtime = time(0);
                entry.size = entry.size + output_size;

                write_file(fat, fs_fd, location, entry);
            }
        }
    }
    free(output);
    return NULL;
}

/**
 * Copies a file or directory to a destination in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param source Name of the source file or directory to copy.
 * @param dest Name of the destination file or directory.
 * @return Returns true if the copy is successful; otherwise, false.
 */
bool fs_cp(uint16_t* fat, int fs_fd, const char* source, const char* dest) {
    return fs_cp_mode(fat, fs_fd, source, dest, false, false);
}

/**
 * Copies a file or directory with specified input and output modes in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param source Name of the source file or directory to copy.
 * @param dest Name of the destination file or directory.
 * @param host_in Input mode: true for hostOS to PennFAT, false for PennFAT to PennFAT.
 * @param host_out Output mode: true for PennFAT to hostOS, false for PennFAT to PennFAT.
 * @return Returns true if the copy is successful; otherwise, false.
 */
bool fs_cp_mode(uint16_t* fat, int fs_fd, const char* source, const char* dest, bool host_in, bool host_out) {
    int source_size; // size of source file
    char* buffer; // contents to copy
    int bytes_read; // size of buffer
    if (host_in) { // hostOS ->
        // open input file
        int source_fd = safe_open(source, O_RDONLY, DEFAULT_PERMISSIONS); // permissions ignored because no O_CREAT
        // read input file
        source_size = (int) safe_lseek(source_fd, 0, SEEK_END);
        buffer = malloc(source_size);
        safe_lseek(source_fd, 0, SEEK_SET); // read from start
        bytes_read = (int) safe_read(source_fd, buffer, source_size);
    } else { // PennFAT ->
        // open input file
        point_t source_loc;
        dir_entry_t source_ent;
        if (!find_file(fat, fs_fd, ROOTDIR, source, &source_loc, &source_ent)) return false;
        // read input file
        source_size = source_ent.size;
        buffer = malloc(source_size);
        read_chain(fat, fs_fd, source_ent.firstBlock, buffer, source_ent.size);
        bytes_read = source_ent.size;
    }

    if (host_out) { // -> hostOS
        // open output file
        int dest_fd = safe_open(dest, O_WRONLY|O_TRUNC|O_CREAT, DEFAULT_PERMISSIONS);
        // write to output file
        safe_write(dest_fd, buffer, bytes_read);
        safe_close(dest_fd);
    } else { // -> PennFat
        // open output file
        point_t dest_loc;
        dir_entry_t dest_ent;
        bool dest_found = find_file(fat, fs_fd, ROOTDIR, dest, &dest_loc, &dest_ent);
        if (!dest_found) { // create file
            add_file(fat, fs_fd, ROOTDIR, dest);
            find_file(fat, fs_fd, ROOTDIR, dest, &dest_loc, &dest_ent);
        }
        // delete old contents of dest and build new chain using buffer
        delete_chain(fat, dest_ent.firstBlock);
        int new_head = get_free_block(fat);
        build_chain(fat, fs_fd, new_head, buffer, bytes_read);
        dest_ent.firstBlock = new_head;
        dest_ent.mtime = time(0);
        dest_ent.size = bytes_read;
        // write to output file
        write_file(fat, fs_fd, dest_loc, dest_ent);
    }
    free(buffer);
    return true;
}

/**
 * Displays information about a single directory entry.
 * @param entry Pointer to the directory entry.
 * @return None.
 */
void fs_ls_single(dir_entry_t* entry) {
    if (entry->name[0] <= FILENAME_DEL_INUSE) return; // not a valid file

    char* time_str = ctime(&entry->mtime);
    time_str[strlen(time_str)-1] = '\0'; // delete '\n'
    char* rwx_perm = "---";
    switch (entry->perm) {
        case 0b000: rwx_perm = "---"; break;
        case 0b010: rwx_perm = "-w-"; break;
        case 0b100: rwx_perm = "r--"; break;
        case 0b101: rwx_perm = "r-x"; break;
        case 0b110: rwx_perm = "rw-"; break;
        case 0b111: rwx_perm = "rwx"; break;
        default:
            fprintf(stderr, "invalid permissions: %d", entry->perm);
            exit(EXIT_FAILURE);
    }

    fprintf(stderr, "%5d %s %d %s %s\n", 
            entry->firstBlock,
            rwx_perm,
            entry->size,
            time_str,
            entry->name);
}

/**
 * Displays information about all directory entries in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @return None.
 */
void fs_ls(uint16_t* fat, int fs_fd) {
    int curr_block = ROOTDIR;
    int n_blocks;
    int block_size;
    fs_getmeta(fat, fs_fd, &n_blocks, &block_size);
    while (curr_block != LASTBLOCK) {
        safe_lseek(fs_fd, mem_idx(fat, curr_block), SEEK_SET);
        dir_entry_t entry;
        for (int i = 0; i < block_size / DIR_ENTRY_SIZE; i++) {
            safe_read(fs_fd, &entry, DIR_ENTRY_SIZE);
            fs_ls_single(&entry);
        } 
        curr_block = fat[curr_block];
    }
}

/**
 * Changes the permissions of a file or directory in the filesystem.
 * @param fat Pointer to FAT.
 * @param fs_fd File descriptor of the filesystem.
 * @param target Name of the file or directory to change permissions.
 * @param permissions New permissions to set.
 * @return Returns the old permissions before the change.
 */
uint8_t fs_chmod(uint16_t* fat, int fs_fd, const char* target, uint8_t permissions) {
    point_t location;
    dir_entry_t entry;
    find_file(fat, fs_fd, ROOTDIR, target, &location, &entry);
    uint8_t old_perm = entry.perm;
    entry.perm = permissions; // update permissions
    entry.mtime = time(0); // update timestamp

    write_file(fat, fs_fd, location, entry);
    return old_perm;
}