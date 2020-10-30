#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <list.h>
#include "threads/synch.h"
#include "threads/thread.h"

#define MAX_CMD_LENGTH 100
#define MAX_EXEC_NAME_LENGTH 15
#define MAX_ARGS 50

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void find_exec_name (const char *file_name, char *execute_name);
void find_args (char *file_name, int *argc, char *argv[]);

struct filename_lock
{
  char *filename;
  struct lock lock;
  struct list_elem elem;
};
#endif /* userprog/process.h */
