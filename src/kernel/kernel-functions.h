#ifndef KERNEL_FUNCTIONS_H
#define KERNEL_FUNCTIONS_H

#include "PCB.h" // Include PCB.h for PCB structure and functions

// Function prototypes
PCB *k_process_create(PCB *parent);
int k_process_kill(PCB *process, int signal);
void k_process_deep_cleanup(PCB *process);
void k_process_cleanup(PCB *process);

#endif // KERNEL_FUNCTIONS_H