#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/synch.h"
#include "threads/palloc.h"
#include "vm/page.h"

/* List of all frame tables allocated here */
struct list frame_table;

/* Entries of frame table */
struct frame_table_entry
{
  /* Entry of the supplemental page table */
  void *frame;                          /* Address of the frame */
  struct sup_page_table_entry *spte;    /* Corresponding SPTE */

  /* List element */
  struct list_elem elem;
};

struct frame_table_entry *frame_find_entry (void *);

void frame_table_init (void);
struct frame_table_entry *frame_allocate_page 
  (struct sup_page_table_entry *, enum palloc_flags);
void frame_free_fte (struct frame_table_entry *);
void frame_free_page (void *);
struct frame_table_entry *find_entry_to_evict(void);
bool compare_access_time(const struct list_elem *, 
  const struct list_elem *, void *);
#endif /* userprog/frame.h */
