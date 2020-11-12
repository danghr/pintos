#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "fixed-point.h"
#include "synch.h"
/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* Nice priorities. */
#define NICE_MIN -20                    /* Lowest "nice" value. */
#define NICE_DEFAULT 0                  /* Default "nice" value. */
#define NICE_MAX 20                     /* Highest "nice" value. */

#define MAX_THREADS 300                 /* Max number of threads */

/* List element of waiting child */
struct waiting_sema
{
  struct list_elem elem;
  int child_tid;
  struct semaphore sema;
};

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads
                                           list. */
    int64_t sleeping_ticks;             /* A counter of remaining sleeping 
                                           ticks. */
   
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    int exit_status;                    /* Exit status of the thread */
    int next_fd;                        /* Next file descriptor */
    struct list opened_files;           /* List of opened files */
    struct file *executing_file;        /* Pointer to the executable of the 
                                           current thread */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */

   /* Variables used in priority donation. */
    struct list locks;                  /* List of holding locks. */
    int priority_wo_donation;           /* Priority without donation. */
    struct lock *lock_wait;             /* The lock a thread is waiting 
                                           for. */

   /* Variables used in advanced schedular. */
    int nice;                           /* "nice" value of a thread. */
    fixed_point recent_cpu;             /* "recent_cpu" value of a thread. */

    /* List used to store its child threads */
    struct list child_threads_list;
    struct list_elem child_elem;

    struct list waiting;
    bool is_exited;
    bool is_waited;
    bool is_waiting;
    struct thread* parent_thread;

    /* List used to store supplemental page tables */
    struct list sup_page_table;
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/* File operation lock */
struct lock file_lock;

void thread_init (void);
void thread_start (void);

void thread_sleep_monitor (struct thread *t, void *aux);
void thread_tick (void);
void thread_print_stats (void);
void thread_update_priority (struct thread* a);
void thread_priority_donation (struct lock *);
void thread_store_lock (struct lock *lock);
typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);
int thread_get_maxlock(struct thread *t);
void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

bool thread_priority_compare (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

int thread_get_nice (void);
void thread_set_nice (int);
void thread_update_priority_by_nice (struct thread *, void *);
int thread_get_load_avg (void);
int thread_get_recent_cpu (void);
void update_load_avg (void);
void thread_update_recent_cpu_by_one (void);
void thread_update_recent_cpu (struct thread *);
void thread_update_recent_cpu_of_all (void);
int count_ready_threads (void);
void sort_ready_list(void);
void round_robin(void);

#endif /* threads/thread.h */
