#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/symbol.h"
#include "shotgun/lib/object.h"
#include "shotgun/lib/string.h"
#include "shotgun/lib/hash.h"
#include "shotgun/lib/primitive_indexes.h"

#include "shotgun/lib/subtend/ffi_amd64.h"
#include "shotgun/lib/subtend/nmethod.h"
#include "shotgun/lib/subtend/nmc.h"
#include "shotgun/lib/subtend/ffi.h"
#include "shotgun/lib/subtend/library.h"

/* Arguments passed in this order: %rdi, %rsi, %rdx, %rcx, %r8, %r9, stack */
static int ffi_amd64_reg_offset[] = {FFI_RDI, FFI_RSI, FFI_RDX, FFI_RCX, FFI_R8, FFI_R9}; 

/*
 * Generate a shim or wrapper around a C function.
 *
 * Produces an intermediary function directly into the
 * process at runtime using a hand-built machine code.
 * The creator provides the information about the 
 * function to call and the order and types of its
 * arguments (max 6) as well as its return type.
 *
 * The result is wrapped in an opaque Rubinius struct
 * so that it can be treated as an object.
 */ 
OBJECT ffi_amd64_generate_c_shim(STATE, 
                                 int arg_count, int* arg_types, 
                                 int ret_type, 
                                 void* func) 
{
  OBJECT  obj; 
  char*   start;
  char*   code; 
  void**  code_ptr; 
  void*   converter; 
  int     i, offset, reg_count, xmm_count, flags = MAP_PRIVATE;
      
  /* Only 6 arguments currently */
  if(arg_count > 6) return Qnil;

#ifdef MAP_ANONYMOUS
  flags |= MAP_ANONYMOUS;
#else
  flags |= MAP_ANON;
#endif

  start = mmap(NULL, FFI_CODE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
               flags, -1, 0);
  code = start;

  /* Function prolog */
  BYTE(0x55);                                    /* push %rbp */ 
  BYTE(0x48); BYTE(0x89); BYTE(0xe5);            /* mov %rsp, %rbp */
  BYTE(0x48); BYTE(0x83); BYTE(0xec); BYTE(96);  /* sub $96, %rsp */ 


  /* Byte offset from base pointer */
  offset = 0xfe; 

  /* These keep track of register positions needed */
  reg_count = 0; 
  xmm_count = 0; 

  /* Retrieve arguments from the Rubinius stack to our stack */
  for (i = 0 ; i < arg_count; ++i) {
    converter = ffi_get_to_converter(arg_types[i]);

    BYTE(0x48); BYTE(0xba); QWORD(converter);    /* mov converter, %rdx */
    BYTE(0xff); BYTE(0xd2);                      /* callq *%rdx */ 

    switch (arg_types[i]) { 
      case RBX_FFI_TYPE_FLOAT:
        /* movss %xmm0, offset(%rbp) */
        offset -= 4; 
        BYTE(0xf3); BYTE(0x0f); BYTE(0x11); BYTE(0x45); BYTE(offset); 
        ++xmm_count; 
        break; 
        
      case RBX_FFI_TYPE_DOUBLE:
        /* movsd %xmm0, offset(%rbp) */
        offset -= 8; 
        BYTE(0xf2); BYTE(0x0f); BYTE(0x11); BYTE(0x45); BYTE(offset);
        ++xmm_count; 
        break; 

      default:
        /* push %rax */
        BYTE(0x50 + FFI_RAX);
        ++reg_count; 
        break; 
    }
  }

  /* Load arguments back from the stack to the appropriate registers */
  for (i = arg_count - 1; i >= 0; --i) {
    switch (arg_types[i]) { 
      case RBX_FFI_TYPE_FLOAT:
        /* movss offset(%rbp), %xmmN ; each %xmmN increment is + 8 */
        BYTE(0xf3); BYTE(0x0f); BYTE(0x10); BYTE(0x45 + (--xmm_count * 8)); BYTE(offset);
        offset += 4; 
        break; 
        
      case RBX_FFI_TYPE_DOUBLE:
        /* movsd offset(%rbp), %xmmN */
        BYTE(0xf2); BYTE(0x0f); BYTE(0x10); BYTE(0x45 + (--xmm_count * 8)); BYTE(offset);
        offset += 8; 
        break; 

      default:
        /* pop %<register> */
        BYTE(0x58 + ffi_amd64_reg_offset[--reg_count]); 
        break; 
    }
  }

  /* Call the given function */
  BYTE(0x48); BYTE(0xb8); QWORD(func);           /* mov func, %rax ; %rdx may be taken */
  BYTE(0xff); BYTE(0xd0);                        /* callq *%rax */ 

  /* Set up return value processing */
  switch (ret_type) {
    case RBX_FFI_TYPE_VOID:
      BYTE(0x48); BYTE(0xbf); QWORD(1);          /* mov $1, %rdi ; dummy */
      break; 

    /* Floats and doubles are already in %xmm0 */
    case RBX_FFI_TYPE_FLOAT:
    case RBX_FFI_TYPE_DOUBLE:
      break;

    default:
      BYTE(0x48); BYTE(0x89); BYTE(0xc7);        /* mov %rax, %rdi */
      break; 
  }

  converter = ffi_get_from_converter(ret_type); 

  /* Send return value back to rbx */
  BYTE(0x48); BYTE(0xba); QWORD(converter);      /* mov converter, %rdx */
  BYTE(0xff); BYTE(0xd2);                        /* callq *%rdx */
  
  /* Clean up */
  BYTE(0xc9);                                    /* leaveq */
  BYTE(0xc3);                                    /* retq */

  NEW_STRUCT(obj, code_ptr, BASIC_CLASS(ffi_ptr), void*);
  *code_ptr = (void*)start;   /* Stash away the pointer */

  return obj;
}
