// error ids

#define ERR_NONE                    0
/* format (from left to right):
1 digit for each file
2 digits for each function
1 digit for each unique error within the function
*/
// filesystem.c
#define ERR_FS_FILE_NOT_FOUND       1000
#define ERR_F_OPEN_INVALID_PERMS    1010
#define ERR_F_OPEN_WRITE_INUSE      1011
#define ERR_F_OPEN_CREATE_READ      1012
#define ERR_F_OPEN_INVALID_MODE     1013
#define ERR_F_READ_TERM_OUT         1020
#define ERR_F_WRITE_TERM_IN         1030
#define ERR_F_WRITE_RONLY           1031
#define ERR_F_CLOSE_TERMINAL        1040
#define ERR_F_UNLINK_NOT_FOUND      1050
#define ERR_F_LSEEK_TERMINAL        1060
#define ERR_F_LSEEK_OOB             1061
// puser-functions.c
#define ERR_P_SPAWN_NULL_CHILD      2000
#define ERR_P_SPAWN_NULL_STACK      2001
#define ERR_P_WAITPID_NULL_CHILD    2010
#define ERR_P_KILL_NULL_PROCESS     2020
#define ERR_P_NICE_NULL_PROCESS     2030

extern int ERRNO;

/**
 * print a message describing the meaning of the value of ERRNO
 * @param message the message to print
 * @return none
*/
void p_perror(const char* message);