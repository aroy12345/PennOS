
#pragma once

#include <stdint.h>

#include "../kernel/PCB.h"

// filesystem user-level calls interface

typedef struct fileptr fileptr_t;
typedef struct fileptr {
    int pid;
    int ptr;
    fileptr_t* next;
} fileptr_t;

struct file;
typedef struct file { // file information
    char filename[32];
    int file_id; // global file id
    int wr_pid; // -1 if no file is writing, else pid of the only file with write access
    struct fileptr* fileptr_head; // linkedlist of fileptrs
    struct file* next; // next in list
} file_t;

#define F_STDIN     0
#define F_STDOUT    1
#define F_STDERR    2

#define F_WRITE     0
#define F_READ      1
#define F_APPEND    2
/**
 * open a file name fname with the mode `mode` and return a file descriptor
 * @param fname the filename to open
 * @param mode Either `F_WRITE`, `F_READ`, or `F_APPEND`.
 * If `F_WRITE`: reading and writing, truncate if the file exists and create it otherwise;
 * only 1 file instance of `F_WRITE` mode can exist.
 * If `F_READ`: read only.
 * If `F_APPEND`: reading and writing, do not truncate if the file exists; file pointer is set to end of file.
 * @return a file descriptor on success, `-1` otherwise
*/
int f_open(const char *fname, int mode);

/**
 * read from a file
 * @param fd the file descriptor to read from
 * @param n number of bytes to read
 * @param buf buffer to read into
 * @return number of bytes read (including `\0`) on success, `0` if EOF is reached, `-1` on error
*/
int f_read(int fd, int n, char *buf);

/**
 * write to a file & increment file pointer
 * @param fd the file descriptor to write to
 * @param str the string to write from
 * @param n number of bytes to write
 * @return number of bytes written (including `\0`) on success, `-1` on error
*/
int f_write(int fd, const char *str, int n);

/**
 * close a file descriptor
 * @param fd the file descriptor to close
 * @return `0` on success, `-1` on error
*/
int f_close(int fd);

/**
 * remove a file, if it exists & is not in use
 * @param fname the file to remove
 * @return `0` on success, `-1` otherwise
*/
int f_unlink(const char *fname);

#define F_SEEK_SET  0
#define F_SEEK_CURR 1
#define F_SEEK_END  2
/**
 * reposition the file pointer
 * @param fd the file
 * @param offset the offset
 * @param whence Either `F_SEEK_SET`, `F_SEEK_CURR`, or `F_SEEK_END`.
 * If `F_SEEK_SET`: file pointer is set to `offset`.
 * If `F_SEEK_CURR`: file pointer is set to `offset` + the current file pointer position.
 * If `F_SEEK_END`: file pointer is set to `offset` + file size.
 * @return the new file pointer position, or `-1` on error
*/
int f_lseek(int fd, int offset, int whence);

/**
 * list a file in the current directory
 * @param filename the file to list, or `NULL` to list all files in the current directory
 * @return none
*/
void f_ls(const char *filename);

/**
 * list a file in the current directory
 * @param filenames files to touch
 * @param n number of files to touch
 * @return none
*/
void f_touch(char* filenames[], int n);

/**
 * print to terminal (F_STDERR)
 * @param str the string to print (use `snprintf` to format)
 * @return number of bytes written (including `\0`) on success, `-1` on error
*/
int f_print(const char* str);

// mount
int f_mount(char* fs_name, uint16_t** fat);

// unmount
void f_unmount(uint16_t** fat, int fs_fd);

// rename src to dest
void f_mv(char* src, char* dest); 

// copy src to dest
void f_cp(char* src, char* dest);

// remove filenames
void f_rm(char* filenames[], int n);

#define FPERM_READ  0b100
#define FPERM_WRIT  0b010
#define FPERM_EXEC  0b001
// change permissions
void f_chmod(char* filename, int perms);

void print_fileptr_pids_all();


// TODO: these should go in a kernel-level filesystem header
void process_create_fileptrs(PCB* pcb);
void process_delete_fileptrs(PCB* pcb);
file_t* find_file_entry_by_file_id(int file_id);