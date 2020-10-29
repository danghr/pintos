#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define MAX_CMD_LENGTH 100
#define MAX_EXEC_NAME_LENGTH 15
#define MAX_ARGS 50

/* States of a file that has been loaded to be run by a process. */
enum load_status
  {
    NOT_LOADED,         /* Initial loading state. */
    LOAD_SUCCESS,       /* The file was loaded successfully with no issues. */
    LOAD_FAILED         /* The file failed to load. */
  };


struct process
  {
    struct list_elem elem;
    struct list_elem child_elem;
            /* List element for child processes list. */
    int pid;                    /* The process/thread id. */
    int exit_status;              /* The exit status of the process. */
    enum load_status load_status; /* The load status of the file being executed by the process. */
    struct semaphore load_sema;
    struct semaphore wait_sema;
    
    bool is_exited;
    bool is_waited;        /* Used to wait for the file being executed to load (or fail to load). */
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void find_exec_name (const char *file_name, char *execute_name);
void find_args (char *file_name, int *argc, char *argv[]);

struct process* process_create (tid_t tid);
struct process* find_process_by_id(tid_t tid);
void update_load_status(struct thread* t, enum load_status status);
#endif /* userprog/process.h */
