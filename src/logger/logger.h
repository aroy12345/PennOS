
#pragma once

#include "stdio.h"


extern FILE* logfile;

void log_schedule_event(int pid, int prio, char* process_name);

void log_create_event(int pid, int prio, char* process_name);
void log_signaled_event(int pid, int prio, char* process_name);
void log_exited_event(int pid, int prio, char* process_name);
void log_zombie_event(int pid, int prio, char* process_name);
void log_orphan_event(int pid, int prio, char* process_name);
void log_waited_event(int pid, int prio, char* process_name);

void log_nice_event(int pid, int old_prio, int new_prio, char* process_name);

void log_blocked_event(int pid, int prio, char* process_name);
void log_unblocked_event(int pid, int prio, char* process_name);

void log_stopped_event(int pid, int prio, char* process_name);
void log_continued_event(int pid, int prio, char* process_name);

