#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

/* Interrupt handler functions */
static int *syscall_handlers[20];

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* System call handler
   Retrieve the system call number, then any system call 
   arguments, and carry out appropriate actions. */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

/* *** IMPORTANT ***

   Remember to validate memory access when implementing system
   calls.  If the user program requires a wrong memory address,
   terminate the process and free its resources.

   Might be checked by using pagedir_get_page() in
   "userprog/pagedir.c", which returns NULL if the address is
   unmapped. 

   Also, all the arguments passed to system calls should be 
   checked, and with an illegal argument, we can returning an 
   error value (for those calls that return a value), returning 
   an undefined value, or terminating the process.
   */

/* Projects 2 and later. */

/* Terminates Pintos */
static void
syscall_halt (void)
{
  shutdown_power_off (); 
}

/* Terminates the current user program, returning STATUS to the 
   kernel. If the process's parent "waits" for it (see below), 
   this is the status that will be returned. */
static void
syscall_exit (int status)
{
   thread_current ()->exit_status = status;
   thread_exit ();
}

/* Runs the executable whose name is given in CMD_LINE, passing 
   any given arguments, and returns the new process's program id 
   (pid). */
static pid_t
syscall_exec (const char *cmd_line)
{

}

/* Waits for a child process PID and retrieves the child's exit
   status. */
static int
syscall_wait (pid_t pid)
{

}

/* *** IMPORTANT*** 
   For the following system calls related to file operations, 
   limitations of the file system should be considered. See 
   section 3.1.2 of the document. 
   */

/* Creates a new file called FILE initially initial_size bytes 
   in size. 
   Returns true if successful, false otherwise. */
static bool
syscall_create (const char *file, unsigned initial_size)
{

}

/* Deletes the file called FILE. 
   Returns true if successful, false otherwise. */
static bool
syscall_remove (const char *file)
{

}

/* Opens the file called FILE. 
   Returns a nonnegative integer handle called a "file descriptor"
   (fd), or -1 if the file could not be opened. */
static int
syscall_open (const char *file)
{

}

/* Returns the size, in bytes, of the file open as FD. */
static int
syscall_filesize (int fd)
{

}

/* Reads size bytes from the file open as FD into buffer. 
   Returns the number of bytes actually read (0 at end of file), or 
   -1 if the file could not be read (due to a condition other than 
   end of file). */
static int
syscall_read (int fd, void *buffer, unsigned length)
{

}

/* Writes size bytes from buffer to the open file FD. 
   Returns the number of bytes actually written, which may be less 
   than size if some bytes could not be written. */
static int
syscall_write (int fd, const void *buffer, unsigned length)
{

}

/* Changes the next byte to be read or written in open file FD to 
   POSITION, expressed in bytes from the beginning of the file. 
   (Thus, a position of 0 is the file's start.) */
static void
syscall_seek (int fd, unsigned position)
{

}

/* Returns the position of the next byte to be read or written in 
   open file FD, expressed in bytes from the beginning of the 
   file. */
static unsigned
syscall_tell (int fd)
{

}

/* Closes file descriptor FD. Exiting or terminating a process 
   implicitly closes all its open file descriptors, as if by calling 
   this function for each one. */
static void
syscall_close (int fd)
{

}
