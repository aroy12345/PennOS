#ifndef PUSER_FUNCTIONS_H
#define PUSER_FUNCTIONS_H
#include "kernel-functions.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ucontext.h>


extern PCB *current_pcb;
extern int ticks;

int p_spawn(void (*func)(), char *argv[], int fd0, int fd1);
pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang);
int p_kill(pid_t pid, int sig);
int p_nice(pid_t pid, int priority);
void p_sleep(unsigned int time);
void p_exit(void);
bool W_WIFEXITED(int status);
bool W_WIFSTOPPED(int status);
bool W_WIFCONTINUED(int status);
bool W_WIFSIGNALED(int status);

#endif