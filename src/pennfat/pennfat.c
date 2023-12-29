#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include "fat.h"
#include "safe.h"
#include "../util/parser.h"
#include "../util/util.h"

#define CONTINUE {free(command); continue;}

// return true if argc matches what was expected, print & return false otherwise
bool correct_argc(int expected, int actual) {
    if (expected != actual) {
        fprintf(stderr, "expected %d args, got %d instead\n", expected, actual);
        return false;
    }
    return true;
}

// return true if file system is mounted, print & return false otherwise
bool valid_fs_mounted(int fs_fd) {
    if (fs_fd == -1) {
        fprintf(stderr, "no file system mounted\n");
        return false;
    }
    return true;
}

// return true if all file arguments exist (between first_file_arg, last_file_arg)
// print & return false otherwise
// must call valid_fs_mounted() before this
bool all_files_exist(uint16_t* fat, int fs_fd, char* argv[], int first_file_arg, int last_file_arg) {
    for (int f = first_file_arg; f < last_file_arg; f++) {
        char* target = argv[f];
        point_t location;
        dir_entry_t entry;
        bool found = find_file(fat, fs_fd, ROOTDIR, target, &location, &entry);
        if (!found) {
            fprintf(stderr, "failed, file does not exist: %s\n", argv[f]);
            return false;
        }
    }
    return true;
}



int main(int argc, char* argv[]) {
    int fs_fd = -1; // filesystem file descriptor
    uint16_t* fat = NULL; // FAT
    uint16_t metadata = -1; // these values are only valid if fs_fd != -1
    int n_blocks = -1; // = FAT_BLOCKS(metadata);
    int block_size = -1; // = BLOCK_SIZE(metadata);

    // buffer for read
    char line[10001];
    int n_bytes = 0;
    struct parsed_command* command = NULL;

    // main loop
    while (1) {
        fprintf(stderr, "$ "); // prompt

        n_bytes = safe_read(STDIN_FILENO, line, 10000);
        line[n_bytes] = '\0';
        if (line[n_bytes -  1] != '\n') fprintf(stderr, "\n"); // newline upon ctrl-D

        int parse_command_res = parse_command(line, &command);
        if (parse_command_res < 0) {
            perror("parse_command");
            exit(EXIT_FAILURE);
        } else if (parse_command_res > 0) {
            if (fprintf(stderr, "invalid\n") < 0) {
                perror("fprintf");
                exit(EXIT_FAILURE);
            }
            CONTINUE
        }
        
        if (command->num_commands == 0) {
            CONTINUE
        }
        // print_parsed_command(command); // DEBUG: show command

        if (strcmp(command->commands[0][0], "mkfs") == 0) { // mkfs FS_NAME BLOCKS_IN_FAT BLOCK_SIZE_CONFIG
            int argc = get_argc(command->commands[0]);
            if (!correct_argc(4, argc)) CONTINUE
            char* dir = command->commands[0][1]; // FS_NAME
            int blocks_in_fat = atoi(command->commands[0][2]); // BLOCKS_IN_FAT
            if (blocks_in_fat < 1 || blocks_in_fat > 32) {
                fprintf(stderr, "invalid BLOCKS_IN_FAT:[%d] (must be within 1-32)\n", blocks_in_fat);
                CONTINUE
            }
            int block_size_config = atoi(command->commands[0][3]); // BLOCK_SIZE_CONFIG
            if (block_size_config < 0 || block_size_config > 4) {
                fprintf(stderr, "invalid BLOCK_SIZE_CONFIG:[%d] (must be within 0-4)\n", block_size_config);
                CONTINUE
            }
            int block_size_bytes = BYTE_SIZE << block_size_config;

            int fat_size = blocks_in_fat * block_size_bytes;
            int n_fat_entries = fat_size / 2;
            int data_region_size = block_size_bytes * (n_fat_entries - 1);
            
            // fprintf(stderr, "mkfs %s %d %d\n", dir, blocks_in_fat, block_size_bytes); // DEBUG: show parsed mkfs

            int fd = safe_open(dir, O_CREAT|O_TRUNC|O_RDWR, DEFAULT_PERMISSIONS);

            // write the FAT region
            uint16_t* fat_region = safe_malloc(fat_size);
            for (int j = 0; j < n_fat_entries; j++) {
                fat_region[j] = 0;
            }
            fat_region[0] = (blocks_in_fat << BITS_PER_BYTE) | block_size_config; // metadata
            fat_region[1] = LASTBLOCK;
            safe_write(fd, fat_region, fat_size);
            free(fat_region);

            // write the data region
            char* block = safe_malloc(block_size_bytes);
            for (int j = 0; j < block_size_bytes; j++) {
                block[j] = '\0';
            }
            for (int i = 0; i < data_region_size / block_size_bytes; i++) {
                safe_write(fd, block, block_size_bytes);
            }

            free(block);

            safe_close(fd);
        } else if (strcmp(command->commands[0][0], "mount") == 0) { // mount FS_NAME
            int argc = get_argc(command->commands[0]);
            if (!correct_argc(2, argc)) CONTINUE

            char* dir = command->commands[0][1]; // FS_NAME
            fs_fd = fs_mount(dir, &fat);
            safe_lseek(fs_fd, 0, SEEK_SET);
            safe_read(fs_fd, &metadata, 2);
            fs_getmeta(fat, fs_fd, &n_blocks, &block_size);
        } else if (strcmp(command->commands[0][0], "unmount") == 0) { // umount
            int argc = get_argc(command->commands[0]);
            if (!correct_argc(1, argc)) CONTINUE
            if (!valid_fs_mounted(fs_fd)) CONTINUE

            fs_unmount(&fat, fs_fd);
            fs_fd = -1;
        } else if (strcmp(command->commands[0][0], "touch") == 0) { // touch FILE ...
            int argc = get_argc(command->commands[0]);
            if (!valid_fs_mounted(fs_fd)) CONTINUE

            for (int f = 1; f < argc; f++) {
                char* target = command->commands[0][f];
                if (!valid_filename(target)) CONTINUE

                fs_touch(fat, fs_fd, target);
            }
        } else if (strcmp(command->commands[0][0], "mv") == 0) { // mv SOURCE DEST
            int argc = get_argc(command->commands[0]);
            if (!correct_argc(3, argc)) CONTINUE
            if (!valid_fs_mounted(fs_fd)) CONTINUE

            char* old_name = command->commands[0][1]; // SOURCE
            char* new_name = command->commands[0][2]; // DEST
            if (!all_files_exist(fat, fs_fd, command->commands[0], 1, 2)) CONTINUE // check SOURCE
            if (find_file(fat, fs_fd, ROOTDIR, new_name, NULL, NULL)) { // new_name already exists
                fprintf(stderr, "DEST name already exists\n");
                CONTINUE
            }
            if (!valid_filename(new_name)) CONTINUE

            fs_mv(fat, fs_fd, old_name, new_name);
        } else if (strcmp(command->commands[0][0], "rm") == 0) { // rm FILE ...
            int argc = get_argc(command->commands[0]);
            if (!valid_fs_mounted(fs_fd)) CONTINUE
            if (!all_files_exist(fat, fs_fd, command->commands[0], 1, argc)) CONTINUE

            for (int f = 1; f < argc; f++) {
                char* target = command->commands[0][f];
                
                fs_rm(fat, fs_fd, target);
            }
        } else if (strcmp(command->commands[0][0], "cat") == 0) { // cat [FILE ...] [ {-w -a} OUTPUT_FILE ]
            int argc = get_argc(command->commands[0]);
            if (!valid_fs_mounted(fs_fd)) CONTINUE

            int input_mode = 0; // 0 for stdin, 1 for files
            int output_mode = 0; // 0 for stdout, 1 for file write, 2 for file append

            if (strcmp(command->commands[0][1], "-w") == 0) { // terminal -> overwrite
                input_mode = 0;
                output_mode = 1;
            } else if (strcmp(command->commands[0][1], "-a") == 0) { // terminal -> append
                input_mode = 0;
                output_mode = 2;
            } else { // input from file
                if (argc >= 2 && strcmp(command->commands[0][argc - 2], "-w") == 0) { // files -> overwrite
                    input_mode = 1;
                    output_mode = 1;
                } else if (argc >= 2 && strcmp(command->commands[0][argc - 2], "-a") == 0) { // files -> append
                    input_mode = 1;
                    output_mode = 2;
                } else { // files -> terminal
                    input_mode = 1;
                    output_mode = 0;
                }
            }
            // check if files to be concatenated exist
            if (output_mode == 0) { // no -w/-a arg, check all files
                if (!all_files_exist(fat, fs_fd, command->commands[0], 1, argc)) CONTINUE
            } else { // -w/-a and OUTPUT arg exist, check until last 2 args
                if (!all_files_exist(fat, fs_fd, command->commands[0], 1, argc-2)) CONTINUE
                if (!valid_filename(command->commands[0][argc-1])) CONTINUE
            }

            if (input_mode == 0) { // repeatedly prompt until ctrl+D
                while (1) {
                    char input[10001];
                    int input_size = safe_read(STDIN_FILENO, input, 10000);
                    if (input_size <= 0) break;
                    input[input_size] = '\0';

                    char* output = fs_cat(fat, fs_fd, input_mode, output_mode, input, NULL, command->commands[0][argc-1]);
                    if (output_mode == 0) fprintf(stderr, "%s\n", output);
                    free(output);
                    if (output_mode == 1) output_mode = 2; // append after first overwrite
                }
            } else {
                int lastfile_arg = (output_mode == 0) ? argc : argc - 2;
                int n_files = lastfile_arg-1;
                char** input_files = (char**) safe_malloc(n_files * sizeof(char*));
                int idx = 0;
                for (int i = 1; i < lastfile_arg; i++) { // copy list of input files
                    input_files[i-1] = (char*) safe_malloc(32 * sizeof(char));
                    strcpy(input_files[idx], command->commands[0][i]);
                    idx++;
                }
                char* output = fs_cat(fat, fs_fd, lastfile_arg-1, output_mode, NULL, input_files, command->commands[0][argc-1]);
                if (output_mode == 0) fprintf(stderr, "%s\n", output);
                free(output);

                for (int i = 0; i < idx; i++) {
                    free(input_files[i]);
                }
                free(input_files);
            }
        } else if (strcmp(command->commands[0][0], "cp") == 0) { // cp [ -h ] SOURCE DEST
            int argc = get_argc(command->commands[0]);
            if (argc < 3 || argc > 4) {
                fprintf(stderr, "expected 3-4 args, got %d instead\n", argc);
                CONTINUE
            }
            if (!valid_fs_mounted(fs_fd)) CONTINUE

            if (strcmp(command->commands[0][1], "-h") == 0) { // host OS -> PennFAT
                char* source = command->commands[0][2];
                char* dest = command->commands[0][3];
                if (!valid_filename(dest)) CONTINUE // check DEST
                
                fs_cp_mode(fat, fs_fd, source, dest, true, false);
            } else if (strcmp(command->commands[0][2], "-h") == 0) { // PennFAT -> host OS
                char* source = command->commands[0][1];
                char* dest = command->commands[0][3];
                if (!all_files_exist(fat, fs_fd, command->commands[0], 1, 2)) CONTINUE // check SOURCE

                fs_cp_mode(fat, fs_fd, source, dest, false, true);
            } else { // PennFAT -> PennFat
                char* source = command->commands[0][1];
                char* dest = command->commands[0][2];
                if (!all_files_exist(fat, fs_fd, command->commands[0], 1, 2)) CONTINUE // check SOURCE
                if (!valid_filename(dest)) CONTINUE // check DEST

                fs_cp(fat, fs_fd, source, dest);
            }
        } else if (strcmp(command->commands[0][0], "ls") == 0) { // ls
            int argc = get_argc(command->commands[0]);
            if (!correct_argc(1, argc)) CONTINUE
            if (!valid_fs_mounted(fs_fd)) CONTINUE

            fs_ls(fat, fs_fd);
        } else if (strcmp(command->commands[0][0], "chmod") == 0) { // chmod PERMISSIONS FILE ...
            int argc = get_argc(command->commands[0]);
            if (!valid_fs_mounted(fs_fd)) CONTINUE
            if (!all_files_exist(fat, fs_fd, command->commands[0], 2, argc)) CONTINUE

            char* perm_arg = command->commands[0][1];
            uint8_t permissions = (uint8_t) atoi(perm_arg); // PERMISSIONS
            if (permissions == 0) {
                if (strcmp(perm_arg, "0") == 0) permissions = FILEPERM_NONE;
                else if (strcmp(perm_arg, "---") == 0) permissions = FILEPERM_NONE;
                else if (strcmp(perm_arg, "-w-") == 0) permissions = FILEPERM_WR;
                else if (strcmp(perm_arg, "r--") == 0) permissions = FILEPERM_RD;
                else if (strcmp(perm_arg, "r-x") == 0) permissions = FILEPERM_RD|FILEPERM_EX;
                else if (strcmp(perm_arg, "rw-") == 0) permissions = FILEPERM_RD|FILEPERM_WR;
                else if (strcmp(perm_arg, "rwx") == 0) permissions = FILEPERM_RD|FILEPERM_WR|FILEPERM_EX;
                else {
                    fprintf(stderr, "invalid PERMISSIONS:[%s] (must be r/w/x with - representing restriction)\n", perm_arg);
                    CONTINUE
                }
            }
            if (permissions < 0 || permissions > 7 || permissions == 1 || permissions == 3) {
                fprintf(stderr, "invalid PERMISSIONS:[%d] (must be {0,2,4,5,6,7})\n", permissions);
                CONTINUE
            }

            for (int f = 2; f < argc; f++) {
                char* target = command->commands[0][f];
                fs_chmod(fat, fs_fd, target, permissions);
            }
        } else if (strcmp(command->commands[0][0], "hd") == 0) { // hd [ -c ] [ -n BYTES ] [-b]
            int argc = get_argc(command->commands[0]);
            if (argc > 5) {
                fprintf(stderr, "expected 1-5 args, got %d instead\n", argc);
                CONTINUE
            }
            if (!valid_fs_mounted(fs_fd)) CONTINUE

            bool display_chars = false; // display ascii if true, otherwise just hex
            int display_bytes = -1; // -1 to display all, otherwise display first n bytes
            bool display_blocks = false; // display blocks next to address if true
            bool invalid_args = false;
            // parse arguments
            for (int i = 1; i < argc; i++) {
                if (strcmp(command->commands[0][i], "-c") == 0) {
                    display_chars = true;
                } else if (strcmp(command->commands[0][i], "-n") == 0) {
                    if (i+1 < argc) {
                        display_bytes = atoi(command->commands[0][i+1]);
                        i++;
                    } 
                    if (display_bytes <= 0) {
                        fprintf(stderr, "failed: -n requires a number of bytes\n");
                        invalid_args = true;
                    }
                } else if (strcmp(command->commands[0][i], "-b") == 0) {
                    display_blocks = true;
                } else {
                    fprintf(stderr, "failed: unknown option:[%s]\n", argv[i]);
                    invalid_args = true;
                }
            }
            if (invalid_args) CONTINUE
            // fprintf(stderr, "char:[%d] bytes:[%d]\n", display_chars, display_bytes); // DEBUG: show parsed args
            
            if (display_bytes == -1) display_bytes = safe_lseek(fs_fd, 0, SEEK_END); // read entire FS
            char* buffer = safe_malloc(display_bytes);
            safe_lseek(fs_fd, 0, SEEK_SET); // read from start
            ssize_t bytes_read = safe_read(fs_fd, buffer, display_bytes);

            for (size_t i = 0; i < bytes_read; i++) {
                if (i % 16 == 0) {
                    if (display_blocks) fprintf(stderr, "%08zx blk%04x:  ", i, (int) i/block_size);
                    else fprintf(stderr, "%08zx:  ", i);
                }
                fprintf(stderr, "%02x ", (unsigned char) buffer[i]);
                if (i % 8 == 7) fprintf(stderr, " ");
                if (i % 16 == 15) {
                    if (display_chars) {
                        fprintf(stderr, "|");
                        for (int j = i - 15; j <= i; j++) {
                            if (buffer[j] >= 32 && buffer[j] <= 126) {
                                fprintf(stderr, "%c", buffer[j]);
                            } else {
                                fprintf(stderr, ".");
                            }
                        }
                        fprintf(stderr, "|");
                    }
                    fprintf(stderr, "\n");
                }
            }
            int bytes_remaining = bytes_read % 16;
            if (bytes_remaining != 0) { // last line
                int spaces = 3 * (16 - bytes_remaining) + 1;
                if (display_chars) {
                    fprintf(stderr, "%*s|", spaces, "");
                    for (size_t i = bytes_read - bytes_remaining; i < bytes_read; i++) {
                        if (buffer[i] >= 32 && buffer[i] <= 126) {
                            fprintf(stderr, "%c", buffer[i]);
                        } else {
                            fprintf(stderr, ".");
                        }
                    }
                    fprintf(stderr, "|");
                }
                fprintf(stderr, "%*s", spaces, "");
            }
            fprintf(stderr, "\n");

            free(buffer);
        } else {

        }
        free(command);
    }

    fprintf(stderr, "%ld\n", (long)fat);
}