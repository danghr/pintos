#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdio.h>
#include <stdbool.h>
#include <list.h>

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

#define READDIR_MAX_LEN 14

/* Mapped file entry point */
struct mapid_entry
{
  struct file *file;
  size_t file_length;
  mapid_t mapid;
  void *user_vaddr;
  bool freed;
  struct list_elem elem;
};

void syscall_init (void);
void terminate_program (int);
bool check_the_string(void *str);
#endif /* userprog/syscall.h */
