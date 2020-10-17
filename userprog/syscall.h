#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "../lib/user/syscall.h"

void syscall_init (void);

/* Projects 2 and later. */
static void syscall_halt (void) NO_RETURN;
static void syscall_exit (int) NO_RETURN;
static pid_t syscall_exec (const char *);
static int syscall_wait (pid_t);
static bool syscall_create (const char *, unsigned);
static bool syscall_remove (const char *);
static int syscall_open (const char *);
static int syscall_filesize (int);
static int syscall_read (int, void *, unsigned);
static int syscall_write (int, const void *, unsigned);
static void syscall_seek (int, unsigned);
static unsigned syscall_tell (int);
static void syscall_close (int);

/* Project 3 and optionally project 4. */
static mapid_t syscall_mmap (int, void *);
static void syscall_munmap (mapid_t);

/* Project 4 only. */
static bool syscall_chdir (const char *);
static bool syscall_mkdir (const char *);
static bool syscall_readdir (int, char[READDIR_MAX_LEN + 1]);
static bool syscall_isdir (int);
static int syscall_inumber (int);

/* System call wrappers. */
/* Projects 2 and later. */
static int syscall_halt_wrapper (struct intr_frame *);
static int syscall_exit_wrapper (struct intr_frame *);
static int syscall_exec_wrapper (struct intr_frame *);
static int syscall_wait_wrapper (struct intr_frame *);
static int syscall_create_wrapper (struct intr_frame *);
static int syscall_remove_wrapper (struct intr_frame *);
static int syscall_open_wrapper (struct intr_frame *);
static int syscall_filesize_wrapper (struct intr_frame *);
static int syscall_read_wrapper (struct intr_frame *);
static int syscall_write_wrapper (struct intr_frame *);
static int syscall_seek_wrapper (struct intr_frame *);
static int syscall_tell_wrapper (struct intr_frame *);
static int syscall_close_wrapper (struct intr_frame *);

/* Project 3 and optionally project 4. */
static int syscall_mmap_wrapper (struct intr_frame *);
static int syscall_munmap_wrapper (struct intr_frame *);

/* Project 4 only. */
static int syscall_chdir_wrapper (struct intr_frame *);
static int syscall_mkdir_wrapper (struct intr_frame *);
static int syscall_readdir_wrapper (struct intr_frame *);
static int syscall_isdir_wrapper (struct intr_frame *);
static int syscall_inumber_wrapper (struct intr_frame *);

#endif /* userprog/syscall.h */
