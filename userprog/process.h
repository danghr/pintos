#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define MAX_CMD_LENGTH 128
#define MAX_EXEC_NAME_LENGTH 15
#define MAX_ARGS 64

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void find_exec_name (const char *file_name, char *execute_name);
void find_args (char *file_name, int *argc, char *argv[]);

struct waiting_sema *create_waiting_sema (int);
struct waiting_sema *find_waiting_sema (struct list *, int);
#endif /* userprog/process.h */
