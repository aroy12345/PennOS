
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

extern int fs_fd; // filesystem file descriptor
extern uint16_t* fat; // FAT

// kernel signals
#define S_SIGSTOP 123 /* a thread receiving this signal should be stopped */
#define S_SIGCONT 456 /* a thread receiving this signal should be continued */
#define S_SIGTERM 789 /* a thread receiving this signal should be terminated */
#define S_SIGCHLD 812 /* a thread receiving this signal should be terminated */

// thread states
#define T_RUNNING 111
#define T_STOPPED 222
#define T_BLOCKED 333
#define T_ZOMBIED 444
#define T_WAITED  555
