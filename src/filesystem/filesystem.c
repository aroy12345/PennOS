// filesystem user-level calls

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "filesystem.h"
#include "../util/globals.h"
#include "../util/util.h"
#include "../util/p-errno.h"
#include "../kernel/PCB.h"
#include "../pennfat/fat.h"
#include "../pennfat/safe.h"

int next_file_id = 0; // next available file_id for a newly opened file
file_t* open_files = NULL; // global list of files currently open by any process
extern PCB* current_pcb; // updated globally

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define BETWEEN_INCL(value, lower, upper) ((value) >= (lower) && (value) <= (upper))

/**
 * create & insert a file pointer into the fileptr list
 * @param fileptr_head the head of the fileptr list
 * @param pid the process id for the fileptr
 * @param ptr the pointer/offset
 * @return none
*/
void create_fileptr(fileptr_t** fileptr_head, int pid, int ptr) {
    fileptr_t* new_fileptr = malloc(sizeof(fileptr_t));
    new_fileptr->pid = pid;
    new_fileptr->ptr = ptr;
    if (*fileptr_head == NULL) {
        new_fileptr->next = NULL;
    } else {
        new_fileptr->next = *fileptr_head;
    }
    *fileptr_head = new_fileptr;
}

/**
 * delete the specified file pointer from the fileptr list
 * @param fileptr_head the head of the fileptr list
 * @param pid the process id whose fileptr should be deleted
 * @return none
*/
void delete_fileptr(fileptr_t** fileptr_head, int pid) {
    fileptr_t* curr = *fileptr_head;
    fileptr_t* prev = NULL;
    while (curr != NULL && curr->pid != pid) { // set curr to the fileptr
        prev = curr;
        curr = curr->next;
    }
    if (curr != NULL) { // fileptr found
        if (prev == NULL) *fileptr_head = curr->next; // first file_entry was deleted
        else prev->next = curr->next; // trailing file_entry was deleted
        free(curr);
    } else { // fileptr not found
        // fprintf(stderr, "fileptr not found (pid:[%d])\n", pid);
    }
}

/**
 * get a file pointer struct for a process
 * @param fileptr_head the head of the fileptr list
 * @param pid the process id to find a fileptr for
 * @return the file pointer struct, or `NULL` if `pid` was not found
*/
fileptr_t* get_fileptr(fileptr_t* fileptr_head, int pid) {
    fileptr_t* curr = fileptr_head;
    // printf("Looking for: %d\n", pid);
    // printf("curr: %ld\n", (long)fileptr_head);
    while (curr != NULL) {
        // printf("PID: %d\n", curr->pid);
        if (pid == curr->pid) return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * get a file pointer for a process
 * @param fileptr_head the head of the fileptr list
 * @param pid the process id to find a fileptr for
 * @return the file pointer, or `-1` if `pid` was not found
*/
int get_fileptr_ptr(fileptr_t* fileptr_head, int pid) {
    fileptr_t* fileptr = get_fileptr(fileptr_head, pid);
    if (!fileptr) return -1;
    return fileptr->ptr;
}



/**
 * create & insert a file entry into `open_files`
 * @param filename the file name
 * @param mode the open mode
 * @param dir_entry the file's fat directory entry
 * @return the file_id
*/
int create_file_entry(const char* filename, int mode, dir_entry_t* dir_entry) {
    file_t* new_file_entry = malloc(sizeof(file_t));
    strcpy(new_file_entry->filename, filename);
    new_file_entry->file_id = next_file_id++;
    new_file_entry->fileptr_head = NULL;
    switch (mode) {
        case F_WRITE:
            create_fileptr(&new_file_entry->fileptr_head, current_pcb->pid, 0);
            new_file_entry->wr_pid = current_pcb->pid; break;
        case F_READ:
            create_fileptr(&new_file_entry->fileptr_head, current_pcb->pid, 0);
            new_file_entry->wr_pid = -1; break;
        case F_APPEND:
            create_fileptr(&new_file_entry->fileptr_head, current_pcb->pid, dir_entry->size);
            new_file_entry->wr_pid = current_pcb->pid; break;
    }
    new_file_entry->next = open_files;
    open_files = new_file_entry;

    return new_file_entry->file_id;
}

/**
 * delete the specified file entry from `open_files`;
 * call when `file_entry.fileptr_head == NULL`
 * @param file_id the file id
 * @return none
*/
void delete_file_entry(int file_id) {
    file_t* curr = open_files;
    file_t* prev = NULL;
    while (curr != NULL && curr->file_id != file_id) { // set curr to the file
        prev = curr;
        curr = curr->next;
    }
    if (curr != NULL) { // file found
        if (prev == NULL) open_files = curr->next; // first file_entry was deleted
        else prev->next = curr->next; // trailing file_entry was deleted
        free(curr);
    } else { // file not found
        // fprintf(stderr, "file not found (file_id:[%d])\n", file_id);
    }
}

#define F_HASPERM(perm, mask) (((perm) & (mask)) != 0) // check if `perm` matches a permission `mask` bitwise
#define F_CANREAD(perm) (F_HASPERM(perm, FILEPERM_RD)) // check if read access is allowed by `perm`
#define F_CANWRITE(perm) ((F_HASPERM(perm, FILEPERM_WR)) && (F_CANREAD(perm))) // check if read & write access is allowed by `perm`
/**
 * check if permissions requested match a file's allowed permissions
 * @param perm the allowed permissions
 * @param mode the permissions requested
 * @return `true` if `perm` allows a file to be opened with `mode`, `false` otherwise
*/
bool valid_perm(uint8_t perm, int mode) {
    return (
        (mode == F_WRITE && F_CANWRITE(perm)) ||
        (mode == F_READ && F_CANREAD(perm)) ||
        (mode == F_APPEND && F_CANWRITE(perm))
    );
}

file_t* find_file_entry_by_file_id(int file_id) {
    file_t* curr = open_files;
    while (curr != NULL) {
        if (curr->file_id == file_id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

file_t* find_file_entry_by_filename(const char* filename) {
    file_t* curr = open_files;
    while (curr != NULL) {
        if (strcmp(curr->filename, filename) == 0) return curr;
        curr = curr->next;
    }
    return false;
}

/**
 * find a file by `fd`
 * @param file_id_ptr set to the file id, if the file is found
 * @param file_entry set to the file entry, if the file is found
 * @return `true` if the file is found, `false` & print otherwise
*/
bool find_file_entry(int fd, int* file_id_ptr, file_t* file_entry) {
    *file_id_ptr = current_pcb->fileDescriptors[fd];
    *file_entry = *find_file_entry_by_file_id(*file_id_ptr);
    if (file_entry == NULL) {
        // fprintf(stderr, "file does not exist (fd:[%d] file_id:[%d])\n", fd, *file_id_ptr);
        ERRNO = ERR_FS_FILE_NOT_FOUND;
        return false;
    }
    return true;
}

/**
 * create file pointers for each unique file_id (called by `p_spawn`)
 * @param pcb the process PCB
 * @return none
*/
void process_create_fileptrs(PCB* pcb) {
    if (next_file_id == 0) return;
    int unique_file_ptrs[next_file_id];
    for (int i = 0; i < next_file_id; i++) { // initialize to -1
        unique_file_ptrs[i] = -1;
    }
    for (int fd = 3; fd < MAX_FDS; fd++) { // mark used file_id
        int file_id = pcb->fileDescriptors[fd];
        if (file_id >= 0) {
            file_t* file_entry = find_file_entry_by_file_id(file_id);
            if (file_entry == NULL) continue; // not open
            int fileptr_ptr = get_fileptr_ptr(file_entry->fileptr_head, pcb->parent_pid);
            unique_file_ptrs[file_id] = fileptr_ptr;
        }
    }
    for (int fid = 0; fid < next_file_id; fid++) { // add fileptr for each unique fd
        if (unique_file_ptrs[fid] == -1) continue;
        file_t* file_entry = find_file_entry_by_file_id(fid);
        create_fileptr(&file_entry->fileptr_head, pcb->pid, unique_file_ptrs[fid]);
    }
}

/**
 * delete file pointers of each unique file_id (called by `p_kill`)
 * @param pcb the process PCB
 * @return none
*/
void process_delete_fileptrs(PCB* pcb) {
    if (next_file_id == 0) return;
    bool unique_file_ids[next_file_id];
    for (int i = 0; i < next_file_id; i++) { // initialize to false
        unique_file_ids[i] = false;
    }
    for (int fd = 3; fd < MAX_FDS; fd++) { // mark used file_id
        int file_id = pcb->fileDescriptors[fd];
        if (file_id >= 0 && file_id < next_file_id) {
            unique_file_ids[file_id] = true;
        }
    }
    for (int fid = 0; fid < next_file_id; fid++) { // delete fileptr for each unique fd
        if (!unique_file_ids[fid]) continue;
        file_t* file_entry = find_file_entry_by_file_id(fid);
        delete_fileptr(&file_entry->fileptr_head, pcb->pid);
        if (file_entry->wr_pid == pcb->pid) file_entry->wr_pid = -1; // current process is no longer writing
    }
}

/**
 * DEBUG: print all file pointers & their pids
 * @return none
*/
void print_fileptr_pids_all() {
    file_t* curr = open_files;
    while (curr != NULL) {
        fprintf(stderr, "file:[%s] id:[%d] - wr:[%d]", curr->filename, curr->file_id, curr->wr_pid);
        fileptr_t* curr_ptr = curr->fileptr_head;
        int hard_cap = 0;
        while (curr_ptr != NULL && hard_cap++ < 10) {
            fprintf(stderr, " %d", curr_ptr->pid);
            curr_ptr = curr_ptr->next;
        }
        fprintf(stderr, "\n");
        curr = curr->next;
    }
}

/**
 * get the first unused file descriptor (for `f_open`)
 * @param pcb the calling process
 * @return the first unused fd, or `-1` if the fd table is full
*/
int find_unused_fd(PCB* pcb) {
    for (int i = 3; i < MAX_FDS; i++) {
        if (pcb->fileDescriptors[i] == NOFILE) return i;
    }
    return -1;
}

/**
 * check if a process already has the file open;
 * use this to check whether to create a file pointer
 * @param pcb the process PCB
 * @param file_id the file id (from `file_t.file_id`)
 * @return `true` if `file_id` is already in the file descriptor table of `pcb`,
 * `false` otherwise
*/
bool is_duplicate_fd(PCB* pcb, int file_id) {
    for (int i = 3; i < MAX_FDS; i++) {
        if (pcb->fileDescriptors[i] == file_id) return true;
    }
    return false;
}

/**
 * check whether a fd refers to the terminal
 * @param fd the file descriptor
 * @return `true` if `fd` is either `F_STDIN`, `F_STDOUT`, or `F_STDERR`; `false` otherwise
*/
bool f_isatty(int fd) {
    if (fd == F_STDIN || fd == F_STDOUT || fd == F_STDERR) return true;
    else return false;
}

// user commands

/**
 * @brief Opens or creates a file and returns a file descriptor.
 *
 * @param fname The name of the file to open or create.
 * @param mode The mode in which to open the file (F_READ, F_WRITE, or F_APPEND).
 * @return The file descriptor on success, or -1 on failure with ERRNO set.
 *
 * @note If the file already exists, the function checks permissions and handles
 * multiple processes attempting to open the same file.
 *
 * @note If the file does not exist, it is created, and the open files list is updated.
 */
int f_open(const char *fname, int mode) {
    file_t* file_entry = find_file_entry_by_filename(fname);
    bool inuse = !(file_entry == NULL);
    point_t loc;
    dir_entry_t dir_entry;
    bool found = find_file(fat, fs_fd, ROOTDIR, fname, &loc, &dir_entry);

    if (inuse) { // don't add another entry
        if (!valid_perm(dir_entry.perm, mode)) { // invalid permissions
            ERRNO = ERR_F_OPEN_INVALID_PERMS;
            return -1;
        }
        if (mode == F_WRITE && file_entry->wr_pid != current_pcb->pid && file_entry->wr_pid != -1) { // another process already has write access
            ERRNO = ERR_F_OPEN_WRITE_INUSE;
            // fprintf(stderr, "another process already has write access:[%d]\n", file_entry->wr_pid);
            // fprintf(stderr, "current pid:[%d]\n", current_pcb->pid);
            return -1;
        }
        if (mode != F_WRITE && mode != F_READ && mode != F_APPEND) { // invalid mode
            ERRNO = ERR_F_OPEN_INVALID_MODE;
            return -1;
        }
        
        fs_touch(fat, fs_fd, fname); // touch file
        // update fileptr & wr_pid if necessary
        int new_fileptr = -1;
        switch (mode) {
            case F_WRITE:
                new_fileptr = 0;
                file_entry->wr_pid = current_pcb->pid; break;
            case F_READ:
                new_fileptr = 0; break;
            case F_APPEND:
                new_fileptr = dir_entry.size;
                file_entry->wr_pid = current_pcb->pid; break;
        }
        if (is_duplicate_fd(current_pcb, file_entry->file_id)) { // process already has file open
            fileptr_t* fp_struct = get_fileptr(file_entry->fileptr_head, current_pcb->pid);
            fp_struct->ptr = new_fileptr;
        } else { // process's first fd for this file
            create_fileptr(&file_entry->fileptr_head, current_pcb->pid, new_fileptr);
        }

        int fd = find_unused_fd(current_pcb);
        current_pcb->fileDescriptors[fd] = file_entry->file_id;
        return fd;
    } else { // add another entry
        if (!found && mode == F_READ) { // file doesn't exist in directory & can't create
            ERRNO = ERR_F_OPEN_CREATE_READ;
            return -1;
        }
        
        fs_touch(fat, fs_fd, fname); // touch file, create if file doesn't exist

        // update open files list
        int fd = find_unused_fd(current_pcb);
        current_pcb->fileDescriptors[fd] = create_file_entry(fname, mode, &dir_entry);
        return fd;
    }
}

/**
 * @brief Reads data from a file descriptor.
 *
 * This function reads data from the specified file descriptor and stores it in the provided buffer.
 * If the file descriptor represents STDIN, data is read from the terminal input. If it represents
 * STDOUT or STDERR, an error is returned. For regular file descriptors, the corresponding file entry
 * is located, and data is read from the file into a temporary buffer.
 *
 * @param fd The file descriptor to read from.
 * @param n The number of bytes to read.
 * @param buf The buffer to store the read data.
 * @return On success, the number of bytes read is returned. On failure, -1 is returned, and the
 * global variable ERRNO is set accordingly. If the end of the file is reached (EOF), 0 is returned.
 */
int f_read(int fd, int n, char* buf) {
    if (current_pcb->fileDescriptors[fd] == STDIN_ID) { // input from terminal
        char input_buf[IOBUFFER_SIZE + 1];
        int input_size = safe_read(STDIN_FILENO, input_buf, IOBUFFER_SIZE);
        if (input_size <= 0) return 0; // EOF
        input_buf[input_size] = '\0';

        int bytes_to_read = MIN(n,input_size);
        for (int i = 0; i <= bytes_to_read; i++) { // copy into buf
            buf[i] = input_buf[i];
        }
        buf[bytes_to_read] = '\0'; // add null terminator
        return bytes_to_read;
    } else if (current_pcb->fileDescriptors[fd] == STDOUT_ID ||
               current_pcb->fileDescriptors[fd] == STDERR_ID) {
        ERRNO = ERR_F_READ_TERM_OUT;
        return -1;
    }

    // find file entry in open files, if it exists
    int file_id;
    file_t file_entry;
    if (!find_file_entry(fd, &file_id, &file_entry)) return -1;

    // printf("file_entry.fileptr_head: %ld\n", (long)file_entry.fileptr_head);
    // printf("file_entry.filename: %s\n", file_entry.filename);

    // find directory entry in pennfat
    point_t loc;
    dir_entry_t entry;
    find_file(fat, fs_fd, ROOTDIR, file_entry.filename, &loc, &entry);

    // read entire file into a temp buffer
    char* temp_buf = malloc(entry.size);
    read_chain(fat, fs_fd, entry.firstBlock, temp_buf, entry.size);

    fileptr_t* fp_struct = get_fileptr(file_entry.fileptr_head, current_pcb->pid);
    // printf("fp_struct: %ld\n", (long)fp_struct);

    int bytes_to_read;
    if (fp_struct->ptr + n > entry.size) { // read all remaining bytes
        bytes_to_read = entry.size - fp_struct->ptr;
    } else { // read n bytes
        bytes_to_read = n;
    }
    for (int i = 0; i < bytes_to_read; i++) { // copy into buf
        buf[i] = temp_buf[fp_struct->ptr + i];
    }
    buf[bytes_to_read] = '\0'; // add null terminator
    fp_struct->ptr += bytes_to_read;

    free(temp_buf);
    return bytes_to_read;
}
/**
 * @brief Writes data to a file descriptor.
 *
 * This function writes data to the specified file descriptor based on the given parameters.
 * If the file descriptor represents STDOUT or STDERR, the data is output to the terminal.
 * For regular file descriptors, the function locates the corresponding file entry, checks for
 * write access, and updates the file content accordingly.
 *
 * @param fd The file descriptor to write to.
 * @param str The string containing the data to be written.
 * @param n The number of bytes to write.
 * @return On success, returns the actual number of bytes written. On failure, returns -1, and the
 * global variable ERRNO is set accordingly.
 */
int f_write(int fd, const char *str, int n) {
    if (current_pcb->fileDescriptors[fd] == STDOUT_ID ||
        current_pcb->fileDescriptors[fd] == STDERR_ID) {  // output to terminal
        char output_buf[IOBUFFER_SIZE+1];
        int bytes_to_write = MIN(n, IOBUFFER_SIZE);
        for (int i = 0; i < bytes_to_write; i++) { // copy into output_buf
            output_buf[i] = str[i];
        }
        output_buf[bytes_to_write] = '\0'; // add null terminator
        fprintf(stderr, "%s", output_buf);
        return bytes_to_write;
    } else if (current_pcb->fileDescriptors[fd] == STDIN_ID) {
        ERRNO = ERR_F_WRITE_TERM_IN;
        return -1;
    }

    // find file entry in open files, if it exists
    int file_id;
    file_t file_entry;
    if (!find_file_entry(fd, &file_id, &file_entry)) return -1;
    if (file_entry.wr_pid != current_pcb->pid) { // current process only has read access
        ERRNO = ERR_F_WRITE_RONLY;
        return -1;
    }

    // find directory entry in pennfat
    point_t loc;
    dir_entry_t entry;
    find_file(fat, fs_fd, ROOTDIR, file_entry.filename, &loc, &entry);

    fileptr_t* fp_struct = get_fileptr(file_entry.fileptr_head, current_pcb->pid);
    int bytes_to_write = n;
    int new_file_size;
    if (fp_struct->ptr + bytes_to_write > entry.size) { // need to allocate more space than current file size
        new_file_size = fp_struct->ptr + bytes_to_write;
        bytes_to_write += 1; // space for null terminator
    } else { // file size won't change post-write
        new_file_size = entry.size;
    }

    // read entire file into a temp buffer
    char* temp_buf = malloc(new_file_size+1);
    for (int i = 0; i <= new_file_size; i++) {
        temp_buf[i] = '\0';
    }
    read_chain(fat, fs_fd, entry.firstBlock, temp_buf, entry.size);
    for (int i = 0; i < bytes_to_write; i++) { // write str
        temp_buf[fp_struct->ptr + i] = str[i];
    }
    temp_buf[fp_struct->ptr + bytes_to_write] = '\0';
    // if (new_file_size != entry.size) { // add null terminator if file size was increased
    //     temp_buf[fp_struct->ptr + bytes_to_write] = '\0';
    // }
    fs_cat(fat ,fs_fd, 0, 1, temp_buf, NULL, file_entry.filename); // write to memory

    fp_struct->ptr += bytes_to_write;
    free(temp_buf);

    // printf("bytes alleged to have been written: %d\n", bytes_to_write);
    return bytes_to_write;
}

/**
 * @brief Closes a file descriptor.
 *
 * This function closes the specified file descriptor, releasing associated resources.
 * If the file descriptor represents a terminal, an error is returned. The function then
 * finds the corresponding file entry in open files, updates the process's file descriptor
 * table, and deallocates file pointers if this is the last instance of the file descriptor
 * for the process. If the process had write access, it is revoked when the last instance is closed.
 * If no process is using the file, the file entry is deleted.
 *
 * @param fd The file descriptor to close.
 * @return On success, returns 0. On failure, returns -1, and the global variable ERRNO is set accordingly.
 */
int f_close(int fd) {
    if (f_isatty(fd)) {
        ERRNO = ERR_F_CLOSE_TERMINAL;
        return -1;
    }

    // find file entry in open files, if it exists
    int file_id;
    file_t file_entry;
    if (!find_file_entry(fd, &file_id, &file_entry)) return -1;

    current_pcb->fileDescriptors[fd] = NOFILE; // mark fd as open
    bool last_fd_instance = !is_duplicate_fd(current_pcb, file_id); // process has no more fds for this file
    if (last_fd_instance) delete_fileptr(&file_entry.fileptr_head, current_pcb->pid);
    if (file_entry.wr_pid == current_pcb->pid && last_fd_instance) { // process had write access & no longer has it open
        file_entry.wr_pid = -1;
    }
    if (file_entry.fileptr_head == NULL) delete_file_entry(file_id); // no more process is using the file
    return 0;
}

/**
 * @brief Unlinks (deletes) a file.
 *
 * This function unlinks (deletes) the specified file, releasing associated resources.
 * It first locates the file entry by filename and checks if the current process is accessing
 * the file. If so, it removes the file pointer and updates the write access status. If no more
 * processes are accessing the file, the file entry is deleted, and the file system is updated.
 * If the file is still in use by another process, it is marked as deleted in the file system.
 *
 * @param fname The name of the file to unlink.
 * @return On success, returns 0. On failure, returns -1, and the global variable ERRNO is set accordingly.
 */
int f_unlink(const char *fname) {
    file_t* file_entry = find_file_entry_by_filename(fname);
    if (file_entry == NULL) { // no file entry
        ERRNO = ERR_F_UNLINK_NOT_FOUND;
        return -1;
    }

    if (get_fileptr(file_entry->fileptr_head, current_pcb->pid) != NULL) { // current process is accessing file
        delete_fileptr(&file_entry->fileptr_head, current_pcb->pid);
        if (file_entry->wr_pid == current_pcb->pid) file_entry->wr_pid = -1; // current process is no longer writing
    }
    if (file_entry->fileptr_head == NULL) { // no more processes are accessing the file
        delete_file_entry(file_entry->file_id);
        fs_rm(fat, fs_fd, fname);
    } else { // file is still in use by another process
        fs_mark_deleted(fat, fs_fd, fname);
    }
    return 0;
}

/**
 * @brief Moves the file pointer to a specified position within a file.
 *
 * This function adjusts the file pointer for the specified file descriptor, allowing seeking
 * to a new position within the file. If the file descriptor represents a terminal, an error
 * is returned. The function then locates the corresponding file entry, retrieves the current
 * file pointer position, and calculates the new offset based on the specified 'whence' parameter.
 * If the new offset is within the file bounds, the file pointer is updated, and the new position
 * is returned. Otherwise, an error is returned.
 *
 * @param fd The file descriptor to seek within.
 * @param offset The offset to move the file pointer.
 * @param whence The reference position for the offset (F_SEEK_CURR, F_SEEK_END, or F_SEEK_SET).
 * @return On success, returns the new file pointer position. On failure, returns -1, and the
 * global variable ERRNO is set accordingly.
 */
int f_lseek(int fd, int offset, int whence) {
    if (f_isatty(fd)) {
        ERRNO = ERR_F_LSEEK_TERMINAL;
        return -1;
    }

    // find file entry in open files, if it exists
    int file_id;
    file_t file_entry;
    if (!find_file_entry(fd, &file_id, &file_entry)) return -1;
    point_t loc;
    dir_entry_t dir_entry;
    find_file(fat, fs_fd, ROOTDIR, file_entry.filename, &loc, &dir_entry);

    // find next fileptr position
    fileptr_t* fp_struct = get_fileptr(file_entry.fileptr_head, current_pcb->pid);
    int new_offset = fp_struct->ptr;
    if (whence == F_SEEK_CURR) {
        new_offset += offset;
    } else if (whence == F_SEEK_END) {
        new_offset = dir_entry.size + offset;
    } else if (whence == F_SEEK_SET) {
        new_offset = offset;
    }

    if (!BETWEEN_INCL(new_offset, 0, dir_entry.size)) { // offset puts fileptr out of bounds
        ERRNO = ERR_F_LSEEK_OOB;
        return -1;
    }
    // new fileptr is valid
    fp_struct->ptr = new_offset;
    return fp_struct->ptr;
}

/**
 * @brief Lists information about files in the file system.
 *
 * If the given filename is NULL, this function lists information about all files in the file system.
 * Otherwise, it lists information about the specified file. It locates the file entry, and if found,
 * displays information about the file using the fs_ls_single function. If the file is not found,
 * ERR_FS_FILE_NOT_FOUND is set in ERRNO.
 *
 * @param filename The name of the file to list information about. If NULL, lists information about all files.
 */
void f_ls(const char *filename) {
    if (filename == NULL) fs_ls(fat, fs_fd); // list all
    else { // list current
        point_t loc;
        dir_entry_t entry;
        if (!find_file(fat, fs_fd, ROOTDIR, filename, &loc, &entry)) {
            ERRNO = ERR_FS_FILE_NOT_FOUND;
            return;
        }
        fs_ls_single(&entry);
    }
}

/**
 * Creates empty files with the specified names.
 * @param filenames An array of strings containing the names of the files to be created.
 * @param n The number of filenames in the array.
 */
void f_touch(char* filenames[], int n) {
    for (int i = 0; i < n; i++) {
        fs_touch(fat, fs_fd, filenames[i]);
    }
}

/** @brief Prints a string to the standard error (stderr).
 *  @param str The string to be printed.
 *  @return On success, returns the number of bytes written. On failure, returns -1.
 */
int f_print(const char* str) {
    return f_write(F_STDERR, str, strlen(str));
}

/** @brief Mounts a file system.
 *  @param fs_name The name of the file system to mount.
 *  @param fat A pointer to the file allocation table (FAT) array.
 *  @return On success, returns 0. On failure, returns -1.
 */
int f_mount(char* fs_name, uint16_t** fat) {
    return fs_mount(fs_name, fat);
}

/** @brief Unmounts a file system.
 *  @param fat A pointer to the file allocation table (FAT) array.
 *  @param fs_fd The file descriptor of the file system to unmount.
 */
void f_unmount(uint16_t** fat, int fs_fd) {
    fs_unmount(fat, fs_fd);
}

/** @brief Moves or renames a file or directory.
 *  @param src The source path of the file or directory.
 *  @param dest The destination path for the file or directory.
 */
void f_mv(char* src, char* dest) {
    fs_mv(fat, fs_fd, src, dest);
}

/** @brief Copies a file or directory.
 *  @param src The source path of the file or directory.
 *  @param dest The destination path for the copied file or directory.
 */
void f_cp(char* src, char* dest) {
    fs_cp(fat, fs_fd, src, dest);
}

/** @brief Removes (deletes) files or directories.
 *  @param filenames An array of strings containing the names of the files or directories to be removed.
 *  @param n The number of filenames in the array.
 */
void f_rm(char* filenames[], int n) {
    for (int i = 0; i < n; i++) {
        fs_rm(fat, fs_fd, filenames[i]);
    }
}

/** @brief Changes the permissions of a file or directory.
 *  @param filename The name of the file or directory.
 *  @param perms The new permissions to set for the file or directory.
 */
void f_chmod(char* filename, int perms) {
    fs_chmod(fat, fs_fd, filename, (uint8_t)perms);
}