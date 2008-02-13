#ifndef RBS_CPU_H
#define RBS_CPU_H

#include <bstrlib.h>

/* Configuration macros. */

/* Enable direct threading */
#if CONFIG_ENABLE_DT
#define DIRECT_THREADED 1
#else
#define DIRECT_THREADED 0
#endif

/* Whether or not to do runtime tracing support */
#define EXCESSIVE_TRACING state->excessive_tracing
// #define EXCESSIVE_TRACING 0

/* For profiling of code */
#define USE_GLOBAL_CACHING 1
#define USE_INLINE_CACHING 1

/* number of contexts deep to allow */
#define CPU_MAX_DEPTH 6000

#define IP_TYPE uint32_t
#define BS_JUMP 2

#define CTX_FLAG_NO_LONG_RETURN (1<<0)

#define CPU_REGISTERS OBJECT sender; \
  OBJECT block; \
  OBJECT method; \
  OBJECT literals; \
  OBJECT locals; \
  unsigned short argcount; \
  OBJECT name; \
  OBJECT method_module; \
  void *opaque_data; \
  OBJECT self; \
  IP_TYPE *data; \
  unsigned char type; \
  unsigned char flags; \
  unsigned int ip; \
  unsigned int sp; \
  unsigned int fp;

struct fast_context {
  CPU_REGISTERS
  unsigned int size;
};

#define FASTCTX(ctx) ((struct fast_context*)BYTES_OF(ctx))

/* 1Meg of stack */
#define InitialStackSize 262144

#define TASK_NO_STACK 1
#define TASK_DEBUG_ON_CTXT_CHANGE 2
#define TASK_FLAG_P(task, flag) ((task->flags & flag) == flag)
#define TASK_SET_FLAG(task, flag) (task->flags |= flag)
#define TASK_CLEAR_FLAG(task, flag) (task->flags ^= flag)

#define CPU_TASK_REGISTERS long int args; \
  unsigned long int stack_slave; \
  long int cache_index; \
  OBJECT *stack_top; \
  unsigned long int stack_size; \
  OBJECT exception; \
  OBJECT enclosing_class; \
  OBJECT active_context, home_context, main; \
  ptr_array paths; \
  int depth; \
  OBJECT current_scope; \
  IP_TYPE **ip_ptr; \
  OBJECT *sp_ptr; \
  int call_flags; \
  OBJECT debug_channel; \
  OBJECT control_channel; \
  unsigned long int flags; \
  unsigned int blockargs;

struct cpu_task_shared {
  CPU_TASK_REGISTERS;
};

struct cpu_task {
  CPU_TASK_REGISTERS;
  unsigned int active;
};

struct rubinius_cpu {
  /* Normal registers are saved and restored per new method call . */
  OBJECT self, sender;
  OBJECT locals;
  IP_TYPE *data;
  unsigned short type;
  unsigned short argcount;
  unsigned int ip;
  unsigned int sp;
  unsigned int fp;
  
  // CPU_REGISTERS;
  
  OBJECT current_task, main_task;
  OBJECT current_thread, main_thread;
  
  /* Task registers are saved and restored when tasks are switched. */
  CPU_TASK_REGISTERS;
};

#define CPU_TASKS_LOCATION(cp) (((char*)cp) + offsetof(struct rubinius_cpu, args))

typedef struct rubinius_cpu *cpu;
typedef OBJECT (*cpu_sampler_collect_cb)(STATE, void*, OBJECT);
typedef OBJECT (*cpu_event_each_channel_cb)(STATE, void*, OBJECT);

#define cpu_stack_empty_p(state, cpu) (cpu->sp_ptr <= cpu->stack_top)
#define cpu_local_get(state, cpu, idx) (NTH_FIELD(cpu->locals, idx))
#define cpu_local_set(state, cpu, idx, obj) (SET_FIELD(cpu->locals, idx, obj))

#define SP_PTR c->sp_ptr
#define stack_push(obj) (*++SP_PTR = obj)
#define stack_pop() (*SP_PTR--)
#define stack_top() (*SP_PTR)
#define stack_back(idx) (SP_PTR[-idx])
#define stack_clear(idx) (SP_PTR -= idx)
#define stack_pop_2(v1, v2) v1 = stack_back(0); v2 = stack_back(1);
#define stack_pop_3 (v1, v2, v3) v1 = stack_back(0); v2 = stack_back(1); v3 = stack_back(2);
#define stack_set_top(val) (*SP_PTR = (val))

#define cpu_current_block(state, cpu) (FASTCTX(cpu->home_context)->block)
#define cpu_current_method(state, cpu) (FASTCTX(cpu->active_context)->method)
#define cpu_current_literals(state, cpu) (FASTCTX(cpu->active_context)->literals)
#define cpu_current_locals(state, cpu) (FASTCTX(cpu->home_context)->locals)
#define cpu_set_locals(state, cpu, obj) (FASTCTX(cpu->home_context)->locals = obj)
#define cpu_current_name(state, cpu) (FASTCTX(cpu->home_context)->name)
#define cpu_current_module(state, cpu) (FASTCTX(cpu->home_context)->method_module)
#define cpu_current_data(cpu) (FASTCTX(cpu->home_context)->data)
#define cpu_current_argcount(cpu) (cpu->argcount)
#define cpu_current_sender(cpu) (cpu->sender)
#define cpu_current_scope(state, cpu) cmethod_get_staticscope(FASTCTX(cpu->home_context)->method)

#define cpu_flush_ip(cpu) (cpu->ip = (unsigned int)(*cpu->ip_ptr - cpu->data))
#define cpu_flush_sp(cpu) (cpu->sp = (unsigned int)(cpu->sp_ptr - cpu->stack_top))

#define cpu_cache_ip(cpu) (*(cpu->ip_ptr) = (cpu->data + cpu->ip))
#define cpu_cache_sp(cpu) (cpu->sp_ptr = (cpu->stack_top + cpu->sp))

cpu cpu_new(STATE);
void cpu_destroy(cpu c);
void cpu_initialize(STATE, cpu c);
void cpu_setup_top_scope(STATE, cpu c);
void cpu_initialize_context(STATE, cpu c);
void cpu_update_roots(STATE, cpu c, ptr_array roots, int start);
void cpu_activate_context(STATE, cpu c, OBJECT ctx, OBJECT home, int so);
int cpu_return_to_sender(STATE, cpu c, OBJECT val, int consider_block, int exception);
int cpu_simple_return(STATE, cpu c, OBJECT val);
void cpu_save_registers(STATE, cpu c, int offset);
void cpu_yield_debugger_check(STATE, cpu c);

void cpu_activate_method(STATE, cpu c, struct message *msg);
OBJECT cpu_open_module(STATE, cpu c, OBJECT under, OBJECT sym);
void cpu_send_message(STATE, cpu c, struct message *msg);

OBJECT cpu_const_get_in_context(STATE, cpu c, OBJECT sym);
OBJECT cpu_const_get_from(STATE, cpu c, OBJECT sym, OBJECT under);

OBJECT cpu_const_get(STATE, cpu c, OBJECT sym, OBJECT under);
OBJECT cpu_const_set(STATE, cpu c, OBJECT sym, OBJECT val, OBJECT under);
void cpu_run(STATE, cpu c, int setup);
int cpu_dispatch(STATE, cpu c);
void cpu_compile_instructions(STATE, OBJECT bc, OBJECT ba);
OBJECT cpu_compile_method(STATE, OBJECT cm);
OBJECT cpu_create_block_context(STATE, cpu c, OBJECT env, int sp);

void cpu_set_encloser_path(STATE, cpu c, OBJECT cls);
void cpu_push_encloser(STATE, cpu c);
void cpu_add_method(STATE, cpu c, OBJECT target, OBJECT sym, OBJECT method);
void cpu_attach_method(STATE, cpu c, OBJECT target, OBJECT sym, OBJECT method);
void cpu_raise_exception(STATE, cpu c, OBJECT exc);
void cpu_raise_arg_error(STATE, cpu c, int args, int req);
void cpu_raise_arg_error_generic(STATE, cpu c, const char *msg);
void cpu_raise_from_errno(STATE, cpu c, const char *msg);
OBJECT cpu_new_exception(STATE, cpu c, OBJECT klass, const char *msg);
OBJECT cpu_new_exception2(STATE, cpu c, OBJECT klass, const char *msg, ...);
void cpu_perform_hook(STATE, cpu c, OBJECT recv, OBJECT meth, OBJECT arg);

void cpu_goto_method(STATE, cpu c, OBJECT recv, OBJECT meth,
                                     int count, OBJECT name, OBJECT block);

void cpu_send(STATE, cpu c, OBJECT recv, OBJECT sym, int args, OBJECT block);
OBJECT cpu_locate_method_on(STATE, cpu c, OBJECT obj, OBJECT sym, OBJECT include_private);
void cpu_restore_context_with_home(STATE, cpu c, OBJECT ctx, OBJECT home);
void cpu_yield_debugger(STATE, cpu c);

void cpu_run_script(STATE, cpu c, OBJECT meth);

OBJECT exported_cpu_find_method(STATE, cpu c, OBJECT recv, OBJECT name, OBJECT *mod);

OBJECT cpu_unmarshal(STATE, uint8_t *str, int len, int version);
OBJECT cpu_marshal(STATE, OBJECT obj, int version);
OBJECT cpu_unmarshal_file(STATE, const char *path, int version);
bstring cpu_marshal_to_bstring(STATE, OBJECT obj, int version);
OBJECT cpu_marshal_to_file(STATE, OBJECT obj, char *path, int version);

void cpu_bootstrap(STATE);
void cpu_add_roots(STATE, cpu c, ptr_array roots);
void cpu_update_roots(STATE, cpu c, ptr_array roots, int start);
int cpu_ip2line(STATE, OBJECT meth, int ip);


/* Method cache functions */
void cpu_clear_cache(STATE, cpu c);
void cpu_clear_cache_for_method(STATE, cpu c, OBJECT meth, int full);
void cpu_clear_cache_for_class(STATE, cpu c, OBJECT klass);

void cpu_task_flush(STATE, cpu c);
OBJECT cpu_task_dup(STATE, cpu c, OBJECT cur);
int cpu_task_select(STATE, cpu c, OBJECT self);
OBJECT cpu_task_associate(STATE, cpu c, OBJECT self, OBJECT be);
void cpu_task_set_debugging(STATE, OBJECT self, OBJECT dc, OBJECT cc);
OBJECT cpu_channel_new(STATE);
OBJECT cpu_channel_send(STATE, cpu c, OBJECT self, OBJECT obj);
void cpu_channel_receive(STATE, cpu c, OBJECT self, OBJECT cur_task);
int cpu_channel_has_readers_p(STATE, OBJECT self);
void cpu_event_clear_channel(STATE, OBJECT chan);

OBJECT cpu_thread_new(STATE, cpu c);
void cpu_thread_dequeue(STATE, OBJECT thr);
void cpu_thread_switch(STATE, cpu c, OBJECT thr);
OBJECT cpu_thread_get_task(STATE, OBJECT self);
void cpu_thread_preempt(STATE, cpu c);
void cpu_thread_schedule(STATE, OBJECT self);
void cpu_thread_run_best(STATE, cpu c);
void cpu_thread_suspend_current(STATE, cpu c);
void cpu_thread_force_run(STATE, cpu c, OBJECT thr);
void cpu_thread_exited(STATE, cpu c);
int cpu_thread_alive_p(STATE, OBJECT self);

void cpu_task_disable_preemption(STATE);
void cpu_task_configure_preemption(STATE);

void cpu_sampler_collect(STATE, cpu_sampler_collect_cb, void *cb_data);

#define cpu_event_outstanding_p(state) (state->thread_infos != NULL)
#define cpu_event_update(state) if(cpu_event_outstanding_p(state)) cpu_event_runonce(state)
void cpu_event_runonce(STATE);
void cpu_event_init(STATE);
void cpu_event_run(STATE);
void cpu_event_each_channel(STATE, cpu_event_each_channel_cb, void *cb_data);
OBJECT cpu_event_wake_channel(STATE, cpu c, OBJECT channel, double seconds);
OBJECT cpu_event_wait_readable(STATE, cpu c, OBJECT channel, int fd, OBJECT buffer, int count);
OBJECT cpu_event_wait_writable(STATE, cpu c, OBJECT channel, int fd);
OBJECT cpu_event_wait_signal(STATE, cpu c, OBJECT channel, int sig);
OBJECT cpu_event_wait_child(STATE, cpu c, OBJECT channel, int pid, int flags);
int cpu_event_cancel_event(STATE, OBJECT oid);
void cpu_channel_register(STATE, cpu c, OBJECT self, OBJECT cur_thr);
void cpu_event_setup_children(STATE, cpu c);
void cpu_event_clear(STATE, int fd);
void cpu_find_waiters(STATE);

#define channel_set_waiting(obj, val) SET_FIELD(obj, 1, val)
#define channel_get_waiting(obj) NTH_FIELD(obj, 1)
#define channel_set_value(obj, val) SET_FIELD(obj, 2, val)
#define channel_get_value(obj) NTH_FIELD(obj, 2)

void cpu_sampler_init(STATE, cpu c);
void cpu_sampler_activate(STATE, int hz);
OBJECT cpu_sampler_disable(STATE);

#define type_assert(obj, type, message) ({\
  if(type == FixnumType) {\
    if(!FIXNUM_P(obj)) machine_handle_type_error(obj, message); \
  } else if(type == SymbolType) { \
    if(!SYMBOL_P(obj)) machine_handle_type_error(obj, message); \
  } else {\
    if(!REFERENCE_P(obj) || obj->obj_type != type) \
      machine_handle_type_error(obj, message); \
  }\
})

#define cpu_stack_push(state, c, oop, check) (*++(c)->sp_ptr = oop);
#define cpu_stack_pop(state, c) (*(c)->sp_ptr--)
#define cpu_stack_top(state, c) (*(c)->sp_ptr)
#define cpu_stack_set_top(state, c, oop) (*(c)->sp_ptr = oop)

#include "shotgun/lib/sendsite.h"

void cpu_initialize_sendsite(STATE, struct send_site *ss);
typedef int (*prim_func)(STATE, cpu c, const struct message *msg);
void cpu_patch_primitive(STATE, const struct message *msg, prim_func func);
int cpu_perform_system_primitive(STATE, cpu c, int prim, const struct message *msg);

void cpu_patch_ffi(STATE, const struct message *msg);
void ffi_call(STATE, cpu c, OBJECT ptr);
void ffi_autorelease(OBJECT ptr, int ar);
OBJECT ffi_new_pointer(STATE, void *ptr);

#endif /* RBS_CPU_H */
