#include <lightning.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/symbol.h"
#include "shotgun/lib/object.h"
#include "shotgun/lib/string.h"
#include "shotgun/lib/hash.h"
#include "shotgun/lib/primitive_indexes.h"

#include "shotgun/lib/subtend/library.h"
#include "shotgun/lib/subtend/nmethod.h"
#include "shotgun/lib/subtend/nmc.h"
#include "shotgun/lib/subtend/ffi.h"

#if defined(__amd64__) || defined(__x86_64__) || defined(X86_64)
  #include "shotgun/lib/subtend/ffi_amd64.h"
#elif defined(i386)
  #include "shotgun/lib/subtend/ffi_x86.h"
#endif


void Init_ffi(STATE) {
  OBJECT mod;
  BASIC_CLASS(ffi_ptr) = rbs_class_new(state, "MemoryPointer", 0, BASIC_CLASS(bytearray));
  class_set_object_type(BASIC_CLASS(ffi_ptr), I2N(MemPtrType));

  BASIC_CLASS(ffi_func) = rbs_class_new(state, "NativeFunction", 0, BASIC_CLASS(object));

  mod = rbs_module_new(state, "FFI", BASIC_CLASS(object));
  rbs_const_set(state, mod, "TYPE_OBJECT",   I2N(RBX_FFI_TYPE_OBJECT));
  rbs_const_set(state, mod, "TYPE_CHAR",     I2N(RBX_FFI_TYPE_CHAR));
  rbs_const_set(state, mod, "TYPE_UCHAR",    I2N(RBX_FFI_TYPE_UCHAR));
  rbs_const_set(state, mod, "TYPE_SHORT",    I2N(RBX_FFI_TYPE_SHORT));
  rbs_const_set(state, mod, "TYPE_USHORT",   I2N(RBX_FFI_TYPE_USHORT));
  rbs_const_set(state, mod, "TYPE_INT",      I2N(RBX_FFI_TYPE_INT));
  rbs_const_set(state, mod, "TYPE_UINT",     I2N(RBX_FFI_TYPE_UINT));
  rbs_const_set(state, mod, "TYPE_LONG",     I2N(RBX_FFI_TYPE_LONG));
  rbs_const_set(state, mod, "TYPE_ULONG",    I2N(RBX_FFI_TYPE_ULONG));
  rbs_const_set(state, mod, "TYPE_LL",       I2N(RBX_FFI_TYPE_LL));
  rbs_const_set(state, mod, "TYPE_ULL",      I2N(RBX_FFI_TYPE_ULL));
  rbs_const_set(state, mod, "TYPE_FLOAT",    I2N(RBX_FFI_TYPE_FLOAT));
  rbs_const_set(state, mod, "TYPE_DOUBLE",   I2N(RBX_FFI_TYPE_DOUBLE));
  rbs_const_set(state, mod, "TYPE_PTR",      I2N(RBX_FFI_TYPE_PTR));
  rbs_const_set(state, mod, "TYPE_VOID",     I2N(RBX_FFI_TYPE_VOID));
  rbs_const_set(state, mod, "TYPE_STRING",   I2N(RBX_FFI_TYPE_STRING));
  rbs_const_set(state, mod, "TYPE_STATE",    I2N(RBX_FFI_TYPE_STATE));
  rbs_const_set(state, mod, "TYPE_STRPTR",   I2N(RBX_FFI_TYPE_STRPTR));
  rbs_const_set(state, mod, "TYPE_CHARARR",  I2N(RBX_FFI_TYPE_CHARARR));
}

#ifdef RBS_DISASS
void disassemble(FILE*, char*, char*);
#endif

static rni_handle* _ffi_pop() {
  rni_handle *h;
  rni_context *ctx = subtend_retrieve_context();
  h = nmc_handle_new(ctx->nmc, ctx->state->handle_tbl, cpu_stack_pop(ctx->state, ctx->cpu));
  return h;
}

#define ffi_pop(reg) jit_prepare(0); (void)(jit_calli(_ffi_pop)); jit_retval_p(reg);

char *ffi_generate_c_stub(STATE, int args, void *func) {
  void *start; /*, *end; */
  void *codebuf; /*, *res; */
  int in, i; /*, vars, aligned, size */
  int *ids;

  /* The stub is called with the receiver, so if there are no args, we
     can just use func as the stub. */
  if(args == 0) return func;

  /* Until lightning supports more than 6 args, we can only generate a stub
     for 5 args (0 is the receiver). */
  if(args > 5) return NULL;

  codebuf = ALLOC_N(char, 4096);
  start = codebuf;

  (void)jit_set_ip((jit_insn*)codebuf);

  jit_prolog(1);

  in = jit_arg_p();
  jit_getarg_p(JIT_V0, in);

  if(args == 0) {
    jit_prepare_i(1);
    jit_pusharg_p(JIT_V0);
  }
 /* else if(args == 1) {
    ffi_pop(JIT_V1);
    jit_prepare_i(2);
    jit_pusharg_p(JIT_V1);
    jit_pusharg_p(JIT_V0);

  } else if(args == 2) {
    ffi_pop(JIT_V1);
    ffi_pop(JIT_V2);
    jit_prepare_i(3);
    jit_pusharg_p(JIT_V2);
    jit_pusharg_p(JIT_V1);
    jit_pusharg_p(JIT_V0);

  } */
  else {

    ids = ALLOC_N(int, args);

    for(i = 0; i < args; i++) {
      ffi_pop(JIT_V1);
      ids[i] = jit_allocai(sizeof(void*));
      jit_stxi_p(ids[i], JIT_FP, JIT_V1);
    }

    jit_prepare_i(args + 1);

    for(i = args - 1; i >= 0; i--) {
      jit_ldxi_p(JIT_R2, JIT_FP, ids[i]);
      jit_pusharg_p(JIT_R2);
    }

    jit_pusharg_p(JIT_V0);
    XFREE(ids);
  }

  jit_finish(func);
  jit_retval_p(JIT_RET);
  jit_ret();

  if(getenv("RBX_SHOW_STUBS")) {
    printf("Assembly stub for: %p (%d)\n", func, args);

    #ifdef RBS_DISASS
    disassemble(stderr, start, jit_get_ip().ptr);
    #endif
  }

  jit_flush_code(start, jit_get_ip().ptr);
  return start;

/*  size = jit_get_ip().ptr - start;
  res = calloc(size, sizeof(char));
  memcpy(res, start, size);
  XFREE(codebuf);

  jit_flush_code(res, res - size);

  return res;
*/
}

int ffi_type_size(int type) {
  switch(type) {
    case RBX_FFI_TYPE_CHAR:
    case RBX_FFI_TYPE_UCHAR:
    return sizeof(char);

    case RBX_FFI_TYPE_SHORT:
    case RBX_FFI_TYPE_USHORT:
    return sizeof(short);

    case RBX_FFI_TYPE_INT:
    case RBX_FFI_TYPE_UINT:
    return sizeof(int);

    case RBX_FFI_TYPE_LONG:
    case RBX_FFI_TYPE_ULONG:
    return sizeof(long);

    case RBX_FFI_TYPE_LL:
    case RBX_FFI_TYPE_ULL:
    return sizeof(long long);

    case RBX_FFI_TYPE_FLOAT:
    return sizeof(float);

    case RBX_FFI_TYPE_DOUBLE:
    return sizeof(double);

    case RBX_FFI_TYPE_PTR:
    case RBX_FFI_TYPE_STRING:
    case RBX_FFI_TYPE_STATE:
    case RBX_FFI_TYPE_STRPTR:
    case RBX_FFI_TYPE_OBJECT:
    return sizeof(void*);

    default:

    return -1;
  }
}

int ffi_get_alloc_size(int type) {
  int i;
  i = ffi_type_size(type);
  i = (i + 3) & ~3;
  return i;
}

OBJECT ffi_to_object() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);
  return obj;
}

void ffi_from_object(OBJECT obj) {
  rni_context *ctx = subtend_retrieve_context();
  cpu_stack_push(ctx->state, ctx->cpu, obj, FALSE);
}

/* char */

char ffi_to_char() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  type_assert(obj, FixnumType, "converting to char");

  return (char)N2I(obj);
}

void ffi_from_char(char c) {
  OBJECT ret;
  STATE;
  rni_context *ctx = subtend_retrieve_context();
  state = ctx->state;
  ret = I2N((int)c);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

unsigned char ffi_to_uchar() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  type_assert(obj, FixnumType, "converting to unsigned char");

  return (unsigned char)N2I(obj);
}

void ffi_from_uchar(unsigned char c) {
  OBJECT ret;
  STATE;
  rni_context *ctx = subtend_retrieve_context();
  state = ctx->state;

  ret = I2N((int)c);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

/* short */

short ffi_to_short() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  type_assert(obj, FixnumType, "converting to short");

  return (short)N2I(obj);
}

void ffi_from_short(short c) {
  OBJECT ret;
  STATE;
  rni_context *ctx = subtend_retrieve_context();
  state = ctx->state;
  ret = I2N((int)c);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

unsigned short ffi_to_ushort() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  type_assert(obj, FixnumType, "converting to unsigned short");

  return (unsigned short)N2I(obj);
}

void ffi_from_ushort(unsigned short c) {
  OBJECT ret;
  STATE;
  rni_context *ctx = subtend_retrieve_context();
  state = ctx->state;

  ret = I2N((int)c);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}


/* int */

int ffi_to_int() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  if(FIXNUM_P(obj)) {
    return N2I(obj);
  } else {
    type_assert(obj, BignumType, "converting to int");
    return bignum_to_i(ctx->state, obj);
  }
}

void ffi_from_int(int obj) {
  OBJECT ret;
  STATE;
  rni_context *ctx = subtend_retrieve_context();
  state = ctx->state;

  ret = I2N(obj);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

unsigned int ffi_to_uint() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  if(FIXNUM_P(obj)) {
    return (unsigned int)N2I(obj);
  } else {
    type_assert(obj, BignumType, "converting to unsigned int");
    return bignum_to_ui(ctx->state, obj);
  }
}

void ffi_from_uint(unsigned int obj) {
  OBJECT ret;
  STATE;
  rni_context *ctx = subtend_retrieve_context();

  state = ctx->state;

  ret = UI2N(obj);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

/* ll */
long long ffi_to_ll() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  if(FIXNUM_P(obj)) {
    return (long long)N2I(obj);
  } else {
    type_assert(obj, BignumType, "converting to long long");
    return bignum_to_ll(ctx->state, obj);
  }
}

void ffi_from_ll(long long val) {
  rni_context *ctx = subtend_retrieve_context();
  STATE;
  state = ctx->state;

  cpu_stack_push(ctx->state, ctx->cpu, LL2N(val), FALSE);
}

unsigned long long ffi_to_ull() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  if(FIXNUM_P(obj)) {
    return (unsigned long long)N2I(obj);
  } else {
    type_assert(obj, BignumType, "converting to unsigned long long");
    return (unsigned long long)bignum_to_ull(ctx->state, obj);
  }
}

void ffi_from_ull(unsigned long long val) {
  rni_context *ctx = subtend_retrieve_context();
  STATE;
  state = ctx->state;

  cpu_stack_push(ctx->state, ctx->cpu, ULL2N(val), FALSE);
}

/* long */
long ffi_to_long() {
  if(sizeof(long) == sizeof(long long)) {
    return (long)ffi_to_ll();
  } else {
    return (long)ffi_to_int();
  }
}

void ffi_from_long(long obj) {
  if(sizeof(long) == sizeof(long long)) {
    return ffi_from_ll(obj);
  } else {
    return ffi_from_int(obj);
  }
}

unsigned long ffi_to_ulong() {
  if(sizeof(long) == sizeof(long long)) {
    return (unsigned long)ffi_to_ull();
  } else {
    return (unsigned long)ffi_to_uint();
  }
}

void ffi_from_ulong(unsigned long obj) {
  if(sizeof(long) == sizeof(long long)) {
    return ffi_from_ull(obj);
  } else {
    return ffi_from_uint(obj);
  }
}

/* float */
float ffi_to_float() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  type_assert(obj, FloatType, "converting to float");

  return (float)FLOAT_TO_DOUBLE(obj);
}

void ffi_from_float(float val) {
  OBJECT ret;
  rni_context *ctx = subtend_retrieve_context();
  ret = float_new(ctx->state, (double)val);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

/* double */
double ffi_to_double() {
  OBJECT obj;
  double ret;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  type_assert(obj, FloatType, "converting to double");

  ret = FLOAT_TO_DOUBLE(obj);
  return ret;
}

void ffi_from_double(double val) {
  OBJECT ret;
  rni_context *ctx = subtend_retrieve_context();
  ret = float_new(ctx->state, val);
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

/* ptr */
void* ffi_to_ptr() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);

  if(NIL_P(obj)) return NULL;

  type_assert(obj, MemPtrType, "converting to pointer");

  return (*DATA_STRUCT(obj, void**));
}

void ffi_from_ptr(void *ptr) {
  OBJECT ret;
  STATE;

  void **code_start;

  rni_context *ctx = subtend_retrieve_context();
  state = ctx->state;

  if(ptr == NULL) {
    ret = Qnil;
  } else {
    NEW_STRUCT(ret, code_start, BASIC_CLASS(ffi_ptr), void*);
    *code_start = ptr;
  }
  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

void ffi_from_void(int dummy) {
  rni_context *ctx = subtend_retrieve_context();
  cpu_stack_push(ctx->state, ctx->cpu, Qnil, FALSE);
}

/* string */

char *ffi_to_string() {
  OBJECT obj;
  rni_context *ctx = subtend_retrieve_context();
  obj = cpu_stack_pop(ctx->state, ctx->cpu);
  
  /* We explicitly allow a NULL for a string, because
     stuff like Socket.getaddrinfo needs it. */
  if(obj == Qnil) {
    return NULL;
  } else {
    type_assert(obj, StringType, "converting to C string");
    return string_byte_address(ctx->state, obj);
  }
}

void ffi_from_string(char *str) {
  OBJECT ret;
  rni_context *ctx = subtend_retrieve_context();

  if (NULL == str) {
    ret = Qnil;
  } else {
    ret = string_new(ctx->state, str);
  }

  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

/* state */
void* ffi_to_state() {
  rni_context *ctx = subtend_retrieve_context();
  return (void*)ctx->state;
}

/* strptr */
void ffi_from_strptr(char *str) {
  OBJECT obj, ptr, ret;
  STATE;

  void **code_start;

  rni_context *ctx = subtend_retrieve_context();
  obj = string_new(ctx->state, str);

  state = ctx->state;
  NEW_STRUCT(ptr, code_start, BASIC_CLASS(ffi_ptr), void*);
  *code_start = (void*)str;

  ret = array_new(ctx->state, 2);
  array_set(ctx->state, ret, 0, obj);
  array_set(ctx->state, ret, 1, ptr);

  cpu_stack_push(ctx->state, ctx->cpu, ret, FALSE);
}

void* ffi_get_to_converter(int type) {
  switch(type) {
    case RBX_FFI_TYPE_OBJECT:
    return ffi_to_object;

    case RBX_FFI_TYPE_CHAR:
    return ffi_to_char;

    case RBX_FFI_TYPE_UCHAR:
    return ffi_to_uchar;

    case RBX_FFI_TYPE_SHORT:
    return ffi_to_short;

    case RBX_FFI_TYPE_USHORT:
    return ffi_to_ushort;

    case RBX_FFI_TYPE_INT:
    return ffi_to_int;

    case RBX_FFI_TYPE_UINT:
    return ffi_to_uint;

    case RBX_FFI_TYPE_LONG:
    return ffi_to_long;

    case RBX_FFI_TYPE_ULONG:
    return ffi_to_ulong;

    case RBX_FFI_TYPE_LL:
    return ffi_to_ll;

    /* FIXME: have a real converter */
    case RBX_FFI_TYPE_ULL:
    return ffi_to_ull;

    case RBX_FFI_TYPE_FLOAT:
    return ffi_to_float;

    case RBX_FFI_TYPE_DOUBLE:
    return ffi_to_double;

    case RBX_FFI_TYPE_PTR:
    return ffi_to_ptr;

    case RBX_FFI_TYPE_STRING:
    return ffi_to_string;

    case RBX_FFI_TYPE_STATE:
    return ffi_to_state;

    default:

    return NULL;
  }
}

void* ffi_get_from_converter(int type) {
  switch(type) {
    case RBX_FFI_TYPE_OBJECT:
    return ffi_from_object;

    case RBX_FFI_TYPE_CHAR:
    return ffi_from_char;

    case RBX_FFI_TYPE_UCHAR:
    return ffi_from_uchar;

    case RBX_FFI_TYPE_SHORT:
    return ffi_from_short;

    case RBX_FFI_TYPE_USHORT:
    return ffi_from_ushort;

    case RBX_FFI_TYPE_INT:
    return ffi_from_int;

    case RBX_FFI_TYPE_UINT:
    return ffi_from_uint;

    case RBX_FFI_TYPE_LONG:
    return ffi_from_long;

    case RBX_FFI_TYPE_ULONG:
    return ffi_from_ulong;

    case RBX_FFI_TYPE_LL:
    return ffi_from_ll;

    /* FIXME: have a real converter */
    case RBX_FFI_TYPE_ULL:
    return ffi_from_ull;

    case RBX_FFI_TYPE_FLOAT:
    return ffi_from_float;

    case RBX_FFI_TYPE_DOUBLE:
    return ffi_from_double;

    case RBX_FFI_TYPE_PTR:
    return ffi_from_ptr;

    case RBX_FFI_TYPE_VOID:
    return ffi_from_void;

    case RBX_FFI_TYPE_STRING:
    return ffi_from_string;

    case RBX_FFI_TYPE_STRPTR:
    return ffi_from_strptr;

    default:

    return NULL;
  }
}

OBJECT ffi_generate_typed_c_stub(STATE, int args, int *arg_types, int ret_type, void *func) {
  char *start; /* *end */
  void **code_start;
  char *codebuf; /*, *res */
  int i, reg; /* in, aligned, vars, size */
  int *ids;
  void *conv;
  int int_count, float_count, double_count;
  OBJECT obj;

  /* lightning only supports 6 arguments currently. */
  if(args > 6)
  {
    XFREE(arg_types);
    return Qnil;
  }
  int_count = 0;
  float_count = 0;
  double_count = 0;

  codebuf = ALLOC_N(char, 4096);
  start = codebuf;

  (void)jit_set_ip((jit_insn*)codebuf);

  jit_prolog(0);

  if(args > 0) {
    ids = ALLOC_N(int, args);

    for(i = 0; i < args; i++) {
      conv = ffi_get_to_converter(arg_types[i]);
      switch(arg_types[i]) {
      case RBX_FFI_TYPE_FLOAT:
        reg = JIT_FPR5;
        float_count++;
        break;
      case RBX_FFI_TYPE_DOUBLE:
        reg = JIT_FPR5;
        double_count++;
        break;
      default:
        reg = JIT_V1;
        int_count++;
        break;
      }

#define call_conv(kind) jit_prepare(0); (void)(jit_calli(conv)); jit_retval_ ## kind (reg); ids[i] = jit_allocai(ffi_get_alloc_size(arg_types[i]));

      switch(arg_types[i]) {
      case RBX_FFI_TYPE_CHAR:
      case RBX_FFI_TYPE_UCHAR:
        call_conv(c);
        jit_stxi_c(ids[i], JIT_FP, reg);
        break;
      case RBX_FFI_TYPE_SHORT:
      case RBX_FFI_TYPE_USHORT:
        call_conv(s);
        jit_stxi_s(ids[i], JIT_FP, reg);
        break;
      case RBX_FFI_TYPE_INT:
      case RBX_FFI_TYPE_UINT:
        call_conv(i);
        jit_stxi_i(ids[i], JIT_FP, reg);
        break;
      case RBX_FFI_TYPE_LONG:
      case RBX_FFI_TYPE_ULONG:
        call_conv(l);
        jit_stxi_l(ids[i], JIT_FP, reg);
        break;
      case RBX_FFI_TYPE_FLOAT:
        call_conv(f);
        jit_stxi_f(ids[i], JIT_FP, reg);
        break;
      case RBX_FFI_TYPE_DOUBLE:
        call_conv(d);
        jit_stxi_d(ids[i], JIT_FP, reg);
        break;
      case RBX_FFI_TYPE_OBJECT:
      case RBX_FFI_TYPE_PTR:
      case RBX_FFI_TYPE_STRING:
      case RBX_FFI_TYPE_STATE:
        call_conv(p);
        jit_stxi_p(ids[i], JIT_FP, reg);
      }
    }

    if(int_count > 0) jit_prepare_i(int_count);
    if(double_count > 0) jit_prepare_d(double_count);
    if(float_count > 0)  jit_prepare_f(float_count);

    for(i = args - 1; i >= 0; i--) {
      switch(arg_types[i]) {
      case RBX_FFI_TYPE_CHAR:
      case RBX_FFI_TYPE_UCHAR:
        jit_ldxi_c(JIT_R2, JIT_FP, ids[i]);
        jit_pusharg_c(JIT_R2);
        break;

      case RBX_FFI_TYPE_SHORT:
      case RBX_FFI_TYPE_USHORT:
        jit_ldxi_s(JIT_R2, JIT_FP, ids[i]);
        jit_pusharg_s(JIT_R2);
        break;

      case RBX_FFI_TYPE_INT:
      case RBX_FFI_TYPE_UINT:
        jit_ldxi_i(JIT_R2, JIT_FP, ids[i]);
        jit_pusharg_i(JIT_R2);
        break;

      case RBX_FFI_TYPE_LONG:
      case RBX_FFI_TYPE_ULONG:
        jit_ldxi_l(JIT_R2, JIT_FP, ids[i]);
        jit_pusharg_l(JIT_R2);
        break;

      case RBX_FFI_TYPE_FLOAT:
        jit_ldxi_f(JIT_FPR5, JIT_FP, ids[i]);
        jit_pusharg_f(JIT_FPR5);
        break;

      case RBX_FFI_TYPE_DOUBLE:
        jit_ldxi_d(JIT_FPR5, JIT_FP, ids[i]);
        jit_pusharg_d(JIT_FPR5);
        break;

      case RBX_FFI_TYPE_OBJECT:
      case RBX_FFI_TYPE_PTR:
      case RBX_FFI_TYPE_STRING:
      case RBX_FFI_TYPE_STATE:
      default:
        jit_ldxi_p(JIT_R2, JIT_FP, ids[i]);
        jit_pusharg_p(JIT_R2);
      }

    }
    XFREE(arg_types);
    XFREE(ids);
  }

#ifdef i386
# ifdef __linux__
  /* call finit to initialize the fpu. i don't know yet why this is
   * needed -- it seems something's corrupting the FPU registers.
   */
  _O(0x9b); _O(0xdb); _O(0xe3);
# endif
#endif

  jit_finish(func);

  switch(ret_type) {
  case RBX_FFI_TYPE_FLOAT:
  case RBX_FFI_TYPE_DOUBLE:
    reg = JIT_FPR0;
    break;
  default:
    reg = JIT_V1;
  }

  switch(ret_type) {
  case RBX_FFI_TYPE_CHAR:
  case RBX_FFI_TYPE_UCHAR:
    jit_retval_c(reg);
    jit_prepare_i(1);
    jit_pusharg_c(reg);
    break;

  case RBX_FFI_TYPE_SHORT:
  case RBX_FFI_TYPE_USHORT:
    jit_retval_s(reg);
    jit_prepare_i(1);
    jit_pusharg_s(reg);
    break;

  case RBX_FFI_TYPE_INT:
  case RBX_FFI_TYPE_UINT:
    jit_retval_i(reg);
    jit_prepare_i(1);
    jit_pusharg_i(reg);
    break;

  case RBX_FFI_TYPE_LONG:
  case RBX_FFI_TYPE_ULONG:
    jit_retval_l(reg);
    jit_prepare_i(1);
    jit_pusharg_l(reg);
    break;

  case RBX_FFI_TYPE_FLOAT:
    jit_retval_f(reg);
    jit_prepare_f(1);
    jit_pusharg_f(reg);
    break;

  case RBX_FFI_TYPE_DOUBLE:
    jit_retval_d(reg);
    jit_prepare_d(1);
    jit_pusharg_d(reg);
    break;

  case RBX_FFI_TYPE_VOID:
    jit_prepare_i(1);
    jit_movi_i(reg, 1);
    jit_pusharg_i(reg);
    break;

  case RBX_FFI_TYPE_OBJECT:
  case RBX_FFI_TYPE_PTR:
  case RBX_FFI_TYPE_STRING:
  case RBX_FFI_TYPE_STRPTR:
  default:
    jit_retval_p(reg);
    jit_prepare_i(1);
    jit_pusharg_p(reg);
  }

  conv = ffi_get_from_converter(ret_type);
  jit_finish(conv);
  /* probably don't need this, since the return value is void, but it
     doesn't hurt to clear it. */
  jit_movi_i(JIT_RET, 1);
  jit_ret();

  if(getenv("RBX_SHOW_STUBS")) {
    printf("Assembly stub for: %p (%d)\n", func, args);

    #ifdef RBS_DISASS
    disassemble(stderr, start, jit_get_ip().ptr);
    #endif
  }

  jit_flush_code(start, jit_get_ip().ptr);
/*  size = jit_get_ip().ptr - start;

  res = calloc(size, sizeof(char));
  memcpy(res, start, size);
  XFREE(codebuf);

  jit_flush_code(res, res + size);
*/
  // obj = object_memory_new_opaque(state, BASIC_CLASS(ffi_ptr), sizeof(void*));
  NEW_STRUCT(obj, code_start, BASIC_CLASS(ffi_ptr), void*);
  *code_start = (void*)start;
/*  *code_start = (void*)res;*/

  //memcpy(BYTES_OF(obj), &start, sizeof(void*));
  return obj;
}

OBJECT ffi_function_new(STATE, OBJECT ptr, OBJECT name, int args) {
  OBJECT nf;
  nf = nfunc_allocate(state);
  cmethod_set_primitive(nf, I2N(CPU_PRIMITIVE_NFUNC_CALL));
  cmethod_set_required(nf, I2N(args));
  cmethod_set_serial(nf, I2N(0));
  cmethod_set_name(nf, name);
  cmethod_set_file(nf, symtbl_lookup_cstr(state, state->global->symbols, "<system>"));
  nmethod_set_data(nf, ptr);

  return nf;
}

OBJECT ffi_libffi_generate(STATE, int arg_count, int *arg_types, int ret_type, void *func);

/* The main interface function, handles looking up the pointer in the library,
 * generating the stub, wrapping it up and attaching it to the module.
 */
OBJECT ffi_function_create(STATE, OBJECT library, OBJECT name, OBJECT args, OBJECT ret) {
  void *ep;
  int *arg_types;
  int ret_type;
  int i, tot, arg_count;
  OBJECT ptr, func, sym, type; /* meths; */

  ep = subtend_find_symbol(state, library, name);
  if(!ep) return Qnil;

  tot = N2I(array_get_total(args));
  arg_count = tot;
  /* We don't support more than 6 args currently. */
  if(tot > 6) {
    return Qnil;
  } else if(tot > 0) {
    arg_types = ALLOC_N(int, tot);

    for(i = 0; i < tot; i++) {
      type = array_get(state, args, i);
      if(!FIXNUM_P(type)) return Qnil;
      arg_types[i] = N2I(type);

      /* State can only be passed as the first arg, and it's invisible,
         ie doesn't get seen as in onbound arg by ruby. But it can ONLY
         be the first arg. */
      if(arg_types[i] == RBX_FFI_TYPE_STATE) {
        if(i == 0) {
          arg_count--;
        } else {
          XFREE(arg_types);
          printf("Invalid arg types.\n");
          return Qnil;
        }
      }
    }
  } else {
    arg_types = NULL;
  }

  ret_type = N2I(ret);

  ptr = ffi_libffi_generate(state, tot, arg_types, ret_type, ep);

#if 0

#if defined(__amd64__) || defined(__x86_64__) || defined(X86_64)
  ptr = ffi_amd64_generate_c_shim(state, tot, arg_types, ret_type, ep);
  XFREE(arg_types);
#elif defined(i386) // && defined(__linux__)
  ptr = ffi_x86_generate_c_shim(state, tot, arg_types, ret_type, ep);
  XFREE(arg_types);
#else
  ptr = ffi_generate_typed_c_stub(state, tot, arg_types, ret_type, ep);
#endif

#endif

  sym = string_to_sym(state, name);
  func = ffi_function_new(state, ptr, sym, arg_count);

  return func;
}

void ffi_call_stub(STATE, cpu c, OBJECT ptr) {
  nf_stub_ffi func;
  rni_context *ctx;

  ctx = subtend_retrieve_context();
  ctx->state = state;
  ctx->cpu = c;


  func = (nf_stub_ffi)(*DATA_STRUCT(ptr, void**));
  func();
}

typedef void (*nf_take_8)(uint8_t);
typedef void (*nf_take_16)(uint16_t);
typedef void (*nf_take_32)(uint32_t);
typedef void (*nf_take_64)(uint64_t);

typedef uint8_t (*nf_return_8)();
typedef uint16_t (*nf_return_16)();
typedef uint32_t (*nf_return_32)();
typedef uint64_t (*nf_return_64)();

typedef unsigned long (*nf_return_long)();
typedef unsigned long long (*nf_return_ll)();
typedef float (*nf_return_float)();
typedef double (*nf_return_double)();

OBJECT ffi_get_field(char *ptr, int offset, int type) {
  int sz;
  OBJECT ret;
  STATE = current_machine->s;

  ptr += offset;

#define READ(type) (*((type*)(ptr)))
  
  switch(type) {
  case RBX_FFI_TYPE_CHAR:
    ret = I2N(READ(char));
    break;
  case RBX_FFI_TYPE_UCHAR:
    ret = I2N(READ(unsigned char));
    break;
  case RBX_FFI_TYPE_SHORT:
    ret = I2N(READ(short));
    break;
  case RBX_FFI_TYPE_USHORT:
    ret = I2N(READ(unsigned short));
    break;
  case RBX_FFI_TYPE_INT:
    ret = I2N(READ(int));
    break;
  case RBX_FFI_TYPE_UINT:
    ret = I2N(READ(unsigned int));
    break;
  case RBX_FFI_TYPE_LONG:
    ret = I2N(READ(long));
    break;
  case RBX_FFI_TYPE_ULONG:
    ret = I2N(READ(unsigned long));
    break;
  case RBX_FFI_TYPE_FLOAT:
    ret = float_new(state, (double)READ(float));
    break;
  case RBX_FFI_TYPE_DOUBLE:
    ret = float_new(state, READ(double));
    break;
  case RBX_FFI_TYPE_LL:
    ret = LL2N(READ(long long));
    break;
  case RBX_FFI_TYPE_ULL:
    ret = ULL2N(READ(unsigned long long));
    break;
  case RBX_FFI_TYPE_OBJECT:
    ret = READ(OBJECT);
    break;
  case RBX_FFI_TYPE_PTR: {
    void *lptr = READ(void*);
    if(!lptr) {
      ret = Qnil;
    } else {
      ret = ffi_new_pointer(state, lptr);
    }
    break;
  }
  case RBX_FFI_TYPE_STRING: {
    char* result = READ(char*);
    if(result == NULL) {
      ret = Qnil;
    } else {
      ret = string_new(state, result);
    }
    break;
  }
  case RBX_FFI_TYPE_STRPTR: {
    char* result;
    OBJECT s, p;

    result = READ(char*);
    
    if(result == NULL) {
      s = p = Qnil;
    } else {
      p = ffi_new_pointer(state, result);
      s = string_new(state, result);
    }
    
    ret = array_new(state, 2);
    array_set(state, ret, 0, s);
    array_set(state, ret, 1, p);
    break;
  }
  default:
  case RBX_FFI_TYPE_VOID:
    ret = Qnil;
    break;
  }

  return ret;
}

void ffi_set_field(char *ptr, int offset, int type, OBJECT val) {
  int sz;
  STATE = current_machine->s;
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;

  ptr += offset;
  
#define WRITE(type, val) *((type*)ptr) = (type)val

  switch(type) {
  case RBX_FFI_TYPE_CHAR:
    type_assert(val, FixnumType, "converting to char");
    WRITE(char, N2I(val));
    break;
  case RBX_FFI_TYPE_UCHAR:
    type_assert(val, FixnumType, "converting to unsigned char");
    WRITE(unsigned char, N2I(val));
    break;
  case RBX_FFI_TYPE_SHORT:
    type_assert(val, FixnumType, "converting to short");
    WRITE(short, N2I(val));
    break;
  case RBX_FFI_TYPE_USHORT:
    type_assert(val, FixnumType, "converting to unsigned short");
    WRITE(unsigned short, N2I(val));
    break;
  case RBX_FFI_TYPE_INT:
    if(FIXNUM_P(val)) {
      WRITE(int, N2I(val));
    } else {
      type_assert(val, BignumType, "converting to int");
      WRITE(int, bignum_to_i(state, val));
    }
    break;
  case RBX_FFI_TYPE_UINT:
    if(FIXNUM_P(val)) {
      WRITE(unsigned int, N2I(val));
    } else {
      type_assert(val, BignumType, "converting to unsigned int");
      WRITE(unsigned int, bignum_to_i(state, val));
    }
    break;
  case RBX_FFI_TYPE_LONG:
    if(FIXNUM_P(val)) {
      WRITE(long, N2I(val));
    } else {
      type_assert(val, BignumType, "converting to long");
      if(sizeof(long) == sizeof(long long)) {
        WRITE(long, bignum_to_ll(state, val));
      } else {
        WRITE(long, bignum_to_i(state, val));
      }
    }
    break;
  case RBX_FFI_TYPE_ULONG:
    if(FIXNUM_P(val)) {
      WRITE(unsigned long, N2I(val));
    } else {
      type_assert(val, BignumType, "converting to long");
      if(sizeof(long) == sizeof(long long)) {
        WRITE(unsigned long, bignum_to_ll(state, val));
      } else {
        WRITE(unsigned long, bignum_to_i(state, val));
      }
    }
    break;
  case RBX_FFI_TYPE_FLOAT:
    type_assert(val, FloatType, "converting to float");
    WRITE(float, FLOAT_TO_DOUBLE(val));
    break;
  case RBX_FFI_TYPE_DOUBLE:
    type_assert(val, FloatType, "converting to double");
    WRITE(double, FLOAT_TO_DOUBLE(val));
    break;
  case RBX_FFI_TYPE_LL:
    if(FIXNUM_P(val)) {
      WRITE(long long, N2I(val));
    } else {
      type_assert(val, BignumType, "converting to long long");
      WRITE(long long, bignum_to_ll(state, val));
    }
    break;
  case RBX_FFI_TYPE_ULL:
    if(FIXNUM_P(val)) {
      WRITE(unsigned long long, N2I(val));
    } else {
      type_assert(val, BignumType, "converting to unsigned long long");
      WRITE(unsigned long long, bignum_to_ll(state, val));
    }
    break;
  case RBX_FFI_TYPE_OBJECT:
    WRITE(OBJECT, val);
    break;
  case RBX_FFI_TYPE_PTR:
    if(NIL_P(val)) {
      WRITE(void*, NULL);
    } else {
      type_assert(val, MemPtrType, "converting to pointer");
      WRITE(void*, *DATA_STRUCT(val, void**));
    }
    break;
  case RBX_FFI_TYPE_STRING: {
    char* result;
    if(NIL_P(val)) {
      result = NULL;
    } else {
      type_assert(val, StringType, "converting to C string");
      result = string_byte_address(state, val);
    }
    WRITE(char*, result);
    break;
  }
  default:
    sassert(0);
  }
}
