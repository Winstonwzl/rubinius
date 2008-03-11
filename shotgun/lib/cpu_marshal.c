#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/cpu.h"
#include "shotgun/lib/string.h"
#include "shotgun/lib/bytearray.h"
#include "shotgun/lib/symbol.h"
#include "shotgun/lib/tuple.h"
#include "shotgun/lib/bignum.h"
#include "shotgun/lib/float.h"
#include "shotgun/lib/sha1.h"
#include "shotgun/lib/sendsite.h"

#include "shotgun/lib/primitive_util.h"

struct marshal_state {
  int consumed;
  ptr_array objects;
  uint8_t *buf;
};

static int _find_object(OBJECT obj, struct marshal_state *ms) {
  int i;
  return -1;
  for(i = 0; i < ptr_array_length(ms->objects); i++) {
    if(obj == (OBJECT)ptr_array_get_index(ms->objects, i)) {
      return i;
    }
  }
  
  return -1;
}

static void _add_object(OBJECT obj, struct marshal_state *ms) {
  ptr_array_append(ms->objects, (xpointer)obj);
}

static OBJECT unmarshal(STATE, struct marshal_state *ms);
static void   marshal(STATE, OBJECT obj, bstring buf, struct marshal_state *ms);

static void int2be(unsigned int i, unsigned char bytes[4]) {
  bytes[0] = ( i >> 24 ) & 0xff;
  bytes[1] = ( i >> 16 ) & 0xff;
  bytes[2] = ( i >> 8  ) & 0xff;
  bytes[3] = i & 0xff;  
}

#define append_c(ch) bconchar(buf, ch)
static void _append_sz(bstring buf, unsigned int i) {
  unsigned char bytes[4];
  int2be(i, bytes);
  bcatblk(buf, bytes, 4);
}
#define append_sz(sz) _append_sz(buf, (unsigned int)sz)
#define append_str(str, sz) bcatblk(buf, str, sz)

static int read_int(uint8_t *str) {
  return (int)( (str[0] << 24)
              | (str[1] << 16)
              | (str[2] << 8 )
              |  str[3]      );
}

static OBJECT _nth_object(STATE, struct marshal_state *ms) {
  int ref;
  
  ms->consumed += 5;
  ref = read_int(ms->buf + 1);
  return (OBJECT)ptr_array_get_index(ms->objects, ref);
}

static OBJECT unmarshal_int(STATE, struct marshal_state *ms) {
  int i;
  ms->consumed += 6;
  i = read_int(ms->buf + 2);
  if(ms->buf[1] == 'n') {
    i = -i;
  }
  return I2N(i);
}

static void marshal_str(STATE, OBJECT obj, bstring buf) {
  int i;
  i = N2I(string_get_bytes(obj));
  append_c('s');
  append_sz(i);
  append_str(string_byte_address(state, obj), i);
}

static OBJECT unmarshal_str(STATE, struct marshal_state *ms) {
  int sz;
  sz = read_int(ms->buf + 1);
  ms->consumed += 5;
  ms->consumed += sz;
  return string_new2(state, (char *) ms->buf + 5, sz);
}

static void marshal_sym(STATE, OBJECT obj, bstring buf) {
  OBJECT str;
  int i;
  str = symtbl_find_string(state, state->global->symbols, obj);
  i = N2I(string_get_bytes(str));
  append_c('x');
  append_sz(i);
  append_str(string_byte_address(state, str), i);
}

static OBJECT unmarshal_sym(STATE, struct marshal_state *ms) {
  int sz;
  
  sz = read_int(ms->buf + 1);
  ms->consumed += 5;
  ms->consumed += sz;
  
  return symtbl_lookup_str_with_size(state, state->global->symbols,
                                     (char *) ms->buf + 5, sz);
}

static void marshal_sendsite(STATE, OBJECT obj, bstring buf) {
  OBJECT str;
  int i;
  str = symtbl_find_string(state, state->global->symbols, SENDSITE(obj)->name);
  i = N2I(string_get_bytes(str));
  append_c('S');
  append_sz(i);
  append_str(string_byte_address(state, str), i);
}

static OBJECT unmarshal_sendsite(STATE, struct marshal_state *ms) {
  int sz;
  OBJECT sym;
  
  sz = read_int(ms->buf + 1);
  ms->consumed += 5;
  ms->consumed += sz;
  
  sym = symtbl_lookup_str_with_size(state, state->global->symbols,
                                     (char *) ms->buf + 5, sz);
  
  return send_site_create(state, sym);
}

static void marshal_fields_as(STATE, OBJECT obj, bstring buf, char type, struct marshal_state *ms) {
  int sz, i;
  sz = NUM_FIELDS(obj);
  append_c(type);
  append_sz(sz);
  for(i = 0; i < sz; i++) {
    marshal(state, NTH_FIELD(obj, i), buf, ms);
  }
}

static OBJECT unmarshal_into_fields(STATE, int sz, OBJECT tup, struct marshal_state *ms) {
  int i, j, cur;
  OBJECT o;
  cur = ms->consumed;
  for(i = 0; i < sz; i++) {
    uint8_t *old = ms->buf;

    o = unmarshal(state, ms);
    j = ms->consumed - cur;
    ms->buf = old + j;
    cur = ms->consumed;
    SET_FIELD(tup, i, o);
  }
  
  return tup;
}

static int unmarshal_num_fields(struct marshal_state *ms) {
  int i;
  i = read_int(ms->buf + 1);

  ms->consumed += 5;
  ms->buf += 5;

  return i;
}

static void marshal_ary(STATE, OBJECT obj, bstring buf, struct marshal_state *ms) {
  int sz, i;
  sz = N2I(array_get_total(obj));
  append_c('A');
  append_sz(sz);
  for(i = 0; i < sz; i++) {
    marshal(state, array_get(state, obj, i), buf, ms);
  }
}

static OBJECT unmarshal_ary(STATE, struct marshal_state *ms) {
  int i, j, cur;
  OBJECT o;
  int sz = unmarshal_num_fields(ms);
  OBJECT ary = array_new(state, sz);

  cur = ms->consumed;
  for(i = 0; i < sz; i++) {
    uint8_t *old = ms->buf;

    o = unmarshal(state, ms);
    j = ms->consumed - cur;
    ms->buf = old + j;
    cur = ms->consumed;
    array_set(state, ary, i, o);
  }
  
  return ary;
}

static void marshal_tup(STATE, OBJECT obj, bstring buf, struct marshal_state *ms) {
  return marshal_fields_as(state, obj, buf, 'p', ms);
}

static OBJECT unmarshal_tup(STATE, struct marshal_state *ms) {
  int sz;
  OBJECT tup;
  sz = unmarshal_num_fields(ms);
  tup = tuple_new(state, sz);
  unmarshal_into_fields(state, sz, tup, ms);
  return tup;
}

static void marshal_bignum(STATE, OBJECT obj, bstring buf) {
  int i;
  char buffer[1024];
  bignum_into_string(state, obj, 10, buffer, 1024);
  append_c('B');
  i = strlen(buffer);
  append_sz(i);
  bcatblk(buf, buffer, i);
  append_c(0);                  /* zero byte */
}

static void marshal_fixnum(STATE, OBJECT obj, bstring buf) {
  char buffer[1024];
  int i;

  i = snprintf(buffer, 1023, "%ld", (long int)N2I(obj));

  append_c('B');
  append_sz(i);
  bcatblk(buf, buffer, i);
  append_c(0);
}

static OBJECT unmarshal_bignum(STATE, struct marshal_state *ms) {
  int sz;
  sz = read_int(ms->buf + 1);
  ms->consumed += 5;
  ms->consumed += sz;
  ms->consumed++;               /* zero byte */
  return bignum_from_string(state, (char *) ms->buf + 5, 10);
}

static void marshal_floatpoint(STATE, OBJECT obj, bstring buf) {
  int i;
  char buffer[26];

  float_into_string(state, obj, buffer, 26);
  append_c('d');
  i = strlen(buffer);
  append_sz(i);
  bcatblk(buf, buffer, i);
  append_c(0);               /* zero byte */
}

static OBJECT unmarshal_floatpoint(STATE, struct marshal_state *ms) {
  int sz;
  sz = read_int(ms->buf + 1);
  ms->consumed += 5;
  ms->consumed += sz;
  ms->consumed++;               /* zero byte */
  return float_from_string(state, (char *) ms->buf + 5);
}

static void marshal_bytes(STATE, OBJECT obj, bstring buf) {
  int i;
  i = SIZE_OF_BODY(obj);
  append_c('b');
  append_sz(i);
  append_str(bytearray_byte_address(state, obj), i);
}

static OBJECT unmarshal_bytes(STATE, struct marshal_state *ms) {
  int sz;
  OBJECT obj;
  sz = read_int(ms->buf + 1);
  
  ms->consumed += 5;
  ms->consumed += sz;
  obj = bytearray_new(state, sz);
  
  memcpy(bytearray_byte_address(state, obj), ms->buf + 5, sz);
  
  // printf("Unmarshaled bytes: %p / %d / %d\n", bytearray_byte_address(state, obj), sz, bytearray_bytes(state, obj));
  
  return obj;
}

static void marshal_iseq(STATE, OBJECT obj, bstring buf) {
  int i;
  append_c('I');
  append_c('b');
  
  i = SIZE_OF_BODY(obj);
  append_sz(i);
  append_str(bytearray_byte_address(state, obj), i);
}

static OBJECT unmarshal_iseq(STATE, struct marshal_state *ms) {
  int sz;
  char endian;
  OBJECT obj;
  endian = ms->buf[1];
    
  sz = read_int(ms->buf + 2);
  
  ms->consumed += 6;
  ms->consumed += sz;

  obj = iseq_new(state, sz);
  
  memcpy(bytearray_byte_address(state, obj), ms->buf + 6, sz);
  
  /* We only support iseq's stored big endian currently. */
  sassert(endian == 'b');
  
  return obj;
}

static OBJECT unmarshal_cmethod(STATE, struct marshal_state *ms) {
  int sz;
  OBJECT cm, prim;
  sz = unmarshal_num_fields(ms);
  cm = cmethod_allocate(state);
  unmarshal_into_fields(state, sz, cm, ms);
  
  /* fixups */
  prim = cmethod_get_primitive(cm);
  if(SYMBOL_P(prim)) {
    int idx = calc_primitive_index(state, symbol_to_string(state, prim));
    sassert(idx >= 0);
    cmethod_set_primitive(cm, I2N(idx));
  } else if(NIL_P(prim)) {
    cmethod_set_primitive(cm, I2N(-1));
  }
  return cm;
}

static void marshal_cmethod2(STATE, OBJECT obj, bstring buf, struct marshal_state *ms) {
  
  int i;
  append_c('M');
  /* rather than a size, we use a version id */
  append_sz(1);
  
  for(i = 0; i < 16; i++) {
    marshal(state, NTH_FIELD(obj, i), buf, ms);  
  }
}


static OBJECT unmarshal_cmethod2(STATE, struct marshal_state *ms) {
  int ver, i;
  OBJECT cm, prim, o, l;
  
  ver = unmarshal_num_fields(ms);
  cm = cmethod_allocate(state);
  
  unmarshal_into_fields(state, 16, cm, ms);
    
  /* fixups */
  prim = cmethod_get_primitive(cm);
  if(SYMBOL_P(prim)) {
    int idx = calc_primitive_index(state, symbol_to_string(state, prim));
    sassert(idx >= 0);
    cmethod_set_primitive(cm, I2N(idx));
  } else if(NIL_P(prim)) {
    cmethod_set_primitive(cm, I2N(-1));
  }
  
  /* Set the compiled method field on each SendSite */
  l = cmethod_get_literals(cm);
  if(TUPLE_P(l)) {
    int sz = tuple_fields(state, l);
    for(i = 0; i < sz; i++) {
      o = tuple_at(state, l, i);
      if(SENDSITE_P(o)) {
        send_site_set_sender(state, o, cm);
      }
    }
  }
  
  return cm;
}

static OBJECT unmarshal(STATE, struct marshal_state *ms) {
  uint8_t tag = *ms->buf;
  OBJECT o;

//  printf("%c\n", tag);
  switch(tag) {
    case 'i':
      o = unmarshal_int(state, ms);
      break;
    case 's':
      o = unmarshal_str(state, ms);
      _add_object(o, ms);
      break;
    case 'S':
      o = unmarshal_sendsite(state, ms);
      break;
    case 'x':
      o = unmarshal_sym(state, ms);
      break;
    case 'p':
      o = unmarshal_tup(state, ms);
      _add_object(o, ms);
      break;
    case 'A':
      o = unmarshal_ary(state, ms);
      _add_object(o, ms);
      break;
    case 'b':
      o = unmarshal_bytes(state, ms);
      _add_object(o, ms);
      break;
    case 'I':
      o = unmarshal_iseq(state, ms);
      _add_object(o, ms);
      break;
    case 'm':
      o = unmarshal_cmethod(state, ms);
      _add_object(o, ms);
      break;
    case 'M':
      o = unmarshal_cmethod2(state, ms);
      _add_object(o, ms);
      break;
    case 'B':
      o = unmarshal_bignum(state, ms);
      _add_object(o, ms);
      break;
    case 'd':
      o = unmarshal_floatpoint(state, ms);
      _add_object(o, ms);
      break;
    case 'r':
      o = _nth_object(state, ms);
      break;
    case 'n':
      ms->consumed += 1;
      o = Qnil;
      break;
    case 't':
      ms->consumed += 1;
      o = Qtrue;
      break;
    case 'f':
      ms->consumed += 1;
      o = Qfalse;
      break;
    default:
      o = Qnil;
      printf("Unknown marshal type '0x%x' at %d!\n", tag, ms->consumed);
      sassert(0);
  }
  return o;
}

static void marshal(STATE, OBJECT obj, bstring buf, struct marshal_state *ms) {
  OBJECT kls;
  int ref;
  
  if(FIXNUM_P(obj)) {
    marshal_fixnum(state, obj, buf);
  } else if(SYMBOL_P(obj)) {
    marshal_sym(state, obj, buf);
  } else if(obj == Qnil) {
    append_c('n');
  } else if(obj == Qtrue) {
    append_c('t');
  } else if(obj == Qfalse) {
    append_c('f');
  } else if(REFERENCE_P(obj)) {
    if((ref = _find_object(obj, ms)) > 0) {
      append_c('r');
      append_sz(ref);
    } else {
      _add_object(obj, ms);
      kls = object_class(state, obj);
      if(kls == state->global->string) {
        marshal_str(state, obj, buf);
      } else if(kls == state->global->tuple) {
        marshal_tup(state, obj, buf, ms);
      } else if(kls == state->global->array) {
        marshal_ary(state, obj, buf, ms);
      } else if(kls == state->global->cmethod) {
        marshal_cmethod2(state, obj, buf, ms);
      } else if(kls == state->global->bytearray) {
        marshal_bytes(state, obj, buf);
      } else if(kls == state->global->iseq) {
        marshal_iseq(state, obj, buf);
      } else if(kls == BASIC_CLASS(bignum)) {
        marshal_bignum(state, obj, buf);
      } else if(kls == BASIC_CLASS(floatpoint)) {
        marshal_floatpoint(state, obj, buf);
      } else if(SENDSITE_P(obj)) {
        marshal_sendsite(state, obj, buf);
      } else {
        printf("Unable to marshal class %p = %s!\n", (void *)kls, rbs_inspect(state, kls));
      }
    }
  }
}

OBJECT cpu_marshal(STATE, OBJECT obj, int version) {
  bstring buf;
  OBJECT ret;

  buf = cpu_marshal_to_bstring(state, obj, version);
  ret = string_newfrombstr(state, buf);
  bdestroy(buf);
  return ret;
}

bstring cpu_marshal_to_bstring(STATE, OBJECT obj, int version) {
  bstring buf, stream;
  struct marshal_state ms;
  unsigned char cur_digest[20];
  
  ms.consumed = 0;
  ms.objects = ptr_array_new(8);
    
  stream = cstr2bstr("");
  marshal(state, obj, stream, &ms);
  sha1_hash_string((unsigned char*)bdata(stream), blength(stream), cur_digest);
  
  buf = cstr2bstr("RBIX");

  _append_sz(buf, version);
  bcatblk(buf, (void*)cur_digest, 20);
  bconcat(buf, stream);
  bdestroy(stream);
  
  ptr_array_free(ms.objects);
  return buf;
}

OBJECT cpu_marshal_to_file(STATE, OBJECT obj, char *path, int version) {
  bstring buf;
  FILE *f;
  struct marshal_state ms;
  unsigned char cur_digest[20];
  unsigned char bytes[4];
  
  f = fopen(path, "wb");
  if(!f) {
    return Qfalse;
  }
  
  ms.consumed = 0;
  ms.objects = ptr_array_new(8);
  
  buf = cstr2bstr("");
  
  marshal(state, obj, buf, &ms);
  sha1_hash_string((unsigned char*)bdata(buf), blength(buf), cur_digest);
  
  /* TODO do error chceking here */
  fwrite("RBIX", 1, 4, f);

  int2be(version, bytes);  
  fwrite(bytes, 1, 4, f);

  fwrite(cur_digest, 1, 20, f);
  
  fwrite(bdatae(buf,""), 1, blength(buf), f);
  fclose(f);

  bdestroy(buf);
  ptr_array_free(ms.objects);
  return Qtrue;
}

OBJECT cpu_unmarshal(STATE, uint8_t *str, int len, int version) {
  struct marshal_state ms;
  OBJECT ret;
  int in_version;
  int offset = 4;
  unsigned char cur_digest[20];
  
  if(!memcmp(str, "RBIS", 4)) {
    version = -1;
  } else if(!memcmp(str, "RBIX", 4)) {
    in_version = read_int(str + 4);
    if(in_version < version) {
      /* file is out of date. */
      return Qnil;
    }
    
    offset += 4;      
    offset += 20;
      
    sha1_hash_string((unsigned char*)(str + offset), len - offset, cur_digest);

    /* Check if the calculate one is the one in the stream. */
    if(memcmp(str + offset - 20, cur_digest, 20)) {
      return Qnil;
    }
  } else {
    printf("Invalid compiled file.\n");
    return Qnil;
  }
  ms.consumed = 0;
  ms.objects = ptr_array_new(8);
  ms.buf = str + offset;

  ret = unmarshal(state, &ms);
  ptr_array_free(ms.objects);
  return ret;
}

OBJECT cpu_unmarshal_file(STATE, const char *path, int version) {
  OBJECT obj;
  void *map;
  struct stat st;
  int fd;

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    return Qnil;
  }

  if (fstat(fd, &st) || !st.st_size) {
    close(fd);
    return Qnil;
  }

  map = mmap(NULL, (size_t) st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  obj = cpu_unmarshal(state, map, (int)st.st_size, version);
  munmap(map, st.st_size);

  return obj;
}

