int ffi_type_size(int type);
void Init_ffi(STATE);
OBJECT ffi_function_create(STATE, OBJECT library, OBJECT name, OBJECT args, OBJECT ret);

OBJECT ffi_new_pointer(STATE, void *ptr);
void ffi_autorelease(OBJECT ptr, int ar);
#define ffi_pointer(ptr) (*DATA_STRUCT(ptr, void**))

#define RBX_FFI_TYPE_OBJECT  0
#define RBX_FFI_TYPE_CHAR    1
#define RBX_FFI_TYPE_UCHAR   2
#define RBX_FFI_TYPE_SHORT   3
#define RBX_FFI_TYPE_USHORT  4
#define RBX_FFI_TYPE_INT     5
#define RBX_FFI_TYPE_UINT    6
#define RBX_FFI_TYPE_LONG    7
#define RBX_FFI_TYPE_ULONG   8
#define RBX_FFI_TYPE_LL      9
#define RBX_FFI_TYPE_ULL     10
#define RBX_FFI_TYPE_FLOAT   11
#define RBX_FFI_TYPE_DOUBLE  12
#define RBX_FFI_TYPE_PTR     13
#define RBX_FFI_TYPE_VOID    14
#define RBX_FFI_TYPE_STRING  15
#define RBX_FFI_TYPE_STATE   16
#define RBX_FFI_TYPE_STRPTR  17
#define RBX_FFI_TYPE_CHARARR 18

#define NFUNC_FIELDS 7

#define nfunc_allocate(st) (object_memory_new_object_mature(st->om, st->global->ffi_func, NFUNC_FIELDS))
#define nfunc_get_data(obj) cmethod_get_bytecodes(obj)
#define nfunc_set_data(obj, data) cmethod_set_bytecodes(obj, data)
