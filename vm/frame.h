#ifndef FRAME_H
#define FRAME_H

#include <list.h>
#include "threads/synch.h"
#include "threads/palloc.h"

/* List of all frames */
struct list frame_table;

struct frame_table_entry
{
  void *page;               /* Address of the page */
  struct thread *owner;     /* Owner thread/process of the page */
  struct list_elem elem;    /* List element */
};

void frame_table_init ();
void *frame_allocate_page (enum palloc_flags);
void frame_free_page (struct frame_table_entry *);
#endif /* vm/frame.h */
