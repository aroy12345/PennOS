
#pragma once

// system call error handling interface

// error handling for open
int safe_open(const char*, int, mode_t);

// error handling for close
void safe_close(int);

// error handling for read
int safe_read(int, void*, size_t);

// error handling for write
void safe_write(int, const void*, size_t);

// error handling for lseek
off_t safe_lseek(int fd, off_t offset, int whence);

// error handling for msync
void safe_msync(void *addr, size_t length, int flags);

// error handling for mmap
void* safe_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

// error handling for munmap
void safe_munmap(void *addr, size_t length);