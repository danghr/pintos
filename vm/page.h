#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"

/* Entries of supplemental page table */
struct sup_page_table_entry
{
  void *page;               /* User virtual address of the page */
  uint64_t access_time;     /* Record the last access time for LRU */
  bool dirty;
  bool accessed;
  struct thread *owner;     /* Owner thread/process of the page */
  struct list_elem elem;
};

struct sup_page_table_entry *sup_page_find_entry (struct thread *, void *);

struct sup_page_table_entry *sup_page_allocate_page 
  (struct thread *, enum palloc_flags);
void sup_page_free_spte (struct sup_page_table_entry *);
void sup_page_free_page (struct thread *, void *);
#endif /* vm/page.h */
