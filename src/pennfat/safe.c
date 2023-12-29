// system call error handling

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "safe.h"

/**
 * Opens a file with error handling.
 * @param pathname The path of the file to open.
 * @param flags Flags specifying the mode of opening.
 * @param mode The permissions to set if the file is created.
 * @return Returns the file descriptor if successful; otherwise, exits with an error message.
 */
int safe_open(const char *pathname, int flags, mode_t mode)
{
    int open_fd = open(pathname, flags, mode);
    if (open_fd == -1)
    {
        fprintf(stderr, "filename:[%s] ", pathname);
        perror("open");
        exit(EXIT_FAILURE);
        ;
    }
    // fprintf(stderr, "opened file fd:[%d] flags:[%d]\n", open_fd, flags); // DEBUG: display file fd
    return open_fd;
}

/**
 * Closes a file descriptor with error handling.
 * @param fd The file descriptor to close.
 * @return None.
 */
void safe_close(int fd)
{
    // fprintf(stderr, "closed fd:[%d]\n", fd); // DEBUG: display closed fds
    if (close(fd) == -1)
    {
        fprintf(stderr, "fd:[%d] ", fd);
        perror("close");
        exit(EXIT_FAILURE);
        ;
    }
}

/**
 * Reads data from a file descriptor with error handling.
 * @param fd The file descriptor to read from.
 * @param buf The buffer to store the read data.
 * @param count The number of bytes to read.
 * @return Returns the number of bytes read if successful; otherwise, exits with an error message.
 */
int safe_read(int fd, void *buf, size_t count)
{
    int n_bytes = read(fd, buf, count);
    if (n_bytes == -1)
    {
        perror("read");
        exit(EXIT_FAILURE);
        ;
    }
    return n_bytes;
}

/**
 * Writes data to a file descriptor with error handling.
 * @param fd The file descriptor to write to.
 * @param buf The buffer containing the data to write.
 * @param count The number of bytes to write.
 * @return None.
 */
void safe_write(int fd, const void *buf, size_t count)
{
    if (write(fd, buf, count) == -1)
    {
        perror("write");
        exit(EXIT_FAILURE);
        ;
    }
}

/**
 * Performs a seek operation on a file descriptor with error handling.
 * @param fd The file descriptor to seek.
 * @param offset The offset to move the file pointer.
 * @param whence The reference point for the offset.
 * @return Returns the resulting offset location if successful; otherwise, exits with an error message.
 */
off_t safe_lseek(int fd, off_t offset, int whence)
{
    off_t offset_loc = lseek(fd, offset, whence);
    if (offset_loc == (off_t)-1)
    {
        perror("lseek");
        exit(EXIT_FAILURE);
    }
    return offset_loc;
}

/**
 * Synchronizes changes to a file mapping with error handling.
 * @param addr The starting address of the file mapping.
 * @param length The length of the file mapping.
 * @param flags Flags specifying the type of synchronization.
 * @return None.
 */
void safe_msync(void *addr, size_t length, int flags)
{
    if (msync(addr, length, flags) == -1)
    {
        perror("msync");
        exit(EXIT_FAILURE);
    }
}

/**
 * Maps files or devices into memory with error handling.
 * @param addr The desired starting address for the mapping.
 * @param length The length of the mapping.
 * @param prot The desired memory protection of the mapping.
 * @param flags The type of the mapping.
 * @param fd The file descriptor of the file to map.
 * @param offset The offset within the file to start the mapping.
 * @return The starting address of the mapped region.
 */
void *safe_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    void *result = mmap(addr, length, prot, flags, fd, offset);
    if (result == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    return result;
}

/**
 * Unmaps files or devices from memory with error handling.
 * @param addr The starting address of the mapped region.
 * @param length The length of the mapped region.
 */
void safe_munmap(void *addr, size_t length)
{
    if (munmap(addr, length) == -1)
    {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
}