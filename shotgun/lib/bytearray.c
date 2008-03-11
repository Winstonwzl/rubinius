#include <stdlib.h>
#include <string.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/object.h"
#include "shotgun/lib/bytearray.h"

OBJECT bytearray_new(STATE, unsigned int size) {
  unsigned int words;
  OBJECT obj;
  
  words = size / SIZE_OF_OBJECT;
  if(size % SIZE_OF_OBJECT != 0) {
    words += 1;
  }
  
  obj = bytearray_allocate_with_extra(state, words);
  object_make_byte_storage(state, obj);
  fast_memfill(BYTES_OF(obj), 0, words);
  object_initialize_bytes(state, obj);
  return obj;
}

OBJECT bytearray_new_dirty(STATE, unsigned int size) {
  unsigned int words;
  OBJECT obj;
    
  words = size / SIZE_OF_OBJECT;
  if(size % SIZE_OF_OBJECT != 0) {
    words += 1;
  }
    
  obj = object_memory_new_dirty_object(state->om, BASIC_CLASS(bytearray), words);
  object_make_byte_storage(state, obj);
  
  return obj;
}

OBJECT bytearray_dup(STATE, OBJECT self) {
  OBJECT obj;
  int words = NUM_FIELDS(self);
  
  obj = object_memory_new_dirty_object(state->om, BASIC_CLASS(bytearray), words);
  object_make_byte_storage(state, obj);
  
  object_copy_body(state, self, obj);
  
  return obj;
}

char *bytearray_as_string(STATE, OBJECT self) {
  char *str;
  char *out;
  unsigned int sz;
  
  str = (char*)bytearray_byte_address(state, self);
  
  sz = object_size(state, self);
  out = ALLOC_N(char, sz);
  memcpy(out, str, sz);
  
  return out;
}

OBJECT iseq_new(STATE, unsigned int sz) {
  OBJECT obj;
  int fields;

  fields = sz / SIZE_OF_OBJECT;
  if(sz % SIZE_OF_OBJECT != 0) {
    fields += 1;
  }
  obj = NEW_OBJECT(state->global->iseq, fields);
  object_make_byte_storage(state, obj);
  
  return obj;
}

static inline uint32_t read_int_from_be(uint8_t *str) {
  return (uint32_t)((str[0] << 24)
                  | (str[1] << 16)
                  | (str[2] << 8 )
                  |  str[3]      );
}

void iseq_flip(STATE, OBJECT self, OBJECT output) {
  uint8_t *buf;
  uint32_t *ibuf;
  uint32_t val;
  int i, f;
  
  f = object_size(state, self);
  buf = (uint8_t*)bytearray_byte_address(state, self);
  ibuf = (uint32_t*)bytearray_byte_address(state, output);
  
  /* A sanity check. The first thing is always an instruction,
   * and we've got less that 1024 instructions, so if it's less
   * it's already been flipped. */
  if(*(uint32_t*)buf < 1024) return;

  for(i = 0; i < f; i += 4, ibuf++) {
    val = read_int_from_be(buf + i);
    *ibuf = val;
  }
}
