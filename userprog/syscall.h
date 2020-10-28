#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "../lib/user/syscall.h"

void syscall_init (void);
void terminate_program (int);
bool 
check_the_string(void *str);
#endif /* userprog/syscall.h */
