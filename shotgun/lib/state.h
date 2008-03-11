#ifndef RBS_STATE_H
#define RBS_STATE_H

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


#include <hashtable.h>
#include <ptr_array.h>

#ifndef _WIN32
#  include <termios.h>
#endif

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/memutil.h"
#include "shotgun/lib/subtend/PortableUContext.h"

/* These are for custom literals. We'll need an API to set it
   eventually. */
#define CUSTOM_CLASS Qnil

#define SPECIAL_CLASS_MASK 0x1f
#define SPECIAL_CLASS_SIZE 32

struct rubinius_globals {
  
  /* classes for the core 'types' */
  OBJECT blokctx, cmethod, tuple, module, object, array;
  OBJECT class, hash, methtbl, bytearray, methctx, blank;
  OBJECT blokenv, bignum, regexp, regexpdata, matchdata;
  OBJECT string, symbol, io, metaclass, symtbl;
  OBJECT nil_class, true_class, false_class, fixnum_class, undef_class;
  OBJECT floatpoint, fastctx, data, nmethod, nmc, task, list, list_node;
  OBJECT channel, thread, staticscope, send_site, selector, lookuptable;
  
  /* the primary symbol table */
  OBJECT symbols;
  OBJECT method_missing;
  OBJECT sym_inherited, sym_opened_class;
  OBJECT sym_from_literal, sym_method_added, sym_s_method_added, sym_init_copy;
  OBJECT sym_plus, sym_minus, sym_equal, sym_nequal, sym_tequal, sym_lt, sym_gt;
  OBJECT exc_arg, exc_segfault;
  OBJECT exc_loe, exc_type, exc_rex;
  OBJECT exc_stack_explosion;
  OBJECT exc_primitive_failure, sym_initialize;
  
  OBJECT external_ivars, scheduled_threads, errno_mapping;
  OBJECT config, ffi_ptr, ffi_func, sym_send;
  OBJECT sym_public, sym_private, sym_protected, sym_const_missing;
  OBJECT sym_object_id, sym_call;
  OBJECT exception, iseq, icache;
  OBJECT top_scope, on_gc_channel;
  OBJECT selectors;
  
  OBJECT special_classes[SPECIAL_CLASS_SIZE];
};


#define NUM_OF_GLOBALS ((unsigned int)(sizeof(struct rubinius_globals) / SIZE_OF_OBJECT))

#define CPU_CACHE_MASK 0xfff
#define CPU_CACHE_HASH(c,m) ((((uintptr_t)(c)>>3)^((uintptr_t)m)) & CPU_CACHE_MASK)
#define CPU_CACHE_TOLERANCE 3

/* size is mask + 1 + tolerance */
#define CPU_CACHE_SIZE 0x1003

struct method_cache {
  OBJECT klass;
  OBJECT name;
  OBJECT module;
  OBJECT method;
  int is_public;
};

struct rubinius_state;

typedef struct rubinius_state* rstate;

rstate rubinius_state_new();
void state_destroy(rstate);

#ifdef STATE
#undef STATE
#endif

#define STATE rstate state

typedef void (*state_cleanup_func)(STATE, OBJECT);

struct type_info {
  /* IF the type is a ByteArray, how many fields at the front are
   * OBJECT's that need to be GC'd */
  int object_fields;
  state_cleanup_func cleanup;
};


#define FASTCTX_FIELDS 19
#define FASTCTX_NORMAL 1
#define FASTCTX_BLOCK  3
#define FASTCTX_NMC    4

#include "shotgun/lib/cpu.h"
#include "shotgun/lib/object_memory.h"
#include "shotgun/lib/subtend/handle.h"

struct rubinius_state {
  object_memory om;
  
  struct method_cache method_cache[CPU_CACHE_SIZE + CPU_CACHE_TOLERANCE];
  
#ifdef TRACK_STATS
  int cache_hits;
  int cache_misses;
  int cache_used;
  int cache_collisions;
  int cache_inline_hit;
  int cache_inline_stale;
  int cache_inline_const_hit;
#endif

  struct rubinius_globals *global;
  
  /* Used to pass information down to the garbage collectors */
  OBJECT *current_stack;
  OBJECT *current_sp;
  int ac_on_stack, home_on_stack, sender_on_stack;

#ifdef USE_CINVOKE
  CInvContext *c_context;
#endif

  rni_handle_table *handle_tbl;
  
  unsigned long *stack_bottom;
  
  struct hashtable *cleanup;
  struct hashtable *config;
  
  void *event_base;
  void *thread_infos;
  unsigned int event_id;
  
  OBJECT *samples;
  int max_samples, cur_sample;
  
  int excessive_tracing, gc_stats;
  int check_events, pending_threads, pending_events;

#ifndef _WIN32
  struct termios *termios;
#endif

  /* Used to store the value of c->ip_ptr while cpu_run isn't running */
  IP_TYPE* external_ip;

  struct type_info type_info[LastObjectType];

#ifdef TIME_LOOKUP
  uint64_t system_start;
  uint64_t lookup_time;
#endif
};

#ifdef TIME_LOOKUP
void cpu_show_lookup_time(STATE);
uint64_t get_cpu_frequency();
#endif

#define BASIC_CLASS(kind) state->global->kind
#define NEW_OBJECT(kls, size) object_memory_new_object(state->om, kls, size)
#define NEW_STRUCT(obj, str, kls, kind) \
  obj = object_memory_new_opaque(state, kls, sizeof(kind)); \
  str = (kind * )BYTES_OF(obj)

#define NEW_OBJECT_MATURE(kls, size) object_memory_new_object_mature(state->om, kls, size)  

#define DATA_STRUCT(obj, kind) ((kind)BYTES_OF(obj))
#define BYTES2FIELDS(bytes) (bytes % 4 == 0 ? bytes : ((bytes + 4) - ((bytes + 4) % 4)))

#include "shotgun/lib/bignum.h"
#include "shotgun/lib/float.h"
#include "shotgun/lib/array.h"
#include "shotgun/lib/machine.h"

#define FIRE_ACCESS 1
#define FIRE_NULL   2
#define FIRE_STACK  3
#define FIRE_ASSERT 4
#define FIRE_TYPE   5

OBJECT rbs_const_set(STATE, OBJECT module, const char *name, OBJECT obj);
OBJECT rbs_const_get(STATE, OBJECT module, const char *name);
OBJECT rbs_class_new(STATE, const char *name, int fields, OBJECT obj);
OBJECT rbs_class_new_with_namespace(STATE, const char *name, int fields, OBJECT obj, OBJECT ns);
const char *rbs_symbol_to_cstring(STATE, OBJECT sym);
OBJECT rbs_symbol_to_string(STATE, OBJECT sym);
const char *rbs_inspect(STATE, OBJECT obj);
const char *rbs_inspect_verbose(STATE, OBJECT obj);
const char *_inspect(OBJECT obj);
OBJECT rbs_module_new(STATE, const char *name, OBJECT ns);
OBJECT rbs_class_new_instance(STATE, OBJECT cls);

void *XMALLOC(size_t n);
void *XREALLOC(void *p, size_t n);
void *XCALLOC(size_t n, size_t s);
void XFREE(void *p);

#define ALLOC_N(t,n) (t*)XCALLOC(n, sizeof(t))
#define ALLOC(t) (t*)XMALLOC(sizeof(t))
#define REALLOC_N(v,t,n) (v)=(t*)XREALLOC((void*)(v), sizeof(t)*n)
#define FREE(v) XFREE(v)

static inline native_int rbs_to_int(OBJECT obj) {
  return (native_int)STRIP_TAG(obj);
}

static inline OBJECT rbs_int_to_numeric(STATE, native_int num) {
  /* Number is too big for Fixnum. Use Bignum. */
  if(num > FIXNUM_MAX || num < FIXNUM_MIN) {
    return bignum_new(state, num);
  } else {
    return APPLY_TAG((native_int)num, TAG_FIXNUM);
  }
}

/* Do NOT use the APPLY_TAG/STRIP_TAG test in the unsigned cases.
 * C doesn't let you cast from a unsigned to signed type and remove
 * the sign bit, so for large unsigned ints, the test is a false
 * positive for it fitting properly. */
static inline OBJECT rbs_uint_to_numeric(STATE, unsigned int num) {
  /* No need to check what 'num' is if it will always fit into a Fixnum */
#if (CONFIG_WORDSIZE != 64)
  if(num > FIXNUM_MAX) {
    /* Number is too big for Fixnum. Use Bignum. */
    return bignum_new_unsigned(state, num);
  }
#endif
  return APPLY_TAG((native_int)num, TAG_FIXNUM);
}

static inline OBJECT rbs_ll_to_numeric(STATE, long long num) {
  /* Number is too big for Fixnum. Use Bignum. */
  if(num > FIXNUM_MAX || num < FIXNUM_MIN) {
    return bignum_from_ll(state, num);
  } else {
    return APPLY_TAG((native_int)num, TAG_FIXNUM);
  }
}

/* See comment before rbs_uint_to_numeric */
static inline OBJECT rbs_ull_to_numeric(STATE, unsigned long long num) {
  /* Number is too big for Fixnum. Use Bignum. */
  if(num > FIXNUM_MAX) {
    return bignum_from_ull(state, num);
  } else {
    return APPLY_TAG((native_int)num, TAG_FIXNUM);
  }
}

static inline OBJECT rbs_max_long_to_numeric(STATE, long long num) {
  /* Number is too big for Fixnum. Use Bignum. */
  if(num > FIXNUM_MAX || num < FIXNUM_MIN) {
    return bignum_from_ll(state, num);
  } else {
    return APPLY_TAG((native_int)num, TAG_FIXNUM);
  }
}

static inline double rbs_fixnum_to_double(OBJECT obj) {
  double val = rbs_to_int(obj);
  return val;
}


/*
 * These macros convert between native (C) types and Ruby types
 * 'N' means a Numeric (e.g. Fixnum), 'I' means native_int (C)
 */
#define N2I(obj) ((native_int)STRIP_TAG(obj))
#define I2N(i) rbs_int_to_numeric(state, i)
#define UI2N(i) rbs_uint_to_numeric(state, i)
#define ULL2N(i) rbs_ull_to_numeric(state, i)
#define LL2N(i) rbs_ll_to_numeric(state, i)
/* Convert the longest supported integer type to a Numeric */
#define ML2N(i) rbs_max_long_to_numeric(state, (long long)i)

#define FIXNUM_TO_DOUBLE(obj) rbs_fixnum_to_double(obj)
#define FLOAT_TO_DOUBLE(k) (*DATA_STRUCT(k, double*))

void object_memory_check_ptr(void *ptr, OBJECT obj);

// #define XDEBUG 1

#ifdef XDEBUG
/* Copied from assert.h */
#define xassert(cond) ((void)((cond) ? 0 : xassert_message(#cond, __FILE__, __LINE__)))
#define xassert_message(str, file, line) \
  (printf("%s:%u: failed assertion '%s'\n", file, line, str), abort(), 0)
#else
#define xassert(cond) 
#endif

#define sassert(cond) ((void)((cond) ? 0 : machine_handle_assert(#cond, __FILE__, __LINE__)))

// #define CHECK_PTR(obj) object_memory_check_ptr(current_machine->om, obj)
#define CHECK_PTR(obj) 

void machine_handle_fire(int);
void machine_handle_assert(const char *reason, const char *file, int line);
void machine_handle_type_error(OBJECT, const char *message);

#include "shotgun/lib/environment.h"

#define current_machine (environment_current_machine())


#include "shotgun/lib/object_memory-inline.h"

void state_add_cleanup(STATE, OBJECT cls, state_cleanup_func func);
void state_run_cleanup(STATE, OBJECT obj);
void state_setup_type(STATE, int type, struct type_info *info);


#endif /* __STATE__ */
