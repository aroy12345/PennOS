#include <stdbool.h>

#define NOT_STOPPED -1

typedef struct job job_t;

typedef struct job { // job struct (shell's perspective of processes)
    int job_id;
    int pid;
    int stop_order; // -1 if not stopped
    bool done;
    job_t* next;
} job_t;

/**
 * find a job struct by job_id
 * @param head the job linkedlist
 * @param target_job_id the job_id to find
 * @return the job struct if it was found, or `NULL` otherwise
*/
job_t* job_find_by_jobid(job_t** head, int target_job_id);

/**
 * get last process (most recent background process)
 * @param head the job linkedlist
 * @return the last job_id, or `-1` if the list is empty
*/
int job_get_last(job_t** head);

/**
 * create a job and push it onto the linkedlist
 * @param head the job linkedlist
 * @param job_id the new job_id
 * @param pid the job's pid mapping
 * @param stop_order `-1` or `NOT_STOPPED` if foreground process;
 * main `stop_order` if background process
 * @return the created job struct
*/
job_t* jobs_push(job_t** head, int job_id, int pid, int stop_order);

/**
 * insert a job struct at the correct position to maintain sortedness by job_id
 * @param head the job linkedlist
 * @param new_job the job struct
 * @return none
*/
void jobs_insert(job_t** head, job_t* new_job);

/**
 * remove the specified job and return it
 * @param head the job linkedlist
 * @param target_job_id the job_id to find
 * @return the job struct if it was found, or `NULL` otherwise
*/
job_t* jobs_remove(job_t** head, int target_job_id);

/**
 * DEBUG: print job info
 * @param job the job
*/
void job_print(job_t* job);