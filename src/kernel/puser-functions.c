#include "kernel-functions.h"
#include "scheduler.h"
#include "../filesystem/filesystem.h"
#include "../logger/logger.h"
#include "../util/globals.h"
#include "../util/p-errno.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ucontext.h>
#include <valgrind/valgrind.h>
PCB *current_pcb = NULL;
int ticks = 0;

/**
 * spawns PCB with start function \p func , input arguments \p argv , and I/O \p fd0 / \p fd1
 * @param func start function of PCB
 * @param argv arguments of func
 * @param fd0 F_STDIN file descriptor
 * @param fd1 F_STDOUT file descriptor
 * @return pid of spawned PCB on success; -1 on failure
 */
int p_spawn(void (*func)(), char *argv[], int fd0, int fd1)
{
   
    PCB *child = k_process_create(current_pcb);
  
    if (child == NULL)
    {
        ERRNO = ERR_P_SPAWN_NULL_CHILD;
        return -1;
    }

    // TODO: does this work?
    // I think this is all we need for I/O redirection, but this seems a little too simple
    child->fileDescriptors[F_STDIN] = child->fileDescriptors[fd0];
    child->fileDescriptors[F_STDOUT] = child->fileDescriptors[fd1];

    // TODO: if possible, get around this hack;
    // basically the child needs write perms but only one process can open a file
    // in write mode currently; so we pass write perms from the parent to the child
    // if there's IO redirection and the parent has write perms
    int output_fid = child->fileDescriptors[F_STDOUT];
    if (output_fid >= 0)
    {
        file_t *file_entry = find_file_entry_by_file_id(output_fid);
        file_entry->wr_pid = child->pid;
    }

    // update open files list to include child's pid
    process_create_fileptrs(child);
      
    int argc = 0;
    while (argv != NULL && argv[argc] != NULL)
    {
        argc++;
    }

    // initialize the child's context
    void *stack = malloc(STACKSIZE);
    if (stack == NULL)
    { // check that stack was actually allocated
        k_free(child);
        ERRNO = ERR_P_SPAWN_NULL_STACK;
        return -1;
    }
 
    child->context->uc_stack.ss_sp = stack;
    child->context->uc_stack.ss_size = STACKSIZE;
    child->context->uc_stack.ss_flags = 0;
    sigemptyset(&(child->context->uc_sigmask));
    child->context->uc_link = &reaperContext;
    
    makecontext(child->context, func, 2, argc, argv);
    VALGRIND_STACK_REGISTER(stack, stack + STACKSIZE);
   
    if (argv[0] != NULL)
    {
        child->name = malloc(strlen(argv[0]) + 1);
        if (child->name != NULL)
        {
            strcpy(child->name, argv[0]);
        }
        // printf("Child's name: %s\n", child->name);
    }
    else
    {
        child->name = NULL;
    }
    child->priority = 0;

    log_create_event(child->pid, child->priority, child->name);
    return child->pid;
}

/**
 * if \p nohang is false, waits until relevant PCB(s) changes state
 * if \p nohang is true, returns immediately
 * @param pid pid of PCB to wait for; if -1, wait for any child of current PCB
 * @param wstatus pointer to integer to store status in
 * @param nohang true to return immediately, false to block parent PCB
 * @return 0 on success, -1 on failure
 */
pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang)
{
    
    if (pid == -1)
    {   
        ucontext_t uc;
        getcontext(&uc);
      // PCB *parent = findPCBByContext(&uc);
        // printf("Parent's children: %d\n", current_pcb->numChildren);

        if (!nohang)
        {   
            current_pcb->status = T_WAITED;
            log_blocked_event(current_pcb->pid, current_pcb->priority, current_pcb->name);
             
             if(current_pcb->numChildren==0){
                
                return -1;
             }

    
             for (int i = 0; i < current_pcb->numChildren; i++)
                {   
                    PCB *curr = findPCBByPID(current_pcb->children[i]);
                   
                    
                    if (curr!=NULL && curr->status == T_ZOMBIED)
                    {   
                     
                        log_unblocked_event(current_pcb->pid, current_pcb->priority, current_pcb->name);
                        if(wstatus!=NULL){
                        *wstatus = curr->status;
                        }
                        
                        pid_t store =  curr->pid;
                        removePCBFromList(&pcb_list, curr );
                       
                        return store;
                    }

                   
                }

                if(current_pcb->numChildren==0){
                
                return -1;
             }


            swapcontext(current_pcb->context, &schedulerContext);


            for (int i = 0; i < current_pcb->numChildren; i++)
                {   
                    PCB *curr = findPCBByPID(current_pcb->children[i]);
                  
                    
                    if (curr!=NULL && curr->status == T_ZOMBIED)
                    {   
                     
                        log_unblocked_event(current_pcb->pid, current_pcb->priority, current_pcb->name);
                        if(wstatus!=NULL){
                        *wstatus = curr->status;
                        }
                        
                        pid_t store =  curr->pid;
                        removePCBFromList(&pcb_list, curr );
              
                        return store;
                    }

                   
                }

               
             
               
            
            
        }

        else {
              if(current_pcb->numChildren==0){
                
                return -1;
             }

    
             for (int i = 0; i < current_pcb->numChildren; i++)
                {   
                    PCB *curr = findPCBByPID(current_pcb->children[i]);
                   
                    
                    if (curr!=NULL && curr->status == T_ZOMBIED)
                    {   
                     
                        log_unblocked_event(current_pcb->pid, current_pcb->priority, current_pcb->name);
                        if(wstatus!=NULL){
                        *wstatus = curr->status;
                        }
                        
                        pid_t store =  curr->pid;
                        removePCBFromList(&pcb_list, curr );
                       
                        return store;
                    }

                   
                }
               return 0;
        }

        return -1;
    }
    else
    {
        PCB *child = findPCBByPID(pid);
        if (child == NULL)
        {
            ERRNO = ERR_P_WAITPID_NULL_CHILD;
            return -1;
        }

        if (nohang)
        {
            if (child->status == T_ZOMBIED)
            { 
                if(wstatus!=NULL){
                  *wstatus = child->status;
                }

                pid_t store = child->pid;
                removePCBFromList(&pcb_list,child);
              
                return store;
            }
            else
            {
                return 0;
            }
        }
        else
        {
    
            current_pcb->status = T_WAITED;
            log_blocked_event(current_pcb->pid, current_pcb->priority, current_pcb->name);
            
            if (child->status == T_ZOMBIED)
            { 
                if(wstatus!=NULL){
                  *wstatus = child->status;
                }

                pid_t store = child->pid;
                removePCBFromList(&pcb_list,child);
              
                return store;
            }

            swapcontext(current_pcb->context, &schedulerContext);

             if (child->status == T_ZOMBIED)
            { 
                if(wstatus!=NULL){
                  *wstatus = child->status;
                }

                pid_t store = child->pid;
                removePCBFromList(&pcb_list,child);
              
                return store;
            }
            
           return 0;
          
        }
    }
}

/**
 * sends signal \p sig to PCB with pid \p pid
 * @param pid pid of PCB to send signal to
 * @param sig signal to send
 * @return 0 on sucess; -1 on failure
 */
int p_kill(pid_t pid, int sig)
{
    PCB *process = findPCBByPID(pid);
   
    if (process == NULL)
    {
        ERRNO = ERR_P_KILL_NULL_PROCESS;
        return -1;
    }
    process_delete_fileptrs(process);
    k_process_kill(process, sig);
    return 0;
}

/**
 * changes priority of PCB with pid \p pid to inputted priority \p priority
 * @param pid pid of PCB to change priority of
 * @param priority priority to change to
 * @return 0 on sucess; -1 on failure
 */
int p_nice(pid_t pid, int priority)
{
    PCB *process = findPCBByPID(pid);
    int old = process->priority;
    if (process == NULL) {
        ERRNO = ERR_P_NICE_NULL_PROCESS;
        return -1;
    }

    removePCBFromList(&pcb_list, process);
    addPCBToList(&pcb_list, process);

    process->priority = priority;
    log_nice_event(pid, old, process->priority, process->name);
    return 0;
}

/**
 * blocks current PCB for \p time ticks
 * @param time ticks to block for
 * @return none
 */
void p_sleep(unsigned int time)
{
    // TODO: this implementation doesn't match the writeup;
    // it needs to set the current_pcb status to T_BLOCKED,
    // then set it to running after some time.
    // time is in OS ticks
    PCB *caller = current_pcb;
    // caller->status = T_BLOCKED;
    int startTime = ticks;
    log_blocked_event(caller->pid, caller->priority, caller->name);
    while (ticks - startTime < time)
    {
        sleep(1);
        // printf("%d %d\n", ticks, startTime);
    }
    log_unblocked_event(caller->pid, caller->priority, caller->name);
    // caller->status = T_RUNNING;
}

/**
 * exits current PCB unconditionally
 * @return none
 */
void p_exit(void)
{

  
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    // TODO: this is a hack to unblock the parent when the child exits
    // if the parent calls p_waitpid. Is there a better way to do this?
    
    log_exited_event(current_pcb->pid, current_pcb->priority, current_pcb->name);
    process_delete_fileptrs(current_pcb);   // delete the current_pcb's file pointers
    current_pcb->status = T_ZOMBIED;       // set the status of the current_pcb to T_ZOMBIED
    log_zombie_event(current_pcb->pid, current_pcb->priority, current_pcb->name);

     for(int i = 0; i<current_pcb->numChildren; i++){
        PCB* curr = findPCBByPID(current_pcb->children[i]);
        log_orphan_event(curr->pid, curr->priority, curr->name);
    }
    
    
    PCB *parent_pcb = findPCBByPID(current_pcb->parent_pid);
    if(parent_pcb==NULL) {
         
       
      removePCBFromList(&pcb_list, current_pcb);
    }

    if (parent_pcb != NULL && parent_pcb->status == T_WAITED)
    {
        parent_pcb->status = T_RUNNING;
        log_continued_event(parent_pcb->pid, parent_pcb->priority, parent_pcb->name);
      
    }

    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

/**
 * @return true if the child terminated normally, that is, by a call to \ref p_exit or by returning
 */
bool W_WIFEXITED(int status)
{
    return status == T_ZOMBIED;
}

/**
 * @return true if the child was stopped by a signal
 */
bool W_WIFSTOPPED(int status)
{
    return status == T_STOPPED;
}

/**
 * @return true if the child was continued by a signal
 */
bool W_WIFCONTINUED(int status)
{
    return status == T_RUNNING;
}

/**
 * @return true if the child was terminated by a signal
 */
bool W_WIFSIGNALED(int status)
{
    return status == T_ZOMBIED;
}