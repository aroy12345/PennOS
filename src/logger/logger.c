
#include "logger.h"
#include "stdio.h"
#include "../kernel/puser-functions.h"


FILE* logfile;

/**
 * @brief Logs a scheduling event.
 *
 * @param pid Process ID of the scheduled process.
 * @param prio Priority of the scheduled process.
 * @param process_name Name of the scheduled process.
 */
void log_schedule_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t SCHEDULE \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}
/**
 * @brief Logs a process creation event.
 *
 * @param pid Process ID of the created process.
 * @param prio Priority of the created process.
 * @param process_name Name of the created process.
 */
void log_create_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t CREATE \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}

/**
 * @brief Logs a signalling event.
 *
 * @param pid Process ID of the signalled process.
 * @param prio Priority of the signalled process.
 * @param process_name Name of the signalled process.
 */
void log_signaled_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t SIGNALED \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}

/**
 * @brief Logs an exiting event (natural termination via \ref p_exit) event.
 *
 * @param pid Process ID of the exited process.
 * @param prio Priority of the exited process.
 * @param process_name Name of the exited process.
 */
void log_exited_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t EXITED \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}
/**
 * @brief Logs zombie event.
 *
 * @param pid Process ID of the zombied process.
 * @param prio Priority of the zombied process.
 * @param process_name Name of the zombied process.
 */
void log_zombie_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t ZOMBIE \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}

/**
 * @brief Logs orphan event.
 *
 * @param pid Process ID of the ophaned process.
 * @param prio Priority of the ophaned process.
 * @param process_name Name of the ophaned process.
 */
void log_orphan_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t ORPHAN \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}   

/**
 * @brief Logs waiting event.
 *
 * @param pid Process ID of the waiting process.
 * @param prio Priority of the waiting process.
 * @param process_name Name of the waiting process.
 */
void log_waited_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t WAITED \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}


/**
 * @brief Logs a priority change event.
 *
 * @param pid Process ID of the changed process.
 * @param old_prio Old priority of the process.
 * @param new_prio New priority of the process.
 * @param process_name Name of the process.
 */
void log_nice_event(int pid, int old_prio, int new_prio, char* process_name) {
     fprintf(logfile, "[%d ] \t CHANGED \t %d \t %d \t %d \t %s \n", ticks, pid, old_prio, new_prio,process_name);
}

/**
 * @brief Logs a blocked event (via \ref waitpid).
 *
 * @param pid Process ID of the blocked process.
 * @param prio Priority of the process.
 * @param process_name Name of the process.
 */
void log_blocked_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t BLOCKED \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}

/**
 * @brief Logs an unblocked event.
 *
 * @param pid Process ID of the unblocked process.
 * @param prio Priority of the process.
 * @param process_name Name of the process.
 */
void log_unblocked_event(int pid, int prio, char* process_name) {
 fprintf(logfile, "[%d ] \t UNBLOCKED \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}

/**
 * @brief Logs a stopped event (via signalling).
 *
 * @param pid Process ID of the stopped process.
 * @param prio Priority of the process.
 * @param process_name Name of the process.
 */
void log_stopped_event(int pid, int prio, char* process_name) {
   fprintf(logfile, "[%d ] \t STOPPED \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}

/**
 * @brief Logs a continued event.
 *
 * @param pid Process ID of the continued process.
 * @param prio Priority of the process.
 * @param process_name Name of the process.
 */
void log_continued_event(int pid, int prio, char* process_name) {
    fprintf(logfile, "[%d ] \t CONTINUED \t %d \t %d \t %s \n", ticks, pid, prio, process_name);
}