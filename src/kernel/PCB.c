#include "PCB.h"
#include "scheduler.h"
#include <stdio.h>
#include "../util/globals.h"
#include <valgrind/valgrind.h>

pid_t next_pid = 1;
PCB *pcb_list = NULL;

/**
 * retrieves tail of a circular linked list \p circular_ll
 * @param circular_ll pointer to head of circular linked list
 * @return tail of inputted linked list
 */
PCB *get_tail(PCB *circular_ll)
{
    PCB *curr = circular_ll;
    while (curr->next != circular_ll)
    {
        curr = curr->next;
    }
    return curr;
}

/**
 * frees inputted PCB \p process
 * @param process pointer to PCB to be freed
 * @return none
 */
void k_free(PCB *process)
{
    if (process != NULL)
    {
        if (process->context != NULL)
        {

            if (process->context->uc_stack.ss_sp != NULL)
            {
                VALGRIND_STACK_DEREGISTER(process->context->uc_stack.ss_sp);
                free(process->context->uc_stack.ss_sp);
                process->context->uc_stack.ss_sp = NULL;
            }

            free(process->context);
            process->context = NULL;
        }

        free(process->name);
        free(process);
    }
}

/**
 * creates PCB
 * if parent \p Parent isn't NULL, parent_pid and priority will be inherited
 * instantiated PCB will also be added to \p Parent 's array of children
 * @param Parent of parent (if it exists) for the newly created PCB.
 * @return returns the newly created PCB.
 */
PCB *createPCB(PCB *Parent)
{
    PCB *new_pcb = (PCB *)malloc(sizeof(PCB));

    if (new_pcb)
    {
        new_pcb->pid = next_pid;
        new_pcb->status = T_RUNNING;
        next_pid++;
        new_pcb->numChildren = 0;

        new_pcb->context = (ucontext_t *)malloc(sizeof(ucontext_t)); // Allocate on the heap
        if (new_pcb->context == NULL)
        { // Check if malloc was successful
            k_free(new_pcb);
            return NULL;
        }

        if (Parent)
        { // if parent, then need to update children of parent
            Parent->children[Parent->numChildren] = new_pcb->pid;
            Parent->numChildren++;

            new_pcb->parent_pid = Parent->pid;
            new_pcb->priority = Parent->priority;
            if (getcontext(new_pcb->context) == -1)
            {
                k_free(new_pcb);
                return NULL;
            }
        }
        else
        { // if no parent, then job will be at the root level
            new_pcb->parent_pid = 0;
            new_pcb->priority = 0;
        }

        if (getcontext(new_pcb->context) == -1)
        { // try to get the current context
            k_free(new_pcb);
            return NULL;
        }

        // set fields in the new PCB
        new_pcb->priority = 0;
        new_pcb->next = NULL;

        if (Parent)
        { // if parent, copy the parent's file descriptors
            for (int i = 0; i < MAX_FDS; i++)
            {
                new_pcb->fileDescriptors[i] = Parent->fileDescriptors[i];
            }
        }
        else
        { // if no parent, then file descriptors all start empty, except for 0, 1, 2 (in/out/err)
            for (int i = 3; i < MAX_FDS; i++)
            {
                new_pcb->fileDescriptors[i] = NOFILE;
            }
            new_pcb->fileDescriptors[0] = STDIN_ID;
            new_pcb->fileDescriptors[1] = STDOUT_ID;
            new_pcb->fileDescriptors[2] = STDERR_ID;
        }

        return new_pcb;
    }

    return NULL;
}

/**
 * adds a given a PCB \p pcb to list
 * @param head pointer to head of circular linked list
 *  @param pcb the pcb we want to add
 * @return None
 */
void addPCBToList(PCB **head, PCB *pcb)
{
    if (*head == NULL)
    {
        pcb->next = pcb;
        *head = pcb;
    }
    else
    {
        PCB *tail = get_tail(*head);
        tail->next = pcb;
        pcb->next = *head;
    }
}

/**
 * removes a given a PCB \p pcb from a given list
 * @param head pointer to head of circular linked list
 * @param pcb the pcb we want to remove
 * @return None
 */
void removePCBFromList(PCB **head, PCB *pcb)
{
    if (*head == NULL)
    {
        return;
    }

    if (pcb == *head)
    {
        PCB *prev = get_tail(pcb);
        if (prev == pcb)
        {
            *head = NULL;
        }
        else
        {
            prev->next = pcb->next;
            *head = pcb->next;
        }
    }
    else
    {
        PCB *prev = get_tail(pcb);
        prev->next = pcb->next;
    }

    if (*head != NULL)
    {
        if (pcb->status == T_ZOMBIED)
        {
            for (int i = 0; i < pcb->numChildren; i++)
            {
                PCB *child = findPCBByPID(pcb->children[i]);
                child->parent_pid = -1;
            }
            if (pcb->parent_pid != 0)
            {
                PCB *parent = findPCBByPID(pcb->parent_pid);
                int index = -1;
                for (int i = 0; i < parent->numChildren; i++)
                {
                    if (parent->children[i] == pcb->pid)
                    {
                        index = i;
                    }
                }
                parent->children[index] = parent->children[parent->numChildren - 1];
                parent->numChildren--;
            }
            k_free(pcb);
        }
    }
}

/**
 * finds PCB with desired pid \p pid
 * @param pid the pid of the process we want to find
 * @return PCB with desired pid, if it exists
 */
PCB *findPCBByPID(pid_t pid)
{
    if (pcb_list != NULL)
    {
        PCB *head = pcb_list;
        PCB *curr = pcb_list;
        while (true)
        {
            if (curr->pid == pid)
            {
                return curr;
            }
            curr = curr->next;
            if (curr == head)
            {
                break;
            }
        }
    }
    return NULL;
}

/**
 * finds PCB with desired ucontext_t \p context
 * @param context the pointer of the given context
 * @return PCB with desired context, if it exists
 */
PCB *findPCBByContext(ucontext_t *context)
{
    if (pcb_list != NULL)
    {
        PCB *head = pcb_list;
        PCB *curr = pcb_list;

        if (head->context == context)
        {
            return head;
        }
        else
        {
            while (curr->next != head)
            {
                if (curr->context == context)
                {
                    return curr;
                }
                curr = curr->next;
            }
        }
    }
    return NULL;
}

/**
 * gets length of circular linked list
 * @param head pointer to the head of circular linked list
 * @return the length of the pcb list
 */
int getLength(PCB *head)
{
    if (head == NULL)
    {
        return 0;
    }
    else
    {
        PCB *curr = head;
        int len = 1;
        while (curr->next != head)
        {
            len++;
            curr = curr->next;
        }
        return len;
    }
}

/**
 * count number of T_RUNNING processes in a circular linked list
 * @param head pointer to the head of circular linked list
 * @return number of T_RUNNING processes
 */
int count_running(PCB *head)
{
    if (head == NULL)
    {
        return 0;
    }
    else
    {
        PCB *curr = head;
        int len = 0;
        if (head->status == T_RUNNING)
            len = 1;
        while (curr->next != head)
        {
            if (curr->next->status == T_RUNNING)
                len++;
            curr = curr->next;
        }
        return len;
    }
}

/**
 * count number of T_RUNNING processes with desired priority \p prio in a circular linked list
 * @param head pointer to the head of circular linked list
 * @param prio desired priority (-1, 0, or 1)
 * @return number relevant processes
 */
int count_running_priority(PCB *head, int prio)
{
    if (head == NULL)
    {
        return 0;
    }
    else
    {
        PCB *curr = head;
        int len = 0;
        if (head->status == T_RUNNING && head->priority == prio)
            len = 1;
        while (curr->next != head)
        {
            if (curr->next->status == T_RUNNING && curr->next->priority == prio)
                len++;
            curr = curr->next;
        }
        return len;
    }
}
