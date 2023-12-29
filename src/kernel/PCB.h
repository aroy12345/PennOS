#include <ucontext.h>
#include <sys/types.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>

#ifndef PCB_H
#define PCB_H

#define STACKSIZE 4096*256 // TODO: maybe we should increase this
#define MAX_FDS 1024

// file ids
#define NOFILE -1
#define STDIN_ID -2
#define STDOUT_ID -3
#define STDERR_ID -4

typedef struct PCB PCB;

typedef struct PCB
{
   char* name;                      // the name of the process (i.e., "cat")
   ucontext_t *context;
   pid_t parent_pid;
   pid_t pid;
   pid_t children[10000];
   int numChildren;
   int fileDescriptors[MAX_FDS];
   int priority;
   int status; // see util/globals.h for statuses
   struct PCB* next;
} PCB;

extern PCB* pcb_list;
extern pid_t next_pid;

/**
 * free memory of a PCB
 * @param pcb the pcb
 * @return none
*/
void k_free(PCB *pcb);

/**
 * create a `PCB`
 * @param parent the parent process PCB
 * @return the created PCB
*/
PCB* createPCB(PCB* parent);

/**
 * add a process to the PCBList
 * @param list the `PCBList`
 * @param pcb the process `PCB` to add
 * @return none
*/
void addPCBToList(PCB** list, PCB *pcb);

/**
 * remove a process from the PCBList
 * @param list the `PCBList`
 * @param pcb the process `PCB` to remove
 * @return none
*/
void removePCBFromList(PCB** list, PCB *pcb);

/**
 * find a PCB by pid in the global PCB list
 * @param pid the process pid to find
 * @return the `PCB`, or `NULL` if it was not found
*/
PCB *findPCBByPID(pid_t pid);

/**
 * find a PCB by `ucontext` in the global PCB list
 * @param context the process ucontext to find
 * @return the `PCB`, or `NULL` if it was not found
*/
PCB *findPCBByContext(ucontext_t *context);

int getLength(PCB* list);
int count_running(PCB* head);
int count_running_priority(PCB* head, int prio);

#endif // PCB_H