#include <stdio.h>
#include <stdlib.h>

#include "../util/parser.h"
#include "../util/util.h"
#include "../util/globals.h"
#include "../util/p-errno.h"
#include "../util/safe-user.h"
#include "../filesystem/filesystem.h"
#include "../kernel/puser-functions.h"
#include "../kernel/stress.h"
#include "../logger/logger.h"
#include "job-list.h"

#define PROMPT "$ "
#define CONTINUE {free(command); continue;}

const int MAX_ARGUMENTS = 1000;
const char* MAN_COMMANDS = "\
--- Independently scheduled processes ---\n\
cat OUTPUT\n\
sleep SECONDS\n\
busy\n\
echo [ STRING ]\n\
ls [ FILENAME ]\n\
touch FILE ...\n\
mv SRC DEST\n\
cp SRC DEST\n\
rm FILE ...\n\
chmod FILE PERM\n\
ps\n\
kill [ -SIGNAL_NAME ] PID ...\n\
zombify\n\
orphanify\n\
\n\
--- Shell subroutines ---\n\
nice PRIORITY COMMAND [ ARG ]\n\
nice_pid PRIORITY PID\n\
man\n\
bg [ JOB_ID ]\n\
fg [ JOB_ID ]\n\
jobs\n\
logout\n\
";

job_t* foreground_job = NULL; // foreground job
int current_jobid; // current job_id (either current job or last stopped job)
int jobid_ctr = 1; // global job_id counter 
int stop_order = 1; // maintain order of stopped jobs
job_t* head = NULL; // first bg process
job_t** background = &head; // bg queue

int n_reaped = 0; // number of zombies reaped
job_t* reaped[1000]; // holds reaped processes temporarily

bool stop_trigger = false; // whether S_SIGSTOP was triggered
void stop_handler(int signal) {
    if (foreground_job == NULL) return;
    safe_f_print("stopped ");
    job_print(foreground_job);
    stop_trigger = true;
    safe_p_kill(foreground_job->pid, S_SIGSTOP);
}

void term_handler(int signal) {
    if (foreground_job == NULL) return;
    safe_f_print("terminated ");
    job_print(foreground_job);
    printf("name: %s\n",findPCBByPID(foreground_job->pid)->name);
    safe_p_kill(foreground_job->pid, S_SIGTERM);
}


void shell_cat(int argc, char* argv[]) {
    char buffer[IOBUFFER_SIZE+1]; // for simplicity, we assume no file has more than 10001 bytes
    if (argc == 1) { // read from stdin
        int bytes_read = safe_f_read(F_STDIN, IOBUFFER_SIZE, buffer);
        safe_f_write(F_STDOUT, buffer, bytes_read);
    } else { // read from filenames in argv
        for (int i = 1; i < argc; i++) {
            int fd = safe_f_open(argv[i], F_READ);
            int bytes_read = safe_f_read(fd, IOBUFFER_SIZE, buffer); // for simplicity, we assume no file has more than 10001 bytes
            safe_f_write(F_STDOUT, buffer, bytes_read);
            safe_f_close(fd);
        }
    }
    p_exit();
}

void shell_sleep(int argc, char* argv[]) {
    if (argc == 2) {
        char* time_string = argv[1];
        int time = atoi(time_string);
        p_sleep(time * 100); // assuming 100 ticks per second
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "sleep expected 2 args but got:[%d]\n", argc);
        safe_f_print(buffer);
    }
    p_exit();
}

void shell_busy(int argc, char* argv[]) {
    while (1) {}
    p_exit();
}

void shell_echo(int argc, char* argv[]) {
    char buffer[IOBUFFER_SIZE];
    int buff_idx = 0;
    for (int i = 1; i < argc; i++) {
        for (int j = 0; argv[i][j] != '\0'; j++) { // copy argv[i]
            buffer[buff_idx++] = argv[i][j];
        }
        if (i < argc - 1) buffer[buff_idx++] = ' '; // add space between remaining args
    }
    buffer[buff_idx++] = '\n';
    buffer[buff_idx] = '\0';
    safe_f_write(F_STDOUT, buffer, strlen(buffer));
    p_exit();
}

void shell_ls(int argc, char* argv[]) {
    if (argc == 1) {
        f_ls(NULL);
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "ls expected no args but got:[%d]\n", argc - 1);
        safe_f_print(buffer);
    }
    p_exit();
}

void shell_touch(int argc, char* argv[]) {
    if (argc >= 2) {
        f_touch(&argv[1], argc - 1);
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "touch expected 1+ args but got:[%d]\n", argc - 1);
        safe_f_print(buffer);
    }
    p_exit();
}

void shell_mv(int argc, char* argv[]) {
    if (argc == 3) {
        f_mv(argv[1], argv[2]);
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "mv expected 2 args but got:[%d]\n", argc - 1);
        safe_f_print(buffer);
    }
    p_exit();
}

void shell_cp(int argc, char* argv[]) {
    if (argc == 3) {
        f_cp(argv[1], argv[2]);
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "cp expected 2 args but got:[%d]\n", argc - 1);
        safe_f_print(buffer);
    }
    p_exit();
}

void shell_rm(int argc, char* argv[]) {
    if (argc >= 2) {
        f_rm(&argv[1], argc - 1);
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "rm expected 1+ args but got:[%d]\n", argc - 1);
        safe_f_print(buffer);
    }
    p_exit();
}

void shell_chmod(int argc, char* argv[]) {
    if (argc == 3) {
        char* perms_string = argv[1];
        int perms = 0;
        if (perms_string[0] == 'r') perms |= FPERM_READ;
        if (perms_string[1] == 'w') perms |= FPERM_WRIT;
        if (perms_string[2] == 'x') perms |= FPERM_EXEC;
        f_chmod(argv[2], perms);
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "chmod expected 2 args but got:[%d]\n", argc - 1);
        safe_f_print(buffer);
    }
    p_exit();
}

void shell_ps(int argc, char* argv[]) {
    char buffer[ERRBUFFER_SIZE];
    PCB* curr = pcb_list; // TODO: make p_ps for abstraction
    if (curr == NULL) p_exit();
    while (true) {
        snprintf(buffer, ERRBUFFER_SIZE, "%d %d %d\n", curr->pid, curr->parent_pid, curr->priority);
        safe_f_print(buffer);
        curr = curr->next;
        if (curr == pcb_list) break;
    }
    p_exit();
}

void shell_kill(int argc, char* argv[]) {
    if (argc >= 3) {
        int signal; // signal to send
        int first_process; // 2 if a signal was specified by argument, 1 otherwise
        if (strcmp(argv[1], "-stop") == 0) {signal = S_SIGSTOP; first_process = 2;}
        else if (strcmp(argv[1], "-cont") == 0) {signal = S_SIGCONT; first_process = 2;}
        else if (strcmp(argv[1], "-term") == 0) {signal = S_SIGTERM; first_process = 2;}
        else {signal = S_SIGTERM; first_process = 1;} // terminate by default

        for (int i = first_process; i < argc; i++) {
            char* pid_string = argv[i];
            int pid = atoi(pid_string);
            safe_p_kill(pid, signal);
        }
    } else {
        char buffer[ERRBUFFER_SIZE];
        snprintf(buffer, ERRBUFFER_SIZE, "kill expected 2+ args but got:[%d]\n", argc - 1);
        safe_f_print(buffer);
    }
    p_exit();
}

void zombie_child() {
   p_exit();
}
void shell_zombify(int argc, char* argv[]) {
    char* empty[1] = { "zombie_child" };
    safe_p_spawn(zombie_child, empty, F_STDIN, F_STDOUT);
    while (1) {

    }
    p_exit();
}

void orphan_child() {
   
    while (1) {

    }
}
void shell_orphanify(int argc, char* argv[]) {
    char* empty[1] = {"orphan_child" };

    safe_p_spawn(orphan_child, empty, F_STDIN, F_STDOUT);
    p_exit();
}

// helper function to recursively cull bg processes
void cull_helper(job_t* job) {
    if (job == NULL) {
        return;
    } else {
        cull_helper(job->next);
        int status;
        int wait_res = safe_p_waitpid(job->pid, &status, true);
        if (wait_res > 0) {
            if (W_WIFEXITED(status)) {
                job->done = true;
            } else if (W_WIFSTOPPED(status)) {
                // fprintf(stderr, "Stopped: pid[%d]\n", job->pid);
                job->stop_order = stop_order++; // new most recently stopped
            } else if (W_WIFSIGNALED(status)) {
                // fprintf(stderr, "Signaled: pid[%d]\n", job->pid);
                job_t* corpse = jobs_remove(background, job->job_id);
                free(corpse);
                return;
            }
        }
        if (job->done) {
            job_t* removed = jobs_remove(background, job->job_id);
            if (removed == NULL) return;
            reaped[n_reaped] = removed;
            n_reaped++;
        }
    }
}

// cull bg processes
void cull_background() {
    cull_helper(*background);
}

void debug_print_jobs() {
    if (*background == NULL) {
        // printf("empty bg\n");
        return;
    }
    job_t* curr = *background;
    while (curr != NULL) {
        // fprintf(stderr, "pid[%d] jobid[%d]\n", curr->pid, curr->job_id);
        job_print(curr);
        curr = curr->next;
    }
}

// empty the reaped queue and print all reaped zombies
void empty_reaped() {
    for (int i = 0; i < n_reaped; i++) {
        // if (reaped[i]->pid < 1) continue; // something weird happened
        safe_f_print("finished ");
        job_print(foreground_job);
        free(reaped[i]);
    }
    n_reaped = 0;
}

int spawn_command(char* command[], int in_fd, int out_fd) {
    if (strcmp(command[0], "cat") == 0) { // The usual cat from bash, etc.
        return safe_p_spawn(shell_cat, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "sleep") == 0) { // sleep
        return safe_p_spawn(shell_sleep, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "busy") == 0) { // busy wait indefinitely.
        return safe_p_spawn(shell_busy, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "echo") == 0) { // similar to echo(1) in the VM.
        return safe_p_spawn(shell_echo, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "ls") == 0) { // list all files in the working directory (similar to ls -il in bash), same formatting as ls in the standalone PennFAT.
        return safe_p_spawn(shell_ls, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "touch") == 0) { // create an empty file if it does not exist, or update its timestamp otherwise.
        return safe_p_spawn(shell_touch, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "mv") == 0) { // rename src to dest
        return safe_p_spawn(shell_mv, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "cp") == 0) { // copy src to dest
        return safe_p_spawn(shell_cp, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "rm") == 0) { // remove files
        return safe_p_spawn(shell_rm, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "chmod") == 0) { // similar to chmod(1) in the VM
        return safe_p_spawn(shell_chmod, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "ps") == 0) { // list all processes on PennOS. Display pid, ppid, and priority.
        return safe_p_spawn(shell_ps, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "kill") == 0) { // send specified signal or kill to the processes
        return safe_p_spawn(shell_kill, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "zombify") == 0) { // execute the following code or its equivalent in your API using safe_p_spawn:
        return safe_p_spawn(shell_zombify, command, in_fd, out_fd);
    } 
    else if (strcmp(command[0], "orphanify") == 0) { // execute the following code or its equivalent in your API using safe_p_spawn:
        return safe_p_spawn(shell_orphanify, command, in_fd, out_fd);
    } 
     else if (strcmp(command[0], "hang") == 0) { // execute the following code or its equivalent in your API using safe_p_spawn:
        return safe_p_spawn(hang, command, in_fd, out_fd);
    } 
     else if (strcmp(command[0], "recur") == 0) { // execute the following code or its equivalent in your API using safe_p_spawn:
        return safe_p_spawn(recur, command, in_fd, out_fd);
    } else if (strcmp(command[0], "nohang") == 0) { // execute the following code or its equivalent in your API using safe_p_spawn:
        return safe_p_spawn(nohang, command, in_fd, out_fd);
    } 
    
    else {
        return -1;
    }
}

/**
 * start a (single) independent pennOS process & return the pid;
 * does not wait for the process
 * @param command the command to parse & execute
 * @param in_filename `NULL` for `F_STDIN`, otherwise the input file
 * @param out_filename `NULL` for `F_STDOUT`, otherwise the output file
 * @param append_mode whether to append to `out_filename`; ignore if `out_filename == NULL`
 * @return the pid of the spawned process
*/
int execute_command(char* command[], const char* in_filename, const char* out_filename, bool append_mode) {
    int in_fd = F_STDIN;
    int out_fd = F_STDOUT;
    // print_fileptr_pids_all();
    if (in_filename != NULL) in_fd = safe_f_open(in_filename, F_READ);
    if (out_filename != NULL) {
        if (append_mode) out_fd = safe_f_open(out_filename, F_APPEND);
        else out_fd = safe_f_open(out_filename, F_WRITE);
    }
    // print_fileptr_pids_all();

    // printf("%d\n", in_fd);

    return spawn_command(command, in_fd, out_fd);
}

int execute_script(char* command_in[], const char* in_filename, const char* out_filename, bool append_mode) {
    int script_fd = f_open(command_in[0], F_READ);
    if (script_fd == -1) return -1;
    int in_fd = F_STDIN;
    int out_fd = F_STDOUT;
    if (in_filename != NULL) in_fd = safe_f_open(in_filename, F_READ);
    if (out_filename != NULL) {
        if (append_mode) out_fd = safe_f_open(out_filename, F_APPEND);
        else out_fd = safe_f_open(out_filename, F_WRITE);
    }

    char line[IOBUFFER_SIZE]; // buffer for read

    safe_f_print("executing script\n");
    int file_size = safe_f_read(script_fd, IOBUFFER_SIZE, line);

    struct parsed_command* command;
    char* token = strtok(line, "\n");
    while (token != NULL) {
        int n_bytes = strlen(token);
        if (n_bytes == 0) safe_f_print("\n"); // line was flushed on empty input
        token[n_bytes] = '\0'; // add null terminator
        if (token[n_bytes - 1] != '\n') safe_f_print("\n"); // line was flushed without [enter]
        safe_f_print("$$ ");
        safe_f_print(token);

        // parse input
        int parse_command_res = parse_command(token, &command);

        if (parse_command_res < 0) {
            perror("parse_command"); // should remain normal perror
            p_exit();
        } else if (parse_command_res > 0) {
            safe_f_print("invalid command\n");
            CONTINUE
        }

        int pid = spawn_command(command->commands[0], in_fd, out_fd);
        if (pid != -1) { // valid command was spawned
            int status;
            safe_p_waitpid(pid, &status, false);
        }

        token = strtok(NULL, "\n");
    }
    free(command);
    safe_f_print("\n");
    return 0;
}


void pennos_shell(int argc, char* argv[]) {
    char line[IOBUFFER_SIZE]; // buffer for read

    while (1) {
        // prompt & get input
        safe_f_print("$ ");
        int n_bytes = safe_f_read(STDIN_FILENO, IOBUFFER_SIZE, line);

        if (n_bytes == 0) safe_f_print("\n"); // line was flushed on empty input
        line[n_bytes] = '\0'; // add null terminator
        if (line[n_bytes - 1] != '\n') safe_f_print("\n"); // line was flushed without [enter]

        // parse input
        struct parsed_command* command;
        int parse_command_res = parse_command(line, &command);
        if (parse_command_res < 0) {
            perror("parse_command"); // should remain normal perror
            p_exit();
        } else if (parse_command_res > 0) {
            safe_f_print("invalid command\n");
            CONTINUE
        }

        if (command->num_commands == 0) { // empty line should cull bg
            cull_background();
            empty_reaped();
            CONTINUE
        }
        int command_argc = get_argc(command->commands[0]);

        { // get current job_id
            job_t* current = *background;
            int max_order = 0;
            int max_job = 0;
            while (current != NULL) {
                if (current->stop_order != NOT_STOPPED) {
                    if (current->stop_order > max_order) {
                        max_order = current->stop_order;
                        max_job = current->job_id;
                    }
                }
                current = current->next;
            }
            if (max_order == 0) { // no stopped jobs, get most recently executed one
                current_jobid = job_get_last(background);
            } else { // use most recently stopped
                current_jobid = max_job;
            }
        } 
        
        if (strcmp(command->commands[0][0], "nice") == 0) { // nice priority command [arg] (S) set the priority of the command to priority and execute theCONTINUE
            char* priority_string = command->commands[0][1];
            int priority = atoi(priority_string);
            
            int pid = execute_command(&(command->commands[0][2]), NULL, NULL, false);
            if (pid == -1) {
                continue;
            } else {
                safe_p_nice(pid, priority);
                int status;
                safe_p_waitpid(pid, &status, false);
            }
        } 
        else if (strcmp(command->commands[0][0], "nice_pid") == 0) { // nice_pid priority pid (S) adjust the nice level of process pid to priority priority.
            char* priority_string = command->commands[0][1];
            int priority = atoi(priority_string);

            char* pid_string = command->commands[0][1];
            int pid = atoi(pid_string);

            safe_p_nice(pid, priority);        
        } 
        else if (strcmp(command->commands[0][0], "man") == 0) { // list all available commands.
            safe_f_print(MAN_COMMANDS);
        } 
        else if (strcmp(command->commands[0][0], "bg") == 0) { // continue the specified or last stopped job
            int target_job_id;
            if (command_argc == 1) { // continue last stopped job
                target_job_id = current_jobid;
            } else if (command_argc == 2) { // continue specified job
                target_job_id = atoi(command->commands[0][1]);
            } else { // too many args
                safe_f_print("too many args, expected 1-2\n");
                CONTINUE
            }

            job_t* target_job = job_find_by_jobid(background, target_job_id);
            if (target_job == NULL) {
                safe_f_print("specified job_id does not exist\n");
                CONTINUE
            }
            if (safe_p_kill(target_job->pid, S_SIGCONT) == -1) {
                safe_f_print("specified process does not exist\n");
                CONTINUE
            }
        } 
        else if (strcmp(command->commands[0][0], "fg") == 0) { // bring the specified or last stopped/bg job to fg
            int target_job_id;
            if (command_argc == 1) { // continue last stopped job
                target_job_id = current_jobid;
            } else if (command_argc == 2) { // continue specified job
                target_job_id = atoi(command->commands[0][1]);
            } else { // too many args
                safe_f_print("too many args, expected 1-2\n");
                CONTINUE
            }

            foreground_job = jobs_remove(background, target_job_id);
            if (foreground_job == NULL) { // no job found
                safe_f_print("no jobs to move to fg\n");
                CONTINUE
            } else { // continue stopped/bg job
                if (foreground_job->stop_order != NOT_STOPPED) foreground_job->stop_order = NOT_STOPPED;
                safe_f_print("continued "); job_print(foreground_job);

                safe_signal(SIGINT, term_handler);
                safe_signal(SIGTSTP, stop_handler);
                safe_p_kill(foreground_job->pid, S_SIGCONT);
                /* TODO: shell should surrender terminal control */
                int status;
                safe_p_waitpid(foreground_job->pid, &status, false);

                if (stop_trigger) {
                    foreground_job->stop_order = stop_order++; // new most recent stopped job
                    foreground_job->job_id = jobid_ctr++; // new most recently updated job
                    jobs_insert(background, foreground_job); // add as bg job
                    safe_p_kill(foreground_job->pid, S_SIGSTOP);
                    foreground_job = NULL;
                    stop_trigger = false;
                } else {
                    free(foreground_job);
                    foreground_job = NULL;
                }
            }
        } 
        else if (strcmp(command->commands[0][0], "jobs") == 0) { // list all jobs.
            job_t* current = *background;
            while (current != NULL) { // list jobs
                job_print(current);
                current = current->next;
            }
        } 
        else if (strcmp(command->commands[0][0], "logout") == 0) { // exit the shell and shutdown PennOS.
            // TODO free PCB

            // TODO free scheduler queues

            // free FAT
            f_unmount(&fat, fs_fd);
            
            // free jobs
            if (foreground_job != NULL) free(foreground_job);
            while (*background != NULL && jobid_ctr > 0) {
                jobid_ctr--;
                job_t* removed = jobs_remove(background, jobid_ctr);
                if (removed != NULL) free(removed);
            }

            // close log file
            fclose(logfile);
            printf("hi\n");
            p_exit();
        } 
        else { // independent PennOS process
            // shell_ps(0, NULL);

            const char* in_file = command->stdin_file;
            const char* out_file = command->stdout_file;
            bool append_mode = command->is_file_append;
            int pid = execute_command(command->commands[0], in_file, out_file, append_mode);

            cull_background();
            if (pid == -1) { // not a recognized command
                if (command_argc == 1) {
                    execute_script(command->commands[0], in_file, out_file, append_mode);
                }
                empty_reaped();
                CONTINUE
            }
            safe_signal(SIGINT, term_handler);
            safe_signal(SIGTSTP, stop_handler);

            // fprintf(stderr, "pid %d\n", pid);
            if (!command->is_background) { // create fg process
                jobs_push(&foreground_job, jobid_ctr++, pid, NOT_STOPPED);
                int status;
                safe_p_waitpid(pid, &status, false);
            } else { // add bg process
                jobs_push(background, jobid_ctr++, pid, stop_order++);
            }
            debug_print_jobs();

            if (stop_trigger) {
                foreground_job->stop_order = stop_order++; // new most recent stopped job
                foreground_job->job_id = jobid_ctr++; // new most recently updated job
                jobs_insert(background, foreground_job); // add as bg job
                safe_p_kill(foreground_job->pid, S_SIGSTOP);
                foreground_job = NULL;
                stop_trigger = false;
            } else {
                free(foreground_job);
                foreground_job = NULL;
            }

            // shell_ps(0, NULL);
        }
        empty_reaped();
        CONTINUE
    }
}