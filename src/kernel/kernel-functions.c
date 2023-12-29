
#include "PCB.h"
#include "kernel-functions.h"
#include "../logger/logger.h"
#include "../util/globals.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "stdio.h"

/**
 * creates PCB and adds it to global PCB list
 * @param parent the parent of PCB to be created, if it exists
 * @return created PCB
 */
PCB *k_process_create(PCB *parent)
{
    PCB *child = createPCB(parent);
    if (child == NULL)
    {
        return NULL;
    }
    addPCBToList(&pcb_list, child);
    return child;
}

/**
 * sends signal \p signal to inputted PCB \p process
 * @param process pointer of PCB to send signal to
 * @param signal signal to send
 * @return 0 on success; 1 on failure
 */
int k_process_kill(PCB *process, int signal)
{
    
    if (process == NULL)
    {
        return -1;
    }
    log_signaled_event(process->pid, process->priority, process->name);
    if (signal == S_SIGTERM)
    {
        process->status = T_ZOMBIED;
        log_zombie_event(process->pid, process->priority, process->name);

        for (int i = 0; i < process->numChildren; i++)
        {
            PCB *curr = findPCBByPID(process->children[i]);
            log_orphan_event(curr->pid, curr->priority, curr->name);
        }
            // removePCBFromList(&pcb_list, process);
        return 0;
    }
    else if (signal == S_SIGSTOP)
    {
        process->status = T_STOPPED;
        log_stopped_event(process->pid, process->priority, process->name);
        return 0;
    }
    else if (signal == S_SIGCONT)
    {
        process->status = T_RUNNING;
        log_continued_event(process->pid, process->priority, process->name);
        return 0;
    }
    else if (signal == S_SIGCHLD)
    {
        k_process_kill(process, S_SIGCONT);
        return 0;
    }
    else
    {
        return -1;
    }
}


/**
 * frees PCB \p process and all of its descendants
 * @param process pointer of PCB (and its descendants) to be freed
 * @return none
 */
void k_process_deep_cleanup(PCB *process)
{
    if (process != NULL)
    {

        for (int i = 0; i < process->numChildren; i++)
        {
            PCB *curr = findPCBByPID(process->children[i]);
            k_free(curr);
        }

        k_free(process);
    }
}

/**
 * frees PCB \p process
 * @param process pointer of PCB to be freed
 * @return none
 */
void k_process_cleanup(PCB *process)
{
    if (process != NULL)
    {
        process->status = T_ZOMBIED;
        removePCBFromList(&pcb_list, process);
    }
}