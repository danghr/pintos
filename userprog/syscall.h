#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "../lib/user/syscall.h"

void syscall_init (void);
void terminate_program (int);
void *find_stack (struct intr_frame *);
uint32_t *get_pagedir ();

#endif /* userprog/syscall.h */
