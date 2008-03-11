#include <assert.h>
#include <errno.h>

#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/object.h"
#include "shotgun/lib/symbol.h"
#include "shotgun/lib/module.h"
#include "shotgun/lib/hash.h"
#include "shotgun/lib/lookuptable.h"
#include "shotgun/lib/regexp.h"
#include "shotgun/lib/subtend/ffi.h"
#include "shotgun/lib/selector.h"
#include "shotgun/lib/sendsite.h"

#define BC(o) BASIC_CLASS(o)

void bignum_init(STATE);
void Init_cpu_task(STATE);
void Init_list(STATE);
void cpu_bootstrap_exceptions(STATE);

/* Creates the rubinius object universe from scratch. */
void cpu_bootstrap(STATE) {
  OBJECT cls, obj, tmp, tmp2;
  int i;

  /* Class is created first by hand, and twittle to setup the internal
     recursion. */
  cls = NEW_OBJECT(Qnil, CLASS_FIELDS);
  cls->klass = cls;
  class_set_instance_fields(cls, I2N(CLASS_FIELDS));
  class_set_has_ivars(cls, Qtrue);
  class_set_object_type(cls, I2N(ClassType));
  cls->obj_type = ClassType;
  
  BC(class) = cls;
  obj = _object_basic_class(state, Qnil);
  BC(object) = obj;
  BC(module) = _module_basic_class(state, obj);
  class_set_superclass(cls, BC(module));
  BC(metaclass) = _metaclass_basic_class(state, cls);
  class_set_object_type(BC(metaclass), I2N(MetaclassType));
  
  BC(tuple) = _tuple_basic_class(state, obj);
  BC(hash) =  _hash_basic_class(state, obj);
  BC(lookuptable) = _lookuptable_basic_class(state, obj);
  BC(methtbl) = _methtbl_basic_class(state, BC(lookuptable));
  
  object_create_metaclass(state, obj, cls);
  object_create_metaclass(state, BC(module), object_metaclass(state, obj));
  object_create_metaclass(state, BC(class), object_metaclass(state, BC(module)));
  
  object_create_metaclass(state, BC(tuple), (OBJECT)0);
  object_create_metaclass(state, BC(hash), (OBJECT)0);
  object_create_metaclass(state, BC(lookuptable), (OBJECT)0);
  object_create_metaclass(state, BC(methtbl), (OBJECT)0);
  
  module_setup_fields(state, object_metaclass(state, obj));
  module_setup_fields(state, object_metaclass(state, BC(module)));
  module_setup_fields(state, object_metaclass(state, BC(class)));
  module_setup_fields(state, object_metaclass(state, BC(tuple)));
  module_setup_fields(state, object_metaclass(state, BC(hash)));
  module_setup_fields(state, object_metaclass(state, BC(lookuptable)));
  module_setup_fields(state, object_metaclass(state, BC(methtbl)));
  BC(symbol) = _symbol_class(state, obj);
  BC(array) = _array_class(state, obj);
  BC(bytearray) = _bytearray_class(state, obj);
  BC(string) = _string_class(state, obj);
  BC(symtbl) = _symtbl_class(state, obj);
  BC(cmethod) = _cmethod_class(state, obj);
  BC(io) = _io_class(state, obj);
  BC(blokenv) = _blokenv_class(state, obj);
  BC(icache) = _icache_class(state, obj);
  BC(staticscope) = _staticscope_class(state, obj);
 
  class_set_object_type(BC(bytearray), I2N(ByteArrayType));
  class_set_object_type(BC(string), I2N(StringType));
  class_set_object_type(BC(methtbl), I2N(MTType));
  class_set_object_type(BC(tuple), I2N(TupleType));
  class_set_object_type(BC(hash), I2N(HashType));
  class_set_object_type(BC(lookuptable), I2N(LookupTableType));
  
  /* The symbol table */
  state->global->symbols = symtbl_new(state);
  
  module_setup(state, obj, "Object");
  module_setup(state, cls, "Class");
  module_setup(state, BC(module), "Module");
  module_setup(state, BC(metaclass), "MetaClass");
  module_setup(state, BC(symbol), "Symbol");
  module_setup(state, BC(tuple), "Tuple");
  module_setup(state, BC(array), "Array");
  module_setup(state, BC(bytearray), "ByteArray");
  module_setup(state, BC(hash), "Hash");
  module_setup(state, BC(lookuptable), "LookupTable");
  module_setup(state, BC(string), "String");
  module_setup(state, BC(symtbl), "SymbolTable");
  module_setup(state, BC(methtbl), "MethodTable");
  module_setup(state, BC(cmethod), "CompiledMethod");
  module_setup(state, BC(io), "IO");
  module_setup(state, BC(blokenv), "BlockEnvironment");
  module_setup(state, BC(icache), "InlineCache");
  module_setup(state, BC(staticscope), "StaticScope");
 
  class_set_object_type(BC(array), I2N(ArrayType));
  class_set_object_type(BC(cmethod), I2N(CMethodType));
  class_set_object_type(BC(blokenv), I2N(BlockEnvType));
    
  rbs_const_set(state, obj, "Symbols", state->global->symbols);
  BC(nil_class) = rbs_class_new(state, "NilClass", 0, obj);
  BC(true_class) = rbs_class_new(state, "TrueClass", 0, obj);
  BC(false_class) = rbs_class_new(state, "FalseClass", 0, obj);
  tmp = rbs_class_new(state, "Numeric", 0, obj);
  tmp2 = rbs_class_new(state, "Integer", 0, tmp);
  BC(fixnum_class) = rbs_class_new(state, "Fixnum", 0, tmp2);
  class_set_object_type(BC(fixnum_class), I2N(FixnumType));
  
  BC(bignum) = rbs_class_new(state, "Bignum", 0, tmp2);
  class_set_object_type(BC(bignum), I2N(BignumType));
  bignum_init(state);
  
  BC(floatpoint) = rbs_class_new(state, "Float", 0, tmp);
  class_set_object_type(BC(floatpoint), I2N(FloatType));
  
  BC(undef_class) = rbs_class_new(state, "UndefClass", 0, obj);
  BC(fastctx) = rbs_class_new(state, "MethodContext", 0, obj);
  BC(methctx) = BC(fastctx);
  BC(blokctx) = rbs_class_new(state, "BlockContext", 0, BC(fastctx));
  
  BC(task) = rbs_class_new(state, "Task", 0, obj);
  class_set_object_type(BC(task), I2N(TaskType));
  
  BC(iseq) = rbs_class_new(state, "InstructionSequence", 0, BC(bytearray));
  class_set_object_type(BC(iseq), I2N(ISeqType));

  #define bcs(name, sup, string) BC(name) = _ ## name ## _class(state, sup); \
    module_setup(state, BC(name), string);
  
  /* the special_classes C array is use do quickly calculate the class
     of an immediate by just indexing into the array using (obj & 0x1f) */
  
  /* fixnum, symbol, and custom can have a number of patterns below
     0x1f, so we fill them all. */
     
  for(i = 0; i < SPECIAL_CLASS_SIZE; i += 4) {
    state->global->special_classes[i + 0] = Qnil;
    state->global->special_classes[i + 1] = BC(fixnum_class);
    state->global->special_classes[i + 2] = Qnil;
    if(((i + 3) & 0x7) == 0x3) {
      state->global->special_classes[i + 3] = BC(symbol);
    } else {
      state->global->special_classes[i + 3] = CUSTOM_CLASS;
    }
  }
  
  /* These only have one value, so they only need one spot in
     the array */
  state->global->special_classes[(intptr_t)Qundef] = BC(undef_class);
  state->global->special_classes[(intptr_t)Qfalse] = BC(false_class);
  state->global->special_classes[(intptr_t)Qnil  ] = BC(nil_class);
  state->global->special_classes[(intptr_t)Qtrue ] = BC(true_class);
    
  bcs(regexp, obj, "Regexp");
  class_set_object_type(BC(regexp), I2N(RegexpType));
  bcs(regexpdata, obj, "RegexpData");
  class_set_object_type(BC(regexpdata), I2N(RegexpDataType));
  bcs(matchdata, obj, "MatchData");
      
  cpu_bootstrap_exceptions(state);
  
  rbs_module_new(state, "Rubinius", BC(object));
  
  Init_list(state);
  Init_cpu_task(state);
  Init_ffi(state);
  regexp_init(state);
  selector_init(state);
  send_site_init(state);
    
  rbs_const_set(state, 
        rbs_const_get(state, BASIC_CLASS(object), "Rubinius"),
        "Primitives",
        cpu_populate_prim_names(state));
  
  state->global->external_ivars = lookuptable_new(state);

}

void cpu_bootstrap_exceptions(STATE) {
  int sz;
  sz = 3;
  
  OBJECT exc, scp, std, arg, nam, loe, rex, stk, sxp, sce, type, lje, vm;
  OBJECT fce;
  
  #define dexc(name, sup) rbs_class_new(state, #name, sz, sup)
  
  exc = dexc(Exception, BC(object));
  state->global->exception = exc;
  dexc(fatal, exc);
  vm = dexc(VMError, exc);
  dexc(VMAssertion, vm);
  scp = dexc(ScriptError, exc);
  std = dexc(StandardError, exc);
  type = dexc(TypeError, std);
  arg = dexc(ArgumentError, std);
  nam = dexc(NameError, std);
  rex = dexc(RegexpError, std);
  dexc(NoMethodError, nam);
  dexc(SyntaxError, scp);
  loe = dexc(LoadError, scp);
  dexc(RuntimeError, std);
  sce = dexc(SystemCallError, std);
  stk = dexc(StackError, exc);
  sxp = dexc(StackExploded, stk);
  
  lje = dexc(LocalJumpError, std);
  dexc(IllegalLongReturn, lje);
  
  fce = dexc(FlowControlException, exc);
  dexc(ReturnException, fce);
  dexc(LongReturnException, fce);
  
  state->global->exc_type = type;
  state->global->exc_arg = arg;
  state->global->exc_loe = loe;
  state->global->exc_rex = rex;
  
  state->global->exc_stack_explosion = sxp;
  state->global->exc_primitive_failure = dexc(PrimitiveFailure, exc);
  
  state->global->exc_segfault = dexc(MemorySegmentionError, exc);

  OBJECT ern = rbs_module_new(state, "Errno", state->global->object);

  state->global->errno_mapping = lookuptable_new(state);

  rbs_const_set(state, ern, "Mapping", state->global->errno_mapping);
  
  sz = 4;
  
#define set_syserr(num, name) ({ \
  OBJECT _cls = rbs_class_new_with_namespace(state, name, sz, sce, ern); \
  rbs_const_set(state, _cls, "Errno", I2N(num)); \
  lookuptable_store(state, state->global->errno_mapping, I2N(num), _cls); \
  })

/*
 * Stolen from MRI
 */

#ifdef EPERM
    set_syserr(EPERM, "EPERM");
#endif
#ifdef ENOENT
    set_syserr(ENOENT, "ENOENT");
#endif
#ifdef ESRCH
    set_syserr(ESRCH, "ESRCH");
#endif
#ifdef EINTR
    set_syserr(EINTR, "EINTR");
#endif
#ifdef EIO
    set_syserr(EIO, "EIO");
#endif
#ifdef ENXIO
    set_syserr(ENXIO, "ENXIO");
#endif
#ifdef E2BIG
    set_syserr(E2BIG, "E2BIG");
#endif
#ifdef ENOEXEC
    set_syserr(ENOEXEC, "ENOEXEC");
#endif
#ifdef EBADF
    set_syserr(EBADF, "EBADF");
#endif
#ifdef ECHILD
    set_syserr(ECHILD, "ECHILD");
#endif
#ifdef EAGAIN
    set_syserr(EAGAIN, "EAGAIN");
#endif
#ifdef ENOMEM
    set_syserr(ENOMEM, "ENOMEM");
#endif
#ifdef EACCES
    set_syserr(EACCES, "EACCES");
#endif
#ifdef EFAULT
    set_syserr(EFAULT, "EFAULT");
#endif
#ifdef ENOTBLK
    set_syserr(ENOTBLK, "ENOTBLK");
#endif
#ifdef EBUSY
    set_syserr(EBUSY, "EBUSY");
#endif
#ifdef EEXIST
    set_syserr(EEXIST, "EEXIST");
#endif
#ifdef EXDEV
    set_syserr(EXDEV, "EXDEV");
#endif
#ifdef ENODEV
    set_syserr(ENODEV, "ENODEV");
#endif
#ifdef ENOTDIR
    set_syserr(ENOTDIR, "ENOTDIR");
#endif
#ifdef EISDIR
    set_syserr(EISDIR, "EISDIR");
#endif
#ifdef EINVAL
    set_syserr(EINVAL, "EINVAL");
#endif
#ifdef ENFILE
    set_syserr(ENFILE, "ENFILE");
#endif
#ifdef EMFILE
    set_syserr(EMFILE, "EMFILE");
#endif
#ifdef ENOTTY
    set_syserr(ENOTTY, "ENOTTY");
#endif
#ifdef ETXTBSY
    set_syserr(ETXTBSY, "ETXTBSY");
#endif
#ifdef EFBIG
    set_syserr(EFBIG, "EFBIG");
#endif
#ifdef ENOSPC
    set_syserr(ENOSPC, "ENOSPC");
#endif
#ifdef ESPIPE
    set_syserr(ESPIPE, "ESPIPE");
#endif
#ifdef EROFS
    set_syserr(EROFS, "EROFS");
#endif
#ifdef EMLINK
    set_syserr(EMLINK, "EMLINK");
#endif
#ifdef EPIPE
    set_syserr(EPIPE, "EPIPE");
#endif
#ifdef EDOM
    set_syserr(EDOM, "EDOM");
#endif
#ifdef ERANGE
    set_syserr(ERANGE, "ERANGE");
#endif
#ifdef EDEADLK
    set_syserr(EDEADLK, "EDEADLK");
#endif
#ifdef ENAMETOOLONG
    set_syserr(ENAMETOOLONG, "ENAMETOOLONG");
#endif
#ifdef ENOLCK
    set_syserr(ENOLCK, "ENOLCK");
#endif
#ifdef ENOSYS
    set_syserr(ENOSYS, "ENOSYS");
#endif
#ifdef ENOTEMPTY
    set_syserr(ENOTEMPTY, "ENOTEMPTY");
#endif
#ifdef ELOOP
    set_syserr(ELOOP, "ELOOP");
#endif
#ifdef EWOULDBLOCK
    set_syserr(EWOULDBLOCK, "EWOULDBLOCK");
#endif
#ifdef ENOMSG
    set_syserr(ENOMSG, "ENOMSG");
#endif
#ifdef EIDRM
    set_syserr(EIDRM, "EIDRM");
#endif
#ifdef ECHRNG
    set_syserr(ECHRNG, "ECHRNG");
#endif
#ifdef EL2NSYNC
    set_syserr(EL2NSYNC, "EL2NSYNC");
#endif
#ifdef EL3HLT
    set_syserr(EL3HLT, "EL3HLT");
#endif
#ifdef EL3RST
    set_syserr(EL3RST, "EL3RST");
#endif
#ifdef ELNRNG
    set_syserr(ELNRNG, "ELNRNG");
#endif
#ifdef EUNATCH
    set_syserr(EUNATCH, "EUNATCH");
#endif
#ifdef ENOCSI
    set_syserr(ENOCSI, "ENOCSI");
#endif
#ifdef EL2HLT
    set_syserr(EL2HLT, "EL2HLT");
#endif
#ifdef EBADE
    set_syserr(EBADE, "EBADE");
#endif
#ifdef EBADR
    set_syserr(EBADR, "EBADR");
#endif
#ifdef EXFULL
    set_syserr(EXFULL, "EXFULL");
#endif
#ifdef ENOANO
    set_syserr(ENOANO, "ENOANO");
#endif
#ifdef EBADRQC
    set_syserr(EBADRQC, "EBADRQC");
#endif
#ifdef EBADSLT
    set_syserr(EBADSLT, "EBADSLT");
#endif
#ifdef EDEADLOCK
    set_syserr(EDEADLOCK, "EDEADLOCK");
#endif
#ifdef EBFONT
    set_syserr(EBFONT, "EBFONT");
#endif
#ifdef ENOSTR
    set_syserr(ENOSTR, "ENOSTR");
#endif
#ifdef ENODATA
    set_syserr(ENODATA, "ENODATA");
#endif
#ifdef ETIME
    set_syserr(ETIME, "ETIME");
#endif
#ifdef ENOSR
    set_syserr(ENOSR, "ENOSR");
#endif
#ifdef ENONET
    set_syserr(ENONET, "ENONET");
#endif
#ifdef ENOPKG
    set_syserr(ENOPKG, "ENOPKG");
#endif
#ifdef EREMOTE
    set_syserr(EREMOTE, "EREMOTE");
#endif
#ifdef ENOLINK
    set_syserr(ENOLINK, "ENOLINK");
#endif
#ifdef EADV
    set_syserr(EADV, "EADV");
#endif
#ifdef ESRMNT
    set_syserr(ESRMNT, "ESRMNT");
#endif
#ifdef ECOMM
    set_syserr(ECOMM, "ECOMM");
#endif
#ifdef EPROTO
    set_syserr(EPROTO, "EPROTO");
#endif
#ifdef EMULTIHOP
    set_syserr(EMULTIHOP, "EMULTIHOP");
#endif
#ifdef EDOTDOT
    set_syserr(EDOTDOT, "EDOTDOT");
#endif
#ifdef EBADMSG
    set_syserr(EBADMSG, "EBADMSG");
#endif
#ifdef EOVERFLOW
    set_syserr(EOVERFLOW, "EOVERFLOW");
#endif
#ifdef ENOTUNIQ
    set_syserr(ENOTUNIQ, "ENOTUNIQ");
#endif
#ifdef EBADFD
    set_syserr(EBADFD, "EBADFD");
#endif
#ifdef EREMCHG
    set_syserr(EREMCHG, "EREMCHG");
#endif
#ifdef ELIBACC
    set_syserr(ELIBACC, "ELIBACC");
#endif
#ifdef ELIBBAD
    set_syserr(ELIBBAD, "ELIBBAD");
#endif
#ifdef ELIBSCN
    set_syserr(ELIBSCN, "ELIBSCN");
#endif
#ifdef ELIBMAX
    set_syserr(ELIBMAX, "ELIBMAX");
#endif
#ifdef ELIBEXEC
    set_syserr(ELIBEXEC, "ELIBEXEC");
#endif
#ifdef EILSEQ
    set_syserr(EILSEQ, "EILSEQ");
#endif
#ifdef ERESTART
    set_syserr(ERESTART, "ERESTART");
#endif
#ifdef ESTRPIPE
    set_syserr(ESTRPIPE, "ESTRPIPE");
#endif
#ifdef EUSERS
    set_syserr(EUSERS, "EUSERS");
#endif
#ifdef ENOTSOCK
    set_syserr(ENOTSOCK, "ENOTSOCK");
#endif
#ifdef EDESTADDRREQ
    set_syserr(EDESTADDRREQ, "EDESTADDRREQ");
#endif
#ifdef EMSGSIZE
    set_syserr(EMSGSIZE, "EMSGSIZE");
#endif
#ifdef EPROTOTYPE
    set_syserr(EPROTOTYPE, "EPROTOTYPE");
#endif
#ifdef ENOPROTOOPT
    set_syserr(ENOPROTOOPT, "ENOPROTOOPT");
#endif
#ifdef EPROTONOSUPPORT
    set_syserr(EPROTONOSUPPORT, "EPROTONOSUPPORT");
#endif
#ifdef ESOCKTNOSUPPORT
    set_syserr(ESOCKTNOSUPPORT, "ESOCKTNOSUPPORT");
#endif
#ifdef EOPNOTSUPP
    set_syserr(EOPNOTSUPP, "EOPNOTSUPP");
#endif
#ifdef EPFNOSUPPORT
    set_syserr(EPFNOSUPPORT, "EPFNOSUPPORT");
#endif
#ifdef EAFNOSUPPORT
    set_syserr(EAFNOSUPPORT, "EAFNOSUPPORT");
#endif
#ifdef EADDRINUSE
    set_syserr(EADDRINUSE, "EADDRINUSE");
#endif
#ifdef EADDRNOTAVAIL
    set_syserr(EADDRNOTAVAIL, "EADDRNOTAVAIL");
#endif
#ifdef ENETDOWN
    set_syserr(ENETDOWN, "ENETDOWN");
#endif
#ifdef ENETUNREACH
    set_syserr(ENETUNREACH, "ENETUNREACH");
#endif
#ifdef ENETRESET
    set_syserr(ENETRESET, "ENETRESET");
#endif
#ifdef ECONNABORTED
    set_syserr(ECONNABORTED, "ECONNABORTED");
#endif
#ifdef ECONNRESET
    set_syserr(ECONNRESET, "ECONNRESET");
#endif
#ifdef ENOBUFS
    set_syserr(ENOBUFS, "ENOBUFS");
#endif
#ifdef EISCONN
    set_syserr(EISCONN, "EISCONN");
#endif
#ifdef ENOTCONN
    set_syserr(ENOTCONN, "ENOTCONN");
#endif
#ifdef ESHUTDOWN
    set_syserr(ESHUTDOWN, "ESHUTDOWN");
#endif
#ifdef ETOOMANYREFS
    set_syserr(ETOOMANYREFS, "ETOOMANYREFS");
#endif
#ifdef ETIMEDOUT
    set_syserr(ETIMEDOUT, "ETIMEDOUT");
#endif
#ifdef ECONNREFUSED
    set_syserr(ECONNREFUSED, "ECONNREFUSED");
#endif
#ifdef EHOSTDOWN
    set_syserr(EHOSTDOWN, "EHOSTDOWN");
#endif
#ifdef EHOSTUNREACH
    set_syserr(EHOSTUNREACH, "EHOSTUNREACH");
#endif
#ifdef EALREADY
    set_syserr(EALREADY, "EALREADY");
#endif
#ifdef EINPROGRESS
    set_syserr(EINPROGRESS, "EINPROGRESS");
#endif
#ifdef ESTALE
    set_syserr(ESTALE, "ESTALE");
#endif
#ifdef EUCLEAN
    set_syserr(EUCLEAN, "EUCLEAN");
#endif
#ifdef ENOTNAM
    set_syserr(ENOTNAM, "ENOTNAM");
#endif
#ifdef ENAVAIL
    set_syserr(ENAVAIL, "ENAVAIL");
#endif
#ifdef EISNAM
    set_syserr(EISNAM, "EISNAM");
#endif
#ifdef EREMOTEIO
    set_syserr(EREMOTEIO, "EREMOTEIO");
#endif
#ifdef EDQUOT
    set_syserr(EDQUOT, "EDQUOT");
#endif

}
