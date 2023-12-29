#include <stdbool.h>

#pragma once

// FAT manipulation functions interface

extern const int DIR_ENTRY_SIZE;

extern const int ROOTDIR;
extern const int DEFAULT_PERMISSIONS;

extern const int LASTBLOCK;
extern const int BITS_PER_BYTE;
extern const int BYTE_SIZE;
#define FAT_BLOCKS(x) ((x) >> BITS_PER_BYTE) // get number of blocks in fat, MSB of uint16_t
#define BLOCK_SIZE(x) (BYTE_SIZE << ((x) & 0xFF)) // get block size from metadata, LSB of uint16_t

extern const int FILETYPE_UNKNOWN;
extern const int FILETYPE_FILE;
extern const int FILETYPE_DIRECTORY;
extern const int FILETYPE_LINK;

extern const int FILENAME_ENDDIR;
extern const int FILENAME_DEL_UNUSED;
extern const int FILENAME_DEL_INUSE;

extern const int FILEPERM_NONE;
extern const int FILEPERM_RD;
extern const int FILEPERM_WR;
extern const int FILEPERM_EX;

typedef struct directory_entry { // 64-byte directory entry
    char name[32];
    uint32_t size;
    uint16_t firstBlock;
    uint8_t type;
    uint8_t perm;
    time_t mtime;
    char _BUFFER_[16]; // included so that size of directory_entry is 64 bytes
} dir_entry_t;

typedef struct point { // file location (directory block & entry)
    int first; // block index
    int second; // entry index
} point_t;

/**
 * read a FAT chain
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param head the first index of the chain
 * @param buffer what to read into; assume this has enough space
 * @param chain_bytes length of the chain
 * @return none
*/
void read_chain(uint16_t* fat, int fs_fd, int head, char* buffer, int chain_bytes);

/**
 * search for a filename in the directory
 * use `NULL` for `loc` and `ret` to simply check if the file exists
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param dir_head the first index of the directory chain (`ROOTDIR` or `1` for the root dir)
 * @param filename the file to search for
 * @param loc will be set to the block (`loc.first`) & entry number (`loc.second`) of the file,
 * if the file is found
 * @param ret the directory entry of the file, if the file is found
 * @return `true` if the file is found, `false` otherwise
*/
bool find_file(uint16_t* fat, int fs_fd, int dir_head, const char* filename, point_t* loc, dir_entry_t* ret);

/**
 * check whether a filename is valid
 * @param str filename
 * @return `true` if `str` is POSIX-valid (consists of [A-Za-z0-9._-])
 * and 32 bytes null-terminated (31 characters), `false` & print otherwise
*/
bool valid_filename(char* str);

/**
 * get the metadata of a filesystem
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param n_blocks set to the number of blocks in FAT
 * @param block_size set to the size of a block
 * @return none
*/
void fs_getmeta(uint16_t* fat, int fs_fd, int* n_blocks, int* block_size);

/**
 * mount a filesystem & map fat to memory
 * @param fs_name filesystem filename
 * @param fat set to the filesytem
 * @return the filesystem file descriptor (`fs_fd`)
*/
int fs_mount(char* fs_name, uint16_t** fat);

/**
 * unmount a filesystem
 * @param fat pointer to the filesystem; will be unmapped from memory
 * @param fs_fd filesystem file descriptor; will be closed
 * @return none
*/
void fs_unmount(uint16_t** fat, int fs_fd);

/**
 * touch a file; if it exists, update the timestamp, otherwise create the file
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param target the target filename
 * @return `true` if the file is created, `false` otherwise
*/
bool fs_touch(uint16_t* fat, int fs_fd, const char* target);

/**
 * rename a file
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param old_name the file to rename
 * @param new_name the new name for `old_name`
 * @return `true` if the file is renamed (and found), `false` otherwise
*/
bool fs_mv(uint16_t* fat, int fs_fd, const char* old_name, const char* new_name);

/**
 * delete a file
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param target the file to delete
 * @return `true` if the file was deleted, `false` otherwise
*/
bool fs_rm(uint16_t* fat, int fs_fd, const char* target);

/**
 * mark a file as deleted with `FILENAME_DEL_INUSE`, `-2`;
 * it is still in use by another process, but shouldn't be able to be accessed anew
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param target the file to mark
 * @return `true` if the file was marked as deleted, `false` otherwise
*/
bool fs_mark_deleted(uint16_t* fat, int fs_fd, const char* target);

/**
 * cat a number of files or the `input_str`, output to buffer or another file
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param input_mode `0` for `stdin`, otherwise the number of files in `input_files`
 * @param output_mode `0` for `stdout`, `1` for overwrite, `2` for append
 * @param input_str if `input_mode` is `0`, the input
 * @param input_files if `input_mode` is not `0`, the files to concatenate
 * @param output_file if `output_mode` is not `0`, the file to output to
 * @return if `output_mode` is `0`, the output buffer
*/
char* fs_cat(uint16_t* fat, int fs_fd, int input_mode, int output_mode,
            char* input_str, char* input_files[], char* output_file);

/**
 * copy a file
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param source the file to copy from
 * @param dest the file to copy to; created if necessary, or overwritten if not
 * @return `true` on success, `false` if `source` was not found
*/
bool fs_cp(uint16_t* fat, int fs_fd, const char* source, const char* dest);

/**
 * copy a file (may input/output with host OS)
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param source the file to copy from
 * @param dest the file to copy to; created if necessary, or overwritten if not
 * @param host_in if `true`, input file from host OS
 * @param host_out if `true`, output file to host OS
 * @return `true` on success, `false` if `source` was not found
*/
bool fs_cp_mode(uint16_t* fat, int fs_fd, const char* source, const char* dest, bool host_in, bool host_out);

/**
 * list information for a single file
 * @param entry the directory entry for the file
 * @return none
*/
void fs_ls_single(dir_entry_t* entry);

/**
 * list directory
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @return none
*/
void fs_ls(uint16_t* fat, int fs_fd);

/**
 * change permissions
 * @param fat filesystem
 * @param fs_fd filesystem file descriptor
 * @param target the file to change permissions of
 * @param permissions the new permissions
 * @return the old permissions
*/
uint8_t fs_chmod(uint16_t* fat, int fs_fd, const char* target, uint8_t permissions);