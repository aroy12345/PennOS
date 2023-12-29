#include <stdlib.h>
#include <stdio.h>

#include "job-list.h"
#include "../util/util.h"
#include "../util/p-errno.h"
#include "../filesystem/filesystem.h"
#include "../kernel/puser-functions.h"

/**
 * Finds a job by its job ID in the linked list of jobs.
 * @param head A pointer to the head of the linked list of jobs.
 * @param target_job_id The job ID to search for.
 * @return Returns a pointer to the found job or NULL if not found.
 */
job_t* job_find_by_jobid(job_t** head, int target_job_id) {
    job_t* current = *head;
    while (current != NULL) {
        if (current->job_id == target_job_id) return current;
        current = current->next;
    }
    return NULL;
}

/**
 * Gets the job ID of the last job in the linked list.
 * @param head A pointer to the head of the linked list of jobs.
 * @return Returns the job ID of the last job or -1 if the list is empty.
 */
int job_get_last(job_t** head) {
    if (*head == NULL) return -1; // list is empty
    job_t* current = *head;
    while (current->next != NULL) current = current->next;
    return current->job_id;
}

/**
 * Pushes a new job to the end of the linked list.
 * @param head A pointer to the head of the linked list of jobs.
 * @param job_id The job ID of the new job.
 * @param pid The process ID associated with the job.
 * @param stop_order The order in which the job should be stopped.
 * @return Returns a pointer to the newly added job.
 */
job_t* jobs_push(job_t** head, int job_id, int pid, int stop_order) {
    if (*head == NULL) {
        *head = safe_malloc(sizeof(job_t));
        (*head)->job_id = job_id;
        (*head)->pid = pid;
        (*head)->stop_order = stop_order;
        (*head)->done = false;
        (*head)->next = NULL;
        return (*head);
    } else {
        job_t* curr = *head;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = safe_malloc(sizeof(job_t));
        curr->next->job_id = job_id;
        curr->next->pid = pid;
        curr->next->stop_order = stop_order;
        curr->next->done = false;
        curr->next->next = NULL;
        return (curr->next);
    }
}

/**
 * Inserts a new job into the linked list in ascending order based on job ID.
 * @param head A pointer to the head of the linked list of jobs.
 * @param new_job A pointer to the new job to be inserted.
 */
void jobs_insert(job_t** head, job_t* new_job) {
    if (*head == NULL || (*head)->job_id >= new_job->job_id) {
        new_job->next = *head;
        *head = new_job;
    } else {
        job_t* curr = *head;
        while (curr->next != NULL && curr->next->job_id < new_job->job_id) {
            curr = curr->next;
        }
        new_job->next = curr->next;
        curr->next = new_job;
    }
}

/**
 * Removes a job with the specified job ID from the linked list.
 * @param head A pointer to the head of the linked list of jobs.
 * @param target_job_id The job ID of the job to be removed.
 * @return A pointer to the removed job, or NULL if not found.
 */
job_t* jobs_remove(job_t** head, int target_job_id) {
    job_t* curr = *head;
    job_t* prev = NULL;
    while (curr != NULL && curr->job_id != target_job_id) {
        prev = curr;
        curr = curr->next;
    }
    if (curr == NULL) return NULL; // job_id not found
    if (prev == NULL) {
        *head = curr->next; // remove head
    } else { // remove between prev/curr
        prev->next = curr->next;
    }
    return curr;
}

/**
 * Prints information about a job.
 * @param job A pointer to the job structure to be printed.
 */
void job_print(job_t* job) {
    if (job == NULL) {
        f_print("error: null job\n");
        return;
    }
    // fprintf(stderr, "print pid[%d], jobid[%d], done[%d]\n", job->pid, job->job_id, job->done);
    char buffer[1000];
    
    int status = 0;
    int wait_res = p_waitpid(job->pid, &status, true);
    char* job_status;
    if (wait_res > 0) {
        if (W_WIFSTOPPED(status)) job_status = "stopped";
        if (W_WIFCONTINUED(status)) job_status = "continued";
        if (W_WIFEXITED(status)) job_status = "finished";
    } else if (wait_res == 0) {
        if (job->stop_order == NOT_STOPPED) job_status = "stopped";
        else job_status = "running";
    } else {
        p_perror("p_waitpid");
        exit(EXIT_FAILURE);
    }

    snprintf(buffer, 1000, "[%d] pid:[%d] (%s)\n", job->job_id, job->pid, job_status);
    f_print(buffer);
}