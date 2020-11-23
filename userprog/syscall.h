#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

#define READDIR_MAX_LEN 14

void syscall_init (void);
void terminate_program (int);
bool check_the_string(void *str);
#endif /* userprog/syscall.h */
