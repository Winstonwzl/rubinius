#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/subtend/ffi.h"

#define FFI_CODE_SIZE 256

/* Macros for writing bytes or doublewords.
 * They assume the existence of a variable 'code'.
 */
#define BYTE(byte) \
  *code++ = (uint8_t)(byte)

#define DWORD(dword) \
  do { \
    uint32_t *tmp = (uint32_t *)code; \
    *tmp = (dword); \
    code += 4; \
  } while (0);

/* These are defined in /usr/include/sys/regset.h on Solaris 10. */
#ifdef __sun__
#undef EAX
#undef ECX
#undef EDX
#undef EBX
#undef ESP
#undef EBP
#undef ESI
#undef EDI
#endif

enum {
  EAX = 0,
  ECX,
  EDX,
  EBX,
  ESP,
  EBP,
  ESI,
  EDI
};

/* A big enough frame to fit 6 args */
#define FRAMESIZE 48

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
OBJECT ffi_x86_generate_c_shim(STATE, int arg_count, int *arg_types,
                               int ret_type, void *func)
{
  OBJECT obj;
  char *start, *code;
  void *converter, **code_ptr;
  uint32_t rel_addr;
  int i, flags = MAP_PRIVATE;
  int extra, arg_stack_bytes = 0;
  uint8_t offset;

  /* Only 6 arguments currently */
  if (arg_count > 6)
    return Qnil;

#ifdef MAP_ANONYMOUS
  flags |= MAP_ANONYMOUS;
#else
  flags |= MAP_ANON;
#endif

  code = start = mmap(NULL, FFI_CODE_SIZE,
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      flags, -1, 0);

  /* This is compliant with OS X by making sure that the stack
   * is 16 byte aligned at each call point. */

  /* Function prolog */
  BYTE(0x50 + EBP);                  /* push %ebp */
  BYTE(0x89); BYTE(0xe5);            /* movl %esp, %ebp */
  BYTE(0x83); BYTE(0xec); BYTE(FRAMESIZE + 8);  /* subl $<val>, %esp */

  /* The +8 is to keep the stack aligned and compinsate for eip and
   * ebp on the stack.. */

  /* Byte offset from base pointer */
  offset = 0;

  /* For each argument we're now calling ffi_to_TYPE().
   * Those functions will pop the argument off the Rubinius stack and
   * return them as a C value.
   * That return value will be on the top of the FPU stack if it's a
   * float or a double. If it's a 64 bit value, it will be in %eax and
   * %edx, or just in %eax otherwise.
   * We then store that converted value on our stack.
   */
  for (i = 0; i < arg_count; i++) {
    converter = ffi_get_to_converter(arg_types[i]);

    /* call converter */
    rel_addr = ((char *) converter) - code - 5;
    BYTE(0xe8); DWORD(rel_addr);

    switch (arg_types[i]) {
      case RBX_FFI_TYPE_FLOAT:
        arg_stack_bytes += 4;

        /* fstp offset(%ebp) */
        offset += 4;
        BYTE(0xd9); BYTE(0x5d); BYTE(-offset);
        break;
      case RBX_FFI_TYPE_DOUBLE:
        arg_stack_bytes += 8;
        
        /* fstpl offset(%ebp) */
        offset += 8;
        BYTE(0xdd); BYTE(0x5d); BYTE(-offset);
        break;
      case RBX_FFI_TYPE_LL:
      case RBX_FFI_TYPE_ULL:
        arg_stack_bytes += 4;
        
        offset += 4;
        BYTE(0x89); BYTE(0x55); BYTE(-offset); /* movl %edx, offset(%ebp) */
        /* fall through */
      default:
        arg_stack_bytes += 4;
        
        offset += 4;
        BYTE(0x89); BYTE(0x45); BYTE(-offset); /* movl %eax, offset(%ebp) */
        break;
    }
  }
 
  /* Add a little extra to the stack to keep the call points aligned. */
  if(arg_stack_bytes % 16 != 0) {
    extra = 16 - (arg_stack_bytes % 16);
    BYTE(0x83); BYTE(0xec); BYTE(extra);  /* subl extra, %esp */
  } else {
    extra = 0;
  }

  /* Load arguments from our stack and push them on the stack as the
   * given function's arguments.
   */
  for (i = arg_count - 1; i >= 0; i--) {
    /* As before, we only need to differentiate between 64 bit and
     * 32 bit values. No special handling for floating point numbers
     * is necessary, as these are just bytes on the stack now.
     */
    switch (ffi_type_size(arg_types[i])) {
      case 8:
        BYTE(0xff); BYTE(0x75); BYTE(-offset + 4); /* push (offset+4)(%ebp) */
        BYTE(0xff); BYTE(0x75); BYTE(-offset);     /* push (offset)(%ebp) */
        offset -= 8;
        break;
      default:
        BYTE(0xff); BYTE(0x75); BYTE(-offset);     /* push (offset)(%ebp) */
        offset -= 4;
        break;
    }
  }

  /* call the given function */
  rel_addr = ((char *) func) - code - 5;
  BYTE(0xe8); DWORD(rel_addr);

  /* Remove the padding. */
  /* subl -(arg_stack_bytes + extra), %esp */
  BYTE(0x83); BYTE(0xec); BYTE(-(arg_stack_bytes + extra));

  /* Add some alignment padding. */
  switch(ret_type) {
  case RBX_FFI_TYPE_DOUBLE:
  case RBX_FFI_TYPE_LL:
  case RBX_FFI_TYPE_ULL:
    extra = 8;
    break;
  default:
    extra = 12;
  }

  BYTE(0x83); BYTE(0xec); BYTE(extra);  /* subl extra, %esp */

  /* Set up return value processing */
  switch (ret_type) {
    case RBX_FFI_TYPE_VOID:
      BYTE(0x6a); BYTE(0x01); /* push $1 ; dummy */
      break;
    case RBX_FFI_TYPE_FLOAT:
      /* fstp offset(%ebp) */
      offset += 4;
      BYTE(0xd9); BYTE(0x5d); BYTE(-offset);

      BYTE(0xff); BYTE(0x75); BYTE(-offset);     /* push offset(%ebp) */
      break;
    case RBX_FFI_TYPE_DOUBLE:
      /* fstpl offset(%ebp) */
      offset += 8;
      BYTE(0xdd); BYTE(0x5d); BYTE(-offset);

      BYTE(0xff); BYTE(0x75); BYTE(-offset + 4); /* push (offset+4)(%ebp) */
      BYTE(0xff); BYTE(0x75); BYTE(-offset);     /* push offset(%ebp) */
      break;
    case RBX_FFI_TYPE_LL:
    case RBX_FFI_TYPE_ULL:
      BYTE(0x50 + EDX); /* push %edx */
      /* fall through */
    default:
      BYTE(0x50 + EAX); /* push %eax */
      break;
  }

  converter = ffi_get_from_converter(ret_type);

  /* Send return value back to rbx */
  rel_addr = ((char *) converter) - code - 5;
  BYTE(0xe8); DWORD(rel_addr);

  /* Remove alignment padding. */
  BYTE(0x83); BYTE(0xec); BYTE(-extra);  /* subl -extra, %esp */

  /* Clean up */
  BYTE(0xc9); /* leave */
  BYTE(0xc3); /* ret */

  NEW_STRUCT(obj, code_ptr, BASIC_CLASS(ffi_ptr), void *);
  *code_ptr = start; /* Stash away the pointer */

  /* drop the writable flag */
  mprotect(start, FFI_CODE_SIZE, PROT_READ | PROT_EXEC);

  return obj;
}
