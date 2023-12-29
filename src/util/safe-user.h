#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

// error handling for f_open
int safe_f_open(const char *fname, int mode);

// error handling for f_read
int safe_f_read(int fd, int n, char *buf);

// error handling for f_write
int safe_f_write(int fd, const char *str, int n);

// error handling for f_close
int safe_f_close(int fd);

// error handling for f_unlink
int safe_f_unlink(const char *fname);

// error handling for f_lseek
int safe_f_lseek(int fd, int offset, int whence);

// error handling for f_print
int safe_f_print(const char* str);

// error handling for p_spawn
int safe_p_spawn(void (*func)(), char *argv[], int fd0, int fd1);

// error handling for p_waitpid
pid_t safe_p_waitpid(pid_t pid, int *wstatus, bool nohang);

// error handling for p_kill
int safe_p_kill(pid_t pid, int sig);

// error handling for p_nice
int safe_p_nice(pid_t pid, int priority);