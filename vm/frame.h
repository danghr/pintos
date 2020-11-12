#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/synch.h"
#include "threads/palloc.h"
#include "vm/page.h"

/* List of all frame tables allocated here */
struct list frame_table;

struct frame_table_entry
{
  void *frame;              /* Address of the page */
  struct thread *owner;     /* Owner thread/process of the page */
  struct list_elem elem;    /* List element */
};

struct frame_table_entry *frame_find_entry (void *);

void frame_table_init (void);
void *frame_allocate_page (enum palloc_flags);
void frame_free_fte (struct frame_table_entry *);
void frame_free_page (void *);
#endif /* userprog/frame.h */
