#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "devices/intq.h"
#include "devices/input.h"

/* Lock of filesys access */
static struct lock *filesys_lock;

static void syscall_handler (struct intr_frame *);

/* Interrupt handler wrapper functions */
static int (*syscall_handler_wrapper[20]) (struct intr_frame *);

/* Projects 2 and later. */
void syscall_halt (void);
void syscall_exit (int);
pid_t syscall_exec (const char *);
int syscall_wait (pid_t);
bool syscall_create (const char *, unsigned);
bool syscall_remove (const char *);
int syscall_open (const char *);
int syscall_filesize (int);
int syscall_read (int, void *, unsigned);
int syscall_write (int, const void *, unsigned);
/* The following three functions are changed to int
   so that wrappers can detect whether there is a 
   fault so as to terminate the process */
int syscall_seek (int, unsigned);
int syscall_tell (int);
int syscall_close (int);

/* Project 3 and optionally project 4. */
mapid_t syscall_mmap (int, void *);
void syscall_munmap (mapid_t);

/* Project 4 only. */
bool syscall_chdir (const char *);
bool syscall_mkdir (const char *);
bool syscall_readdir (int, char[READDIR_MAX_LEN + 1]);
bool syscall_isdir (int);
int syscall_inumber (int);

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

/* File descriptor entry point */
struct fd_entry
{
  struct file *file;
  int fd;
  struct list_elem elem;
};

/* Allocate new file descriptor in the thread */
static int
allocate_fd (void)
{
  return (thread_current ()->next_fd)++;
}

/* Get the entry point in current thread according to fd 
   Returns the address of the fd_entry if found, 
   NULL if not found */
static struct fd_entry *
get_fd_entry (int fd)
{
  struct list *fd_list = &(thread_current ()->opened_files);
  for (struct list_elem *e = list_begin (fd_list); 
    e != list_end (fd_list); e = e->next)
    {
      if (list_entry (e, struct fd_entry, elem)->fd == fd)
        return list_entry (e, struct fd_entry, elem);
    }
  return NULL;
}

/* Helper functions */

/* Kill the program which is violating the system */
void
terminate_program (int exit_status)
{
  thread_current ()->exit_status = exit_status;
  thread_exit ();
}

/* Verify that a memory address is valid in user program */
static bool
is_valid_addr (const void *uaddr)
{
  if (!is_user_vaddr (uaddr))
    return false;
  if (pagedir_get_page (thread_current ()->pagedir, uaddr) 
    == NULL)
    return false;
  return true;
}

/* Check the validation of the string */
bool 
check_the_string (void *str){
  while (is_valid_addr (str) && (*(char*)str != '\0'))
    (char*)str++;
  
  if (is_valid_addr (str) && (*(char*)str == '\0'))
    return true;
  else 
    return false;
}

/* Initialize the systemcall handlers */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_handler_wrapper[SYS_HALT] = &syscall_halt_wrapper;
  syscall_handler_wrapper[SYS_EXIT] = &syscall_exit_wrapper;
  syscall_handler_wrapper[SYS_EXEC] = &syscall_exec_wrapper;
  syscall_handler_wrapper[SYS_WAIT] = &syscall_wait_wrapper;
  syscall_handler_wrapper[SYS_CREATE] = &syscall_create_wrapper;
  syscall_handler_wrapper[SYS_REMOVE] = &syscall_remove_wrapper;
  syscall_handler_wrapper[SYS_OPEN] = &syscall_open_wrapper;
  syscall_handler_wrapper[SYS_FILESIZE] = &syscall_filesize_wrapper;
  syscall_handler_wrapper[SYS_READ] = &syscall_read_wrapper;
  syscall_handler_wrapper[SYS_WRITE] = &syscall_write_wrapper;
  syscall_handler_wrapper[SYS_SEEK] = &syscall_seek_wrapper;
  syscall_handler_wrapper[SYS_TELL] = &syscall_tell_wrapper;
  syscall_handler_wrapper[SYS_CLOSE] = &syscall_close_wrapper;
  syscall_handler_wrapper[SYS_MMAP] = &syscall_mmap_wrapper;
  syscall_handler_wrapper[SYS_MUNMAP] = &syscall_munmap_wrapper;
  syscall_handler_wrapper[SYS_CHDIR] = &syscall_chdir_wrapper;
  syscall_handler_wrapper[SYS_MKDIR] = &syscall_mkdir_wrapper;
  syscall_handler_wrapper[SYS_READDIR] = &syscall_readdir_wrapper;
  syscall_handler_wrapper[SYS_ISDIR] = &syscall_isdir_wrapper;
  syscall_handler_wrapper[SYS_INUMBER] = &syscall_inumber_wrapper;
  filesys_lock = malloc (sizeof (struct lock));
  lock_init (filesys_lock);
}

/* System call handler.
   Retrieve the system call number and send it to correct
   wrappers. */
static void
syscall_handler (struct intr_frame *f) 
{
  if (!is_valid_addr (f->esp) || !is_valid_addr (f->esp + 4))
    terminate_program (-1);
  /* System call number is saved in stack pointer (f->esp).
     See section 3.5.2 in the doc for details. */
  int syscall_num = *(int*)(f->esp);
  int wrapper_return;
  /* Check whether correct syscall num is correct */
  if (syscall_num < 0 || syscall_num >= 20)
      terminate_program (-1);
  
  /* Send it to correct wrapper to decode. 
     If the wrapper returns a non-zero status, then we can 
     detect that the user program is violating the system 
     (memory access, etc.), so we need to terminate the 
     program */
  wrapper_return = syscall_handler_wrapper[syscall_num] (f);
  if (wrapper_return != 0)
    terminate_program (-1);
}

/* System calls. */

/* *** IMPORTANT ***

   Remember to validate memory access when implementing system
   calls.  If the user program requires a wrong memory address,
   terminate the process and free its resources.

   If 
     pagedir_get_page (get_pagedir (), PTR) == NULL
   then it means that PTR is not valid. 

   Also, all the arguments passed to system calls should be 
   checked, and with an illegal argument, we can returning an 
   error value (for those calls that return a value), returning 
   an undefined value, or terminating the process. */

/* Projects 2 and later. */

/* Terminates Pintos */
void
syscall_halt (void)
{
  shutdown_power_off (); 
}

/* Terminates the current user program, returning STATUS to the 
   kernel. If the process's parent "waits" for it (see below), 
   this is the status that will be returned. */
void
syscall_exit (int status)
{

  terminate_program (status);
}

/* Runs the executable whose name is given in CMD_LINE, passing 
   any given arguments, and returns the new process's program id 
   (pid). */
pid_t
syscall_exec (const char *cmd_line)
{
  return process_execute (cmd_line);
}

/* Waits for a child process PID and retrieves the child's exit
   status. */
int
syscall_wait (pid_t pid)
{
  return process_wait (pid);
}

/* *** IMPORTANT*** 
   For the following system calls related to file operations, 
   limitations of the file system should be considered. See 
   section 3.1.2 of the document. 
   */

/* Creates a new file called FILE initially initial_size bytes 
   in size. 
   Returns true if successful, false otherwise. */
bool
syscall_create (const char *file, unsigned initial_size)
{
  lock_acquire (filesys_lock);
  bool ret = filesys_create (file, initial_size);
  lock_release (filesys_lock);
  return ret;
}

/* Deletes the file called FILE. 
   Returns true if successful, false otherwise. */
bool
syscall_remove (const char *file)
{
  lock_acquire (filesys_lock);
  bool ret = filesys_remove (file);
  lock_release (filesys_lock);
  return ret;
}

/* Opens the file called FILE. 
   Returns a nonnegative integer handle called a "file descriptor"
   (fd), or -1 if the file could not be opened. */
int
syscall_open (const char *file)
{
  struct file *to_open;
  struct fd_entry *opened_file;

  lock_acquire (filesys_lock);
  to_open = filesys_open (file);
  lock_release (filesys_lock);

  /* If the opening process fails, straightly return -1 */
  if(to_open == NULL)
    return -1;
  
  /* Allocate memory of the fd_entry after the file is opened to 
     avoid memory leak */
  opened_file = malloc (sizeof (struct fd_entry));
  opened_file->file = to_open;
  opened_file->fd = allocate_fd (); /* Allocate file descriptor */

  /* Push into the list of opened files by current thread */
  list_push_back (&(thread_current ()->opened_files), 
    &(opened_file->elem));
  /* Return the file descriptor */
  return opened_file->fd;
}

/* Returns the size, in bytes, of the file open as FD. */
int
syscall_filesize (int fd)
{
  int ret;
  struct fd_entry *fd_e = get_fd_entry (fd);
  if (fd_e == NULL)
    return -1;
  lock_acquire (filesys_lock);
  ret = file_length (fd_e->file);
  lock_release (filesys_lock);
  return ret;
}

/* Reads size bytes from the file open as FD into buffer. 
   Returns the number of bytes actually read (0 at end of file), or 
   -1 if the file could not be read (due to a condition other than 
   end of file). */
int
syscall_read (int fd, void *buffer, unsigned length)
{
  if (fd == STDIN_FILENO)
    {
      /* May need a lot of modification */
      uint8_t input_char;
      int cnt = 0;
      for (unsigned i = 0; i < length; i++)
        {
          input_char = input_getc ();
          intq_putc (buffer, input_char);
          cnt++;
        }
      return cnt;
    }
  struct fd_entry *fd_e = get_fd_entry (fd);
  int ret;
  if (fd_e == NULL)
    return -1;
  lock_acquire (filesys_lock);
  ret = file_read (fd_e->file, buffer, length);
  lock_release (filesys_lock);
  return ret;
}

/* Writes size bytes from buffer to the open file FD. 
   Returns the number of bytes actually written, which may be less 
   than size if some bytes could not be written. */
int
syscall_write (int fd, const void *buffer, unsigned length)
{
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, (size_t)length);
      return (int)length;
    }
  struct fd_entry *fd_e = get_fd_entry (fd);
  int ret;
  if (fd_e == NULL)
    return -1;
  lock_acquire (filesys_lock);
  ret = file_write (fd_e->file, buffer, length);
  lock_release (filesys_lock);
  return ret;
}

/* Changes the next byte to be read or written in open file FD to 
   POSITION, expressed in bytes from the beginning of the file. 
   (Thus, a position of 0 is the file's start.) */
int
syscall_seek (int fd, unsigned position)
{
  struct fd_entry *fd_e = get_fd_entry (fd);
  if (fd_e == NULL)
    return -1;
  lock_acquire (filesys_lock);
  file_seek (fd_e->file, position);
  lock_release (filesys_lock);
  return 0;
}

/* Returns the position of the next byte to be read or written in 
   open file FD, expressed in bytes from the beginning of the 
   file. */
int
syscall_tell (int fd)
{
  unsigned ret;
  struct fd_entry *fd_e = get_fd_entry (fd);
  if (fd_e == NULL)
    return -1;
  lock_acquire (filesys_lock);
  ret = file_tell (fd_e->file);
  lock_release (filesys_lock);
  return ret;
}

/* Closes file descriptor FD. Exiting or terminating a process 
   implicitly closes all its open file descriptors, as if by calling 
   this function for each one. */
int
syscall_close (int fd)
{
  struct fd_entry *fd_e = get_fd_entry (fd);
  if (fd_e == NULL)
    return -1;
  lock_acquire (filesys_lock);
  file_close (fd_e->file);
  lock_release (filesys_lock);

  list_remove (&(fd_e->elem));
  free (fd_e);
  return 0;
}

/* System call wrappers.
   Retrive correct argument from the stack and send it to call 
   functions. 

   Returns 0 if operation is correct, otherwise returns -1. 
   
   Also, return value of system calls should be saved in f->eax.
   See section 3.5.2 in the doc for details. */

/* Projects 2 and later. */

static int
syscall_halt_wrapper (struct intr_frame *f UNUSED)
{
  syscall_halt ();
  return 0;
}

static int
syscall_exit_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  int status = *(int*)(f->esp + 4);

  /* Write the return value */
  syscall_exit (status);
  return 0;
}

static int
syscall_exec_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  char *cmd_line = *(char**)(f->esp + 4);

  if (cmd_line == NULL || !check_the_string (cmd_line))
    {
      terminate_program (-1);
      return -1;
    }

  /* Write the return value */
  f->eax = syscall_exec (cmd_line);
  return 0;
}

static int
syscall_wait_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  pid_t pid = *(pid_t*)(f->esp + 4);

  /* Write the return value */
  f->eax = syscall_wait (pid);
  return 0;
}

static int
syscall_create_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  for (int i = 1; i <= 3; i++)
    if (!is_valid_addr (f->esp + i * 4))
      return -1;

  /* Decode parameters */
  char *file = *(char**)(f->esp + 4);
  unsigned initial_size = *(unsigned*)(f->esp + 8);

  if (file == NULL || !check_the_string (file))
    {
      terminate_program (-1);
      return -1;
    }
  
  /* Write the return value */
  f->eax = syscall_create (file, initial_size);
  return 0;
}

static int
syscall_remove_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  char *file = *(char**)(f->esp + 4);

  if (file == NULL || !check_the_string (file))
    {
      terminate_program (-1);
      return -1;
    }

  /* Write the return value */
  f->eax = syscall_remove (file);
  return 0;
}

static int
syscall_open_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  char *file = *(char**)(f->esp + 4);
  if (file == NULL || !check_the_string (file))
    {
      terminate_program (-1);
      return -1;
    }

  /* Write the return value */
  f->eax = syscall_open (file);
  return 0;
}

static int
syscall_filesize_wrapper (struct intr_frame *f)
{

  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  int fd = *(int*)(f->esp + 4);

  /* Write the return value */
  int ret = syscall_filesize (fd);
  if (ret == -1)
    return -1;
  f->eax = ret;
  return 0;
}

static int
syscall_read_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  for (int i = 1; i <= 4; i++)
    if (!is_valid_addr (f->esp + i * 4))
      return -1;
  
  /* Decode parameters */
  int fd = *(int*)(f->esp + 4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned length = *(unsigned*)(f->esp + 12);

  if (buffer == NULL || !is_valid_addr (buffer) ||
    !is_valid_addr (buffer + length))
    {
      terminate_program (-1);
      return -1;
    }

  /* Write the return value */
  f->eax = syscall_read (fd, buffer, length);
  return 0;
}

static int
syscall_write_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  for (int i = 1; i <= 4; i++)
    if (!is_valid_addr (f->esp + 4 * i))
      return -1;
  
  /* Decode parameters */
  int fd = *((int*)(f->esp + 4));
  void *buffer = *(char**)(f->esp + 8);
  unsigned length = *((unsigned*)(f->esp + 12));

  if (buffer == NULL || !is_valid_addr (buffer) || 
    !is_valid_addr((char *)buffer+length))
    {
      terminate_program (-1);
      return -1;
    }

  /* Write the return value */
  f->eax = syscall_write (fd, buffer, length);
  return 0;
}

static int
syscall_seek_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  for (int i = 1; i <= 3; i++)
    if (!is_valid_addr (f->esp + 4 * i))
      return -1;
  
  /* Decode parameters */
  int fd = *(int*)(f->esp + 4);
  unsigned position = *(unsigned*)(f->esp + 8);

  /* Execute the function */
  return syscall_seek (fd, position);
}

static int
syscall_tell_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  int fd = *(int*)(f->esp + 4);

  /* Execute the function and write the return value */
  int ret = syscall_tell (fd);
  if (ret == -1)
    return -1;
  f->eax = ret;
  return 0;
}

static int
syscall_close_wrapper (struct intr_frame *f)
{
  /* Validate memory address */
  if (!is_valid_addr (f->esp + 8))
    return -1;
  
  /* Decode parameters */
  int fd = *(int*)(f->esp + 4);

  /* Execute the function */
  return syscall_close (fd);
}

/* Project 3 and optionally project 4. */

static int
syscall_mmap_wrapper (struct intr_frame *f UNUSED)
{
  return -1;
}

static int
syscall_munmap_wrapper (struct intr_frame *f UNUSED)
{
  return -1;
}

/* Project 4 only. */

static int
syscall_chdir_wrapper (struct intr_frame *f UNUSED)
{
  return -1;
}

static int
syscall_mkdir_wrapper (struct intr_frame *f UNUSED)
{
  return -1;
}

static int
syscall_readdir_wrapper (struct intr_frame *f UNUSED)
{
  return -1;
}

static int
syscall_isdir_wrapper (struct intr_frame *f UNUSED)
{
  return -1;
}

static int
syscall_inumber_wrapper (struct intr_frame *f UNUSED)
{
  return -1;
}
