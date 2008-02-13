#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/cpu.h"
#include "shotgun/lib/cleanup_hash.h"
#include "shotgun/lib/config_hash.h"
#include "shotgun/lib/machine.h"
#include "shotgun/lib/bignum.h"
#include "shotgun/lib/methctx.h"

#ifdef TIME_LOOKUP
#include <mach/mach_time.h>
#endif

static size_t _gc_current_limit = 0;
#define GC_EXTERNAL_LIMIT 10000000

static void inc_mem(size_t n) {
  _gc_current_limit += n;

  if(_gc_current_limit > GC_EXTERNAL_LIMIT) {
    _gc_current_limit = 0;
    current_machine->s->om->collect_now = OMCollectYoung;    
  }
}

void *XMALLOC(size_t n) {
  inc_mem(n);
  return malloc(n);
}

void *XREALLOC(void *p, size_t n) {
  inc_mem(n);
  return realloc(p, n);
}

void *XCALLOC(size_t n, size_t s) {
  inc_mem(n * s);
  return calloc(n, s);
}

void XFREE(void *p) {
  free(p);
}

rstate rubinius_state_new() {
  rstate st;
  st = (rstate)calloc(1, sizeof(struct rubinius_state));
  st->om = object_memory_new();
  st->global = (struct rubinius_globals*)calloc(1, sizeof(struct rubinius_globals));
  st->cleanup = ht_cleanup_create(11);
  st->config = ht_config_create(11);
#ifdef TIME_LOOKUP
  st->system_start = mach_absolute_time();
  st->lookup_time = 0;
#endif
  
  st->proxy = (struct inter_proxy*)calloc(1, sizeof(struct inter_proxy));
  st->proxy->bignum_new = bignum_new;
  st->proxy->methctx_reference = methctx_reference;
  st->proxy->open_module = cpu_open_module;
  st->proxy->attach_method = cpu_attach_method;
  st->proxy->add_method = cpu_add_method;
  st->proxy->perform_hook = cpu_perform_hook;
  st->proxy->activate_method = cpu_activate_method;
  st->proxy->object_class = object_class;
  st->proxy->send_message = cpu_send_message;
  st->proxy->get_ivar = object_get_ivar;
  st->proxy->set_ivar = object_set_ivar;
  st->proxy->write_barrier = object_memory_write_barrier;
  st->proxy->simple_return = cpu_simple_return;
  st->proxy->set_encloser = cpu_set_encloser_path;
  st->proxy->push_encloser = cpu_push_encloser;

  return st;
}

void state_destroy(STATE) {
  object_memory_destroy(state->om);
  free(state->global);

  ht_cleanup_destroy(state->cleanup);
  ht_config_destroy(state->config);

  free(state->proxy);
  free(state);
}

static ptr_array _gather_roots(STATE, cpu c) {
  ptr_array roots;
  roots = ptr_array_new(NUM_OF_GLOBALS + 100);

  memcpy(roots->array, state->global, sizeof(struct rubinius_globals));
  roots->length = NUM_OF_GLOBALS;

  cpu_add_roots(state, c, roots);
  /* truncate the free_context list since we don't care about them
     after we've collected anyway */
  return roots;
}

void cpu_sampler_suspend(STATE);
void cpu_sampler_resume(STATE);
void cpu_hard_cache(STATE, cpu c);

void state_collect(STATE, cpu c) {
  ptr_array roots;
  int stats = state->gc_stats;
  struct timeval start, fin;

  cpu_task_flush(state, c);

  if(stats) {
    gettimeofday(&start, NULL);
  }

  cpu_flush_ip(c);
  cpu_flush_sp(c);

  state->current_stack = c->stack_top;
  state->current_sp =    c->sp_ptr;

  /* HACK: external_ivars needs to be moved out of being a generic
      global and being a special case one so that it's references
      can't keep objects alive. */

  cpu_sampler_suspend(state);
  object_memory_formalize_contexts(state, state->om);
  roots = _gather_roots(state, c);
  object_memory_collect(state, state->om, roots);
  memcpy(state->global, roots->array, sizeof(struct rubinius_globals));
  cpu_update_roots(state, c, roots, NUM_OF_GLOBALS);

  object_memory_reset_contexts(state, state->om);

  ptr_array_free(roots);

  baker_gc_find_lost_souls(state, state->om->gc);
  cpu_sampler_resume(state);

  if(stats) {
    double elapse;
    gettimeofday(&fin, NULL);
    elapse =  (fin.tv_sec - start.tv_sec);
    elapse += (((double)fin.tv_usec - start.tv_usec) / 1000000);
    printf("[GC Y %f secs, %ldK total, %3dK used, %4d tenured, %d]\n",
      elapse,
      (long int)(state->om->gc->current->size / 1024),
      (unsigned int)(((uintptr_t)state->om->gc->current->current - (uintptr_t)state->om->gc->current->address) / 1024),
      state->om->last_tenured,
      state->om->gc->num_collection
    );
  }

  cpu_task_flush(state, c);
  cpu_hard_cache(state, c);
  cpu_cache_sp(c);
}


void state_major_collect(STATE, cpu c) {
  ptr_array roots;
  int stats = state->gc_stats;
  struct timeval start, fin;

  cpu_task_flush(state, c);

  state_collect(state, c);

  if(stats) {
    gettimeofday(&start, NULL);
  }

  cpu_flush_ip(c);
  cpu_flush_sp(c);

  /* HACK: external_ivars needs to be moved out of being a generic
      global and being a special case one so that it's references
      can't keep objects alive. */

  state->current_stack = c->stack_top;
  state->current_sp =    c->sp_ptr;

  cpu_sampler_suspend(state);
  roots = _gather_roots(state, c);
  object_memory_major_collect(state, state->om, roots);
  memcpy(state->global, roots->array, sizeof(struct rubinius_globals));
  cpu_update_roots(state, c, roots, NUM_OF_GLOBALS);

  ptr_array_free(roots);
  cpu_sampler_suspend(state);

  if(stats) {
    double elapse;
    gettimeofday(&fin, NULL);
    elapse =  (fin.tv_sec - start.tv_sec);
    elapse += (((double)fin.tv_usec - start.tv_usec) / 1000000);

    printf("[GC M %f secs, %d freed, %d total, %d segments, %6dK total]\n", 
      elapse,
      state->om->ms->last_freed, state->om->ms->last_marked,
      state->om->ms->num_chunks,
      state->om->ms->allocated_bytes / 1024
      );
  }

  cpu_task_flush(state, c);
  cpu_hard_cache(state, c);
  cpu_cache_sp(c);
}

void state_object_become(STATE, cpu c, OBJECT from, OBJECT to) {
  ptr_array roots;

  state->current_stack = c->stack_top;
  state->current_sp =    c->sp_ptr;

  roots = _gather_roots(state, c);

  object_memory_setup_become(state, state->om, from, to);

  /* If from is young, then all the refs are from other young objects
     or the remember set, so we just need to mutate in the young space. */
  if(from->gc_zone == YoungObjectZone) {
    object_memory_collect(state, state->om, roots);
  } else {
    object_memory_major_collect(state, state->om, roots);
  }

  object_memory_clear_become(state, state->om);

  memcpy(state->global, roots->array, sizeof(struct rubinius_globals));
  cpu_update_roots(state, c, roots, NUM_OF_GLOBALS);

  ptr_array_free(roots);

}

void state_add_cleanup(STATE, OBJECT cls, state_cleanup_func func) {
  int type = N2I(class_get_object_type(cls));

  state->type_info[type].cleanup = func;
  // printf("Registered cleanup for %p\n", module_get_name(cls));
  class_set_needs_cleanup(cls, Qtrue);
}

void state_run_cleanup(STATE, OBJECT obj) {
  state_cleanup_func func;

  func = state->type_info[obj->obj_type].cleanup;
  
  if(func) func(state, obj);
}

void state_setup_type(STATE, int type, struct type_info *info) {
  state->type_info[type] = *info;
}
