#include <stdio.h>

#include "p-errno.h"
#include "util.h"
#include "../filesystem/filesystem.h"

int ERRNO = ERR_NONE;

/**
 * Returns a string description for the given error code.
 *
 * @param errno The error code.
 * @return A string description for the error code.
 */
char* err_string(int errno) {
    switch (errno) {
        case ERR_NONE                       : return "no error"; break;
        case ERR_FS_FILE_NOT_FOUND          : return "file does not exist"; break;
        case ERR_F_OPEN_INVALID_PERMS       : return "permission denied"; break;
        case ERR_F_OPEN_WRITE_INUSE         : return "another process has write access"; break;
        case ERR_F_OPEN_CREATE_READ         : return "cannot create a file in read mode"; break;
        case ERR_F_OPEN_INVALID_MODE        : return "unknown mode (must be F_WRITE, F_READ, or F_APPEND)"; break;
        case ERR_F_READ_TERM_OUT            : return "cannot read from terminal output (F_STDOUT/F_STDERR)"; break;
        case ERR_F_WRITE_TERM_IN            : return "cannot write to terminal input (F_STDIN)"; break;
        case ERR_F_WRITE_RONLY              : return "current process does not have write access"; break;
        case ERR_F_LSEEK_TERMINAL           : return "cannot seek in a terminal file descriptor"; break;
        case ERR_F_LSEEK_OOB                : return "offset puts file pointer out of bounds"; break;

        case ERR_P_SPAWN_NULL_CHILD         : return "created a null child process"; break;
        case ERR_P_SPAWN_NULL_STACK         : return "stack was not allocated correctly"; break;
        case ERR_P_WAITPID_NULL_CHILD       : return "cannot wait on a pid that was not found"; break;
        case ERR_P_KILL_NULL_PROCESS        : return "cannot kill a pid that was not found"; break;
        case ERR_P_NICE_NULL_PROCESS        : return "cannot change priority of a pid that was not found"; break;

        default: return "undefined error";
    }
}

/**
 * Print an error message along with the corresponding error string.
 * @param message The additional message to print.
 */
void p_perror(const char* message) {
    char buffer[ERRBUFFER_SIZE];
    snprintf(buffer, ERRBUFFER_SIZE, "%s: %s\n", message, err_string(ERRNO));
    f_print(buffer);
}