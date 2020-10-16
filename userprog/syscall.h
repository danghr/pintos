#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "../lib/user/syscall.h"

void syscall_init (void);

/* Projects 2 and later. */
void syscall_halt (void) NO_RETURN;
void syscall_exit (int) NO_RETURN;
pid_t syscall_exec (const char *);
int syscall_wait (pid_t);
bool syscall_create (const char *, unsigned);
bool syscall_remove (const char *);
int syscall_open (const char *);
int syscall_filesize (int);
int syscall_read (int, void *, unsigned);
int syscall_write (int, const void *, unsigned);
void syscall_seek (int, unsigned);
unsigned syscall_tell (int);
void syscall_close (int);

/* Project 3 and optionally project 4. */
// mapid_t syscall_mmap (int, void *);
// void syscall_munmap (mapid_t);

/* Project 4 only. */
// bool syscall_chdir (const char *);
// bool syscall_mkdir (const char *);
// bool syscall_readdir (int, char[READDIR_MAX_LEN + 1]);
// bool syscall_isdir (int);
// int syscall_inumber (int);

#endif /* userprog/syscall.h */
