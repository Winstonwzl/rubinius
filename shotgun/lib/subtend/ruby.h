#ifndef __RUBY_H
#define __RUBY_H

#define RUBY

#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

/* Pointers are seen as totally opaque */
typedef void * VALUE;
#define ID uintptr_t

/* We need to redefine those to casts to VALUE */
#undef Qfalse
#undef Qtrue
#undef Qnil
#undef Qundef

#define Qfalse ((VALUE)6L)
#define Qtrue  ((VALUE)10L)
#define Qnil   ((VALUE)14L)
#define Qundef ((VALUE)18L)

#undef RTEST
#undef NIL_P
#define RTEST(v) (((uintptr_t)(v) & 0x7) != 0x6)
#define NIL_P(v) (v == Qnil)
#define SYM2ID(sym) ((ID)(sym))
#define ID2SYM(id) ((VALUE)(id))

#undef ALLOC_N
#define ALLOC_N(kind, many) (kind*)malloc(sizeof(kind) * many)

#ifndef SYMBOL_P
int SYMBOL_P(VALUE obj);
#endif

extern VALUE rb_funcall(VALUE, ID, int cnt, ...);
extern VALUE rb_funcall2(VALUE, ID, int cnt, VALUE*);


extern VALUE subtend_get_global(int which);
extern VALUE subtend_get_exception(int which);

void rb_define_method_(const char *file, VALUE vmod, const char *name, void *func, int args, int kind);
void rb_define_module_function(VALUE vmod, const char *name, void *func, int args);

void rb_include_module(VALUE parent, VALUE module);

#define rb_define_method(a, b, c, d) rb_define_method_(__FILE__, a, b, c, d, 0)
#define rb_define_private_method(a, b, c, d) rb_define_method_(__FILE__, a, b, c, d, 1)
#define rb_define_protected_method(a, b, c, d) rb_define_method_(__FILE__, a, b, c, d, 2)
#define rb_define_singleton_method(a, b, c, d) rb_define_method_(__FILE__, a, b, c, d, 3)
void rb_define_alloc_func(VALUE class, void *func);

VALUE rb_define_class(const char *name, VALUE super);
VALUE rb_define_class_under(VALUE parent, const char *name, VALUE super);

VALUE rb_define_module(const char *name);
VALUE rb_define_module_under(VALUE parent, const char *name);

VALUE rb_const_get(VALUE klass, ID id);
void rb_define_const(VALUE klass, const char* key, VALUE val);
int rb_const_defined(VALUE klass, ID id);

VALUE rb_ivar_get(VALUE obj, ID sym);
VALUE rb_iv_get(VALUE obj, char *name);
VALUE rb_ivar_set(VALUE obj, ID sym, VALUE val);
VALUE rb_iv_set(VALUE obj, char *name, VALUE val);
void rb_define_attr(VALUE klass, ID id, int read, int write);
VALUE rb_attr_get(VALUE obj, ID sym);

int rb_block_given_p();
VALUE rb_each(VALUE obj);

#define rb_cObject (subtend_get_global(0))
#define rb_mKernel rb_const_get(rb_cObject, rb_intern("Kernel") )

/* TODO: Pull these into an enum */
#define rb_eException          subtend_get_exception(0)
#define rb_eSystemExit         subtend_get_exception(1)
#define rb_eInterrupt          subtend_get_exception(2)
#define rb_eSignal             subtend_get_exception(3)
#define rb_eFatal              subtend_get_exception(4) /* WTF is this? */
#define rb_eStandardError      subtend_get_exception(5)
#define rb_eRuntimeError       subtend_get_exception(6)
#define rb_eTypeError          subtend_get_exception(7)
#define rb_eArgError           subtend_get_exception(8)
#define rb_eIndexError         subtend_get_exception(9)
#define rb_eRangeError         subtend_get_exception(10)
#define rb_eNameError          subtend_get_exception(11)
#define rb_eNoMethodError      subtend_get_exception(12)
#define rb_eSecurityError      subtend_get_exception(13)
#define rb_eNotImpError        subtend_get_exception(14)
#define rb_eNoMemError         subtend_get_exception(15)

#define rb_eScriptError        subtend_get_exception(16)
#define rb_eSyntaxError        subtend_get_exception(17)
#define rb_eLoadError          subtend_get_exception(18)

#define rb_eSystemCallError    subtend_get_exception(19)
#define rb_mErrno              subtend_get_exception(20)
#define rb_eIOError            subtend_get_exception(21)
#define rb_eLocalJumpError     subtend_get_exception(22)

void rb_raise(VALUE exc, const char *fmt, ...);

VALUE rb_require(const char *name);

VALUE rb_obj_alloc(VALUE klass);
void rb_obj_call_init(VALUE obj, int nargs, VALUE *args);
VALUE rb_obj_is_instance_of(VALUE obj, VALUE klass);
VALUE rb_obj_is_kind_of(VALUE obj, VALUE klass);
int rb_respond_to(VALUE obj, ID sym);
ID rb_to_id(VALUE obj);

VALUE rb_yield(VALUE val);

int rb_safe_level();
void rb_secure(int level);
void rb_set_safe_level(int newlevel);

VALUE rb_gv_set(const char *name, VALUE value);
VALUE rb_gv_get(const char *name);

VALUE rb_check_array_type(VALUE ary);
VALUE rb_check_string_type(VALUE str);
VALUE rb_check_convert_type(VALUE val, int type, const char* tname, const char* method);

VALUE rb_class_new_instance(int nargs, VALUE *args, VALUE klass);

void rb_thread_schedule();
#define CHECK_INTS
void rb_secure(int);

/* Conversions */
int FIX2INT(VALUE val);
VALUE INT2NUM(int num);
#define INT2FIX(v) INT2NUM(v)
#define NUM2INT(val) FIX2INT(val)

/* Array */
VALUE rb_Array(VALUE val);
VALUE rb_ary_new(void);
VALUE rb_ary_new2(long length);
int rb_ary_size(VALUE self);
VALUE rb_ary_push(VALUE array, VALUE val);
VALUE rb_ary_pop(VALUE array);
VALUE rb_ary_entry(VALUE array, int offset);
VALUE rb_ary_clear(VALUE array);
VALUE rb_ary_dup(VALUE array);
VALUE rb_ary_join(VALUE array1, VALUE array2);
VALUE rb_ary_reverse(VALUE array);
VALUE rb_ary_unshift(VALUE array, VALUE val);
VALUE rb_ary_shift(VALUE array);
void rb_ary_store(VALUE array, int offset, VALUE val);

/* String */
VALUE rb_str_new(const char *ptr, long len);
VALUE rb_str_new2(const char *ptr);
VALUE rb_str_dup(VALUE str);
VALUE rb_str_append(VALUE str, VALUE str2);
VALUE rb_str_cat(VALUE str, const char *ptr, long len);
VALUE rb_str_plus(VALUE str1, VALUE str2);
VALUE rb_str_buf_cat(VALUE str, const char *ptr, long len);
VALUE rb_str_cmp(VALUE str1, VALUE str2);
VALUE rb_str_split(VALUE str, const char *sep);
VALUE rb_str2inum(VALUE str, int base);
VALUE rb_cstr2inum(VALUE str, int base);
VALUE rb_str_substr(VALUE str, long beg, long len);
VALUE rb_tainted_str_new2(const char *ptr);
char *StringValuePtr(VALUE str);
VALUE rb_obj_as_string(VALUE obj);
char rb_str_get_char(VALUE arg, int index);

void rb_string_value(VALUE *obj);
#define StringValue(v) rb_string_value(&v)
#define SafeStringValue StringValue

/* Hash */
VALUE rb_hash_new(void);
VALUE rb_hash_aref(VALUE hash, VALUE key);
VALUE rb_hash_aset(VALUE hash, VALUE key, VALUE val);
VALUE rb_hash_delete(VALUE hash, VALUE key);

/* Float */
VALUE rb_float_new(double d);

const char *rb_id2name(ID sym);
ID rb_intern(const char *name);
const char *rb_class2name(VALUE klass);

#define rb_obj_freeze(obj) obj

/* RSTRING Macro Replacement */

char* rb_str_get_char_ptr(VALUE arg);
void rb_str_flush_char_ptr(VALUE arg, char* ptr);
int rb_str_get_char_len(VALUE arg);

VALUE subtend_wrap_struct(VALUE klass, void *struct_value, void *mark_func, void *free_func);
void* subtend_get_struct(VALUE obj);

// TODO: Make this do Check_Type as well
#define Data_Get_Struct(obj, type, sval) do { sval = (type *)subtend_get_struct(obj); } while (0)
#define Data_Wrap_Struct(klass, mark, free, sval) subtend_wrap_struct(klass, sval, mark, free)

#endif
