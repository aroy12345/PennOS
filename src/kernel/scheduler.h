#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "puser-functions.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <ucontext.h>
#include <sys/time.h>

extern ucontext_t schedulerContext;
extern ucontext_t reaperContext;

void init_scheduler();
void start_scheduler();
void setAlarmHandler();
void setTimer();    

#endif // SCHEDULER_H