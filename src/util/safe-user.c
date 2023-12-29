#include <stdint.h>
#include "safe-user.h"
#include "p-errno.h"
#include "../kernel/puser-functions.h"
#include "../filesystem/filesystem.h"

int safe_f_open(const char *fname, int mode) {
    int res = f_open(fname, mode);
    if (res == -1) {
        p_perror("f_open");
        p_exit();
    }
    return res;
}

int safe_f_read(int fd, int n, char *buf) {
    int res = f_read(fd, n, buf);
    if (res == -1) {
        p_perror("f_read");
        p_exit();
    }
    return res;
}

int safe_f_write(int fd, const char *str, int n) {
    int res = f_write(fd, str, n);
    if (res == -1) {
        p_perror("f_write");
        p_exit();
    }
    return res;
}

int safe_f_close(int fd) {
    int res = f_close(fd);
    if (res == -1) {
        p_perror("f_close");
        p_exit();
    }
    return res;
}

int safe_f_unlink(const char *fname) {
    int res = f_unlink(fname);
    if (res == -1) {
        p_perror("f_unlink");
        p_exit();
    }
    return res;
}

int safe_f_lseek(int fd, int offset, int whence) {
    int res = f_lseek(fd, offset, whence);
    if (res == -1) {
        p_perror("f_lseek");
        p_exit();
    }
    return res;
}

int safe_f_print(const char* str) {
    int res = f_print(str);
    if (res == -1) {
        p_perror("f_print");
        p_exit();
    }
    return res;
}

int safe_p_spawn(void (*func)(), char *argv[], int fd0, int fd1) {
    int res = p_spawn(func, argv, fd0, fd1);
    if (res == -1) {
        p_perror("p_spawn");
        p_exit();
    }
    return res;
}

pid_t safe_p_waitpid(pid_t pid, int *wstatus, bool nohang) {
    int res = p_waitpid(pid, wstatus, nohang);
    if (res == -1) {
        p_perror("p_waitpid");
        p_exit();
    }
    return res;
}

int safe_p_kill(pid_t pid, int sig) {
    int res = p_kill(pid, sig);
    if (res == -1) {
        p_perror("p_kill");
        p_exit();
    }
    return res;
}

int safe_p_nice(pid_t pid, int priority) {
    int res = p_nice(pid, priority);
    if (res == -1) {
        p_perror("p_nice");
        p_exit();
    }
    return res;
}