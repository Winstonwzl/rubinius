#ifndef RBS_OBJECT_MEMORY_H
#define RBS_OBJECT_MEMORY_H

#include <stdlib.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/baker.h"
#include "shotgun/lib/marksweep.h"

#define OMDefaultSize 4194304
/* A little over 1% of the total heap size. */
#define LargeObjectThreshold 2700
//#define OMDefaultSize 100000

#define OMCollectYoung  0x1
#define OMCollectMature 0x2

struct object_memory_struct {
  int collect_now;
  int enlarge_now;
  int tenure_now;
  int new_size;
  int last_object_id;
  baker_gc gc;
  mark_sweep_gc ms;
  int last_tenured;
  int bootstrap_loaded;
  
  rheap contexts;
  /* The first not referenced stack context */
  OBJECT context_bottom;
  OBJECT context_last;
  
  int context_offset;
};

typedef struct object_memory_struct *object_memory;

object_memory object_memory_new();
int object_memory_destroy(object_memory om);
size_t object_memory_used(object_memory om);
int object_memory_collect(STATE, object_memory om, ptr_array roots);
void object_memory_check_memory(object_memory om);
OBJECT object_memory_new_object_normal(object_memory om, OBJECT cls, unsigned int fields);
static inline OBJECT _om_inline_new_object(object_memory om, OBJECT cls, unsigned int fields);

OBJECT object_memory_new_object_mature(object_memory om, OBJECT cls, unsigned int fields);
void object_memory_print_stats(object_memory om);
OBJECT object_memory_new_opaque(STATE, OBJECT cls, unsigned int sz);
OBJECT object_memory_tenure_object(void* data, OBJECT obj);
void object_memory_major_collect(STATE, object_memory om, ptr_array roots);
OBJECT object_memory_collect_references(STATE, object_memory om, OBJECT mark);
void object_memory_setup_become(STATE, object_memory om, OBJECT from, OBJECT to);
void object_memory_clear_become(STATE, object_memory om);
void object_memory_update_rs(object_memory om, OBJECT target, OBJECT val);

void object_memory_shift_contexts(STATE, object_memory om);
void object_memory_mark_contexts(STATE, object_memory om);
void object_memory_formalize_contexts(STATE, object_memory om);
void object_memory_reset_contexts(STATE, object_memory om);

#define FAST_NEW 1

#ifdef FAST_NEW
#define object_memory_new_object _om_inline_new_object_init
#else
#define object_memory_new_object object_memory_new_object_normal
#endif

#define object_memory_new_dirty_object _om_inline_new_object

#define CTX_SIZE SIZE_IN_BYTES_FIELDS(FASTCTX_FIELDS)

#define BYTES_PAST(ctx, num) ((char*)ctx + num)
#define AFTER_CTX(ctx) BYTES_PAST(ctx, FASTCTX(ctx)->size)

static inline OBJECT object_memory_new_context(object_memory om, unsigned int locals) {
  unsigned int size;
  OBJECT ctx;
  
  if(locals > 0) {
    size = (unsigned int)(CTX_SIZE + SIZE_IN_BYTES_FIELDS(locals) + 4);
  } else {
    size = (unsigned int)CTX_SIZE;
  }
  
  ctx = ((OBJECT)heap_allocate_dirty(om->contexts, size));
  // memset(ctx, 0, size);
  
  /* not really the number of fields, rather the number of bytes
     this context is using. */
  FASTCTX(ctx)->size = size;
  
  return ctx;
}

#define object_memory_context_locals(ctx) ((OBJECT)BYTES_PAST(ctx, CTX_SIZE))
  
#define om_on_stack(om, ctx) heap_contains_p(om->contexts, ctx)
#define om_in_heap(om, ctx) heap_contains_p(om->gc->current, ctx)

#define object_memory_retire_context(om, ctx) \
if(om_on_stack(om, ctx) && (ctx >= om->context_bottom)) { \
  fast_memfill_s20((void*)ctx, 0); heap_putback(om->contexts, FASTCTX(ctx)->size); \
}

#define object_memory_context_referenced(om, ctx) (void)({	  \
 OBJECT _nb = (OBJECT)AFTER_CTX(ctx);	      \
  if(om_on_stack(om, ctx) &&	(om->context_bottom < _nb)) {   \
    om->context_bottom = _nb; } })

#define om_context_referenced_p(om, ctx) ((ctx < om->context_bottom) && (ctx >= (OBJECT)om->contexts->address))

#define om_stack_context_p(om, ctx) (om_on_stack(om, ctx) && (ctx >= om->context_bottom))

#define om_stack_next_ctx(ctx) ((OBJECT)AFTER_CTX(ctx))
#define om_stack_prev_ctx(ctx) ((OBJECT)BYTES_PAST(ctx, -FASTCTX(ctx)->size))
#define om_stack_sender(ctx) om_stack_prev_ctx(ctx)

#define om_valid_context_p(state, ctx) ( \
  (om_stack_context_p(state->om, ctx) && stack_context_p(ctx)) || \
  (om_context_referenced_p(state->om, ctx)) || \
  (om_in_heap(state->om, ctx) && (methctx_is_fast_p(state, ctx) ||  blokctx_s_block_context_p(state, ctx))) \
)

#define EACH_CTX(om, addr) \
  addr = (OBJECT)om->contexts->address; \
  while(addr < (OBJECT) om->contexts->current) {
    
#define DONE_EACH_CTX(addr) addr = (address)AFTER_CTX(addr); }

#define EACH_REFD_CTX(om, addr) \
  addr = (OBJECT)om->contexts->address; \
  while(addr < (OBJECT) om->context_bottom) {
    
#define DONE_EACH_REFD_CTX(addr) addr = (address)AFTER_CTX(addr); }

#define EACH_STACK_CTX(om, addr) \
  addr = (OBJECT)om->context_bottom; \
  while(addr < (OBJECT) om->contexts->current) {
    
#define DONE_EACH_STACK_CTX(addr) addr = (address)AFTER_CTX(addr); }

#define om_no_referenced_ctxs_p(om) (om->context_bottom == (OBJECT)om->contexts->address)

/* These are the 4 scenarios detailed in doc/life_of_a_context.txt */

#define om_valid_sender_p(om, ctx, sender) ( \
  (NIL_P(sender) && om_on_stack(om, ctx)) || \
  (om_on_stack(om, ctx) && om_on_stack(om, sender) && (om_context_referenced_p(om, sender) || (sender == om_stack_sender(ctx)))) || \
  (om_in_heap(om, sender) && om_on_stack(om, ctx) && (om->context_bottom == ctx)) || \
  (om_in_heap(om, ctx) && (om_context_referenced_p(om, sender) || om_in_heap(om, sender))))

#endif
