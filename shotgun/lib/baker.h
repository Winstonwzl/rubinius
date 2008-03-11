#ifndef RBS_BAKER_H
#define RBS_BAKER_H

#include "shotgun/lib/heap.h"

struct baker_gc_struct {
  rheap space_a;
  rheap space_b;
  rheap current;
  rheap next;
  size_t used;
  int tenure_age;
  ptr_array remember_set;
  void *tenure_data;
  OBJECT (*tenure)(void*, OBJECT obj);
  int tenure_now;
  void *om;
  ptr_array seen_weak_refs;
  OBJECT become_from, become_to;
  char *last_start, *last_end;
  int num_collection;
  ptr_array tenured_objects;
};

typedef struct baker_gc_struct* baker_gc;

baker_gc baker_gc_new(int size);
address baker_gc_start_address(baker_gc g);
size_t baker_gc_used(baker_gc g);
int baker_gc_swap(baker_gc g);
int baker_gc_destroy(baker_gc g);
address baker_gc_allocate(baker_gc g, int size);
int baker_gc_set_next(baker_gc g, rheap h);
address baker_gc_allocate_spilled(baker_gc g, int size);
void baker_gc_set_forwarding_address(OBJECT obj, OBJECT dest);
OBJECT baker_gc_forwarded_object(OBJECT obj);
OBJECT baker_gc_mutate_object(STATE, baker_gc g, OBJECT obj);
int baker_gc_contains_p(baker_gc g, OBJECT obj);
int baker_gc_contains_spill_p(baker_gc g, OBJECT obj);
OBJECT baker_gc_mutate_from(STATE, baker_gc g, OBJECT iobj);
unsigned int baker_gc_collect(STATE, baker_gc g, ptr_array roots);
void baker_gc_clear_marked(baker_gc g);
void baker_gc_describe(baker_gc g);
int baker_gc_memory_allocated(baker_gc g);
int baker_gc_memory_in_use(baker_gc g);
void baker_gc_reset_used(baker_gc g);
void baker_gc_find_lost_souls(STATE, baker_gc g);
void baker_gc_collect_references(STATE, baker_gc g, OBJECT mark, ptr_array refs);
void baker_gc_mutate_context(STATE, baker_gc g, OBJECT iobj, int shifted, int top);

static inline int baker_gc_forwarded_p(OBJECT obj) {
  return FORWARDED_P(obj);
}


#define baker_gc_allocate_ultra(g, size) heap_allocate_dirty((g)->current, size)
#define baker_gc_allocate_spilled_ultra(g, size) heap_allocate_dirty((g)->next, size)

// #define MEM_DEBUG 1

#ifdef MEM_DEBUG

#define baker_gc_allocate(g, size) (heap_allocate((g)->current, size))
#define baker_gc_allocate_spilled(g, size) (heap_allocate((g)->next, size))

#else

#define baker_gc_allocate(g, size) baker_gc_allocate_ultra(g, size); g->used++;
#define baker_gc_allocate_spilled(g, size) baker_gc_allocate_spilled_ultra(g, size); g->used++;

#endif

#endif

