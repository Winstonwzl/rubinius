#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ev.h>

/* *BSD dl support */
#ifdef __FreeBSD__
#include <dlfcn.h>
#elif defined(__OpenBSD__)
#include <dlfcn.h>
#endif

#include "shotgun/config.h"
#include "shotgun/lib/shotgun.h"
#include "shotgun/lib/string.h"
#include "shotgun/lib/io.h"
#include "shotgun/lib/hash.h"
#include "shotgun/lib/lookuptable.h"
#include "shotgun/lib/machine.h"
#include "shotgun/lib/array.h"
#include "shotgun/lib/ar.h"
#include "shotgun/lib/symbol.h"
#include "shotgun/lib/config_hash.h"
#include "shotgun/lib/methctx.h"
#include "shotgun/lib/tuple.h"
#include "shotgun/lib/subtend.h"
#include "shotgun/lib/subtend/nmc.h"
#include "shotgun/lib/instruction_names.h"

static int _recursive_reporting = 0;

#define SYM2STR(st, sym) string_byte_address(st, rbs_symbol_to_string(st, sym))

void machine_print_callstack_limited(machine m, int maxlev) {
  OBJECT context, tmp;
  const char *modname, *methname, *filename;
  struct fast_context *fc;

  if(!m) m = current_machine;

  context = m->c->active_context;

  cpu_flush_ip(m->c);
  cpu_flush_sp(m->c);

  FASTCTX(context)->ip = m->c->ip;
  
  while(RTEST(context) && maxlev--) {
    methctx_reference(m->s, context);
    fc = FASTCTX(context);

    if(fc->method_module && RTEST(fc->method_module)) {
      modname = SYM2STR(m->s, module_get_name(fc->method_module));
    } else {
      modname = "<none>";
    }

    if(fc->type == FASTCTX_BLOCK) {
      methname = "<block>";
    } else if(fc->name && RTEST(fc->name)) {
      if(SYMBOL_P(fc->name)) {
        methname = SYM2STR(m->s, fc->name);
      } else {
        methname = "<unknown>";
      }
    } else {
      methname = "<none>";
    }
    
    if(fc->method && RTEST(fc->method)) {
      tmp = cmethod_get_file(fc->method);
      if(SYMBOL_P(tmp)) {
        filename = SYM2STR(m->s, tmp);
      } else {
        filename = "<unknown>";
      }
    } else {
      filename = "<unknown>";
    }
    
    fprintf(stderr, "%10p %s#%s+%d in %s:%d\n",
      (void*)context, modname, methname,
      fc->ip,
      filename,
      cpu_ip2line(m->s, fc->method, fc->ip)
    );
    context = fc->sender;
  }
}

void machine_print_callstack(machine m) {
    machine_print_callstack_limited(m, -1);
}

void machine_print_stack(machine m) {
  unsigned int i, start, end;
  cpu_flush_sp(m->c);
  i = m->c->sp;
  start = (i < 5 ? 0 : i - 5);
  end =   (i + 5 > m->c->stack_size) ? m->c->stack_size : i + 5;
  for(i = start; i < end; i++) {
    if(i == m->c->sp) {
      printf("%4d => ", i);
    } else {
      printf("%4d    ", i);
    }
    printf("%s\n", rbs_inspect_verbose(m->s, m->c->stack_top[i]));
  }
  
}

void machine_print_registers(machine m) {
  cpu_flush_sp(m->c);
  cpu_flush_ip(m->c);
  printf("IP: %04d\nSP: %04d\n", m->c->ip, m->c->sp);
  if(NIL_P(m->c->exception)) {
    printf("Exception: none\n");
  } else {
    printf("Exception: %s\n", rbs_inspect(m->s, m->c->exception));
  }
}

void _machine_error_reporter(int sig, siginfo_t *info, void *ctx) {
  const char *signame;
  rni_context *rni_ctx;
  OBJECT addr;
  
  /* See if the error happened during the running of a C function.
     If so, we raise an exception about the error. */
  rni_ctx = subtend_retrieve_context();
  if(rni_ctx->nmc && rni_ctx->nmc->system_set) {
    /* TODO: generate the C backtrace as a string array and pass it
       via the nmc or global_context so that the exception can include
       it. */
    rni_ctx->fault_address = info->si_addr;
    rni_ctx->nmc->jump_val = SEGFAULT_DETECTED;
    setcontext(&rni_ctx->nmc->system);
  }
  
  /* This is really nice. We don't have to do this check at every
     fetch, instead, let it segfault and handle it here. 
     The check for - 4 is because the bounds checks grabs the number
     of fields from a ref right away, which is where it will segfault
     if it's not a ref. The fields are 4 bytes into the header.
     The check for - 8 is because it's common this happens when
     trying to grab the class of a non-reference. The class is
     8 bytes into the header. */
  if(sig == SIGSEGV || sig == SIGBUS) {
    addr = (OBJECT)(info->si_addr);
    if(!REFERENCE_P(addr) || !REFERENCE_P(addr - 4) || !REFERENCE_P(addr - 8)) {
      printf("Attempted to access field of non-reference.\n");
      if(current_machine->g_use_firesuit) {
        machine_handle_fire(FIRE_NULL);
      }
    }
  }
  
  if(_recursive_reporting) exit(-2);
  _recursive_reporting++;
  
  switch(sig) {
    case SIGSEGV:
      signame = "Segmentation fault (SIGSEGV)";
      break;
    case SIGBUS:
      signame = "Bus violation (SIGBUS)";
      break;
    case SIGABRT:
      signame = "Software abort (SIGABRT)";
      break;
    default:
      signame = "<UNKNOWN>";
  }
  
  printf("\nAn error has occured: %s (%d)\n\n", signame, sig);

  if(getenv("CRASH_WAIT")) {
    printf("Pausing so I can be debugged.\n");
    pause();
    printf("Continuing after debugger.\n");
  }

  printf("Ruby backtrace:\n");
  machine_print_callstack(current_machine);
  
  printf("\nVM Registers:\n");
  machine_print_registers(current_machine);
  
  exit(-2);
}

void machine_setup_signals(machine m) {
        m->error_report.sa_sigaction = _machine_error_reporter;
        sigemptyset(&m->error_report.sa_mask);
        m->error_report.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &m->error_report, NULL);
        sigaction(SIGBUS, &m->error_report, NULL);
        sigaction(SIGABRT, &m->error_report, NULL);
}

static void machine_setup_events(machine m) {
  /* libev will not "autodetect" kqueue because it is broken on darwin */
  m->s->event_base = ev_loop_new(EVFLAG_FORKCHECK);
  m->s->thread_infos = NULL;
}

machine machine_new(environment e) {
  machine m;
  int pipes[2];
  
  m = calloc(1, sizeof(struct rubinius_machine));
  m->g_use_firesuit = 0;
  m->g_access_violation = 0;
  m->sub = 0;
  pipe(pipes);

  /* Setup pipes used for message notification. */
  m->message_read_fd =  pipes[0];
  m->message_write_fd = pipes[1];

  m->s = rubinius_state_new();
  m->c = cpu_new(m->s);
  /* Initialize the instruction addresses. */
  cpu_run(m->s, m->c, TRUE);
  m->c->ip_ptr = &m->s->external_ip;

  machine_setup_signals(m);
  machine_setup_events(m);
  cpu_initialize(m->s, m->c);
  cpu_bootstrap(m->s);
  subtend_setup(m->s);
  cpu_setup_top_scope(m->s, m->c);
  cpu_initialize_context(m->s, m->c);

  machine_set_const(m, "MAIN", m->c->main);
  cpu_task_configure_preemption(m->s);
  environment_add_machine(e, m);
  
  m->s->om->bootstrap_loaded = 1;

  return m;
}

void machine_destroy(machine m) {
  cpu_destroy(m->c);
  state_destroy(m->s);
  free(m);
}

void machine_handle_fire(int kind) {
  current_machine->g_access_violation = kind;
  setcontext(&current_machine->g_firesuit);
}

void machine_handle_type_error(OBJECT obj, const char *message) {
  current_machine->g_firesuit_message = strdup(message);

  if(FIXNUM_P(obj)) {
    current_machine->g_firesuit_arg = FixnumType;
  } else if(SYMBOL_P(obj)) {
    current_machine->g_firesuit_arg = SymbolType;    
  } else if(REFERENCE_P(obj)) {
    current_machine->g_firesuit_arg = obj->obj_type;
  } else if(NIL_P(obj)) {
    current_machine->g_firesuit_arg = NilType;
  } else {
    current_machine->g_firesuit_arg = 0;
  }

  machine_handle_fire(FIRE_TYPE);
}

void machine_handle_assert(const char *reason, const char *file, int line) {
  fprintf(stderr, "VM Assertion: %s (%s:%d)\n", reason, file, line);
  
  printf("\nRuby backtrace:\n");
  machine_print_callstack(current_machine);
  
  if(!current_machine->g_use_firesuit) abort();
  current_machine->g_access_violation = FIRE_ASSERT;
  setcontext(&current_machine->g_firesuit);
}

OBJECT machine_load_file(machine m, const char *path) {
  return cpu_unmarshal_file(m->s, path, 0);
}

void machine_show_exception(machine m, OBJECT exc) {
  OBJECT msg;
  const char *buf;
  printf("\nError: An unhandled exception has terminated this VM.\n");
  msg = exception_get_message(exc);
  if(REFERENCE_P(msg)) {
    buf = string_byte_address(m->s, msg);
  } else {
    buf = "<no message>";
  }
  printf(" => %s (%s)\n\n", buf, rbs_inspect(m->s, exc->klass));

  /* Restore the context it happened at so print_callstack shows it. */
  m->c->active_context = exception_get_context(exc);
  machine_print_callstack(m);
  puts("");
}

int machine_run(machine m) {
  cpu_run(m->s, m->c, 0);
  m->c->ip_ptr = &m->s->external_ip;

  if(RTEST(m->c->exception)) {
    printf("Toplevel exception detected.\n");
    machine_show_exception(m, m->c->exception);
    return FALSE;
  }
  return TRUE;
}

int machine_run_file(machine m, const char *path) {
  OBJECT meth;
  int out;

  if(m->s->excessive_tracing) {
    printf("[ Loading file %s]\n", path);
  }

  meth = machine_load_file(m, path);
  if(!RTEST(meth)) {
    printf("Unable to load '%s'.\n", path);
    return FALSE;
  }
 
  m->c->depth = 0;
  cpu_stack_push(m->s, m->c, meth, FALSE);
  cpu_run_script(m->s, m->c, meth);
  out = machine_run(m);
  if(m->s->excessive_tracing) {
    printf("[ Finished loading file %s]\n", path);
  }
  return out;
}

void machine_set_const_under(machine m, const char *str, OBJECT val, OBJECT under) {
  OBJECT tbl;
  tbl = module_get_constants(under);
  lookuptable_store(m->s, tbl, string_new(m->s, str), val);
}

void machine_set_const(machine m, const char *str, OBJECT val) {
  machine_set_const_under(m, str, val, m->s->global->object);
}

void machine_save_args(machine m, int argc, char **argv) {
  char **na;
  na = calloc(argc, sizeof(char*));
  memcpy(na, argv, argc);
  m->argc = argc;
  m->argv = na;
  
  machine_setup_ruby(m, argv[0]);
  machine_setup_argv(m, argc, argv);
}

void machine_setup_standard_io(machine m) {
  machine_set_const(m, "STDIN", io_new(m->s, 0, "r"));
  machine_set_const(m, "STDOUT", io_new(m->s, 1, "w"));
  machine_set_const(m, "STDERR", io_new(m->s, 2, "w"));
}

int *machine_setup_piped_io(machine m) {
  int pin[2];
  int pout[2];
  int perr[2];
  int *pipes;

  pipe(pin);
  pipe(pout);
  pipe(perr);

  machine_set_const(m, "STDIN",  io_new(m->s, pin[0],  "r"));
  machine_set_const(m, "STDOUT", io_new(m->s, pout[1], "w"));
  machine_set_const(m, "STDERR", io_new(m->s, perr[1], "w"));

  pipes = ALLOC_N(int, 3);
  pipes[0] = pin[1];
  pipes[1] = pout[0];
  pipes[2] = perr[0];

  return pipes;
}

void machine_setup_ruby(machine m, char *name) {
  char buf[MAXPATHLEN];
  char wd[MAXPATHLEN];
  /* 
    HACK: this should be replaced by normal ruby code.  
      C sucks - Ryan Davis
  */
  if(name[0] != '/') {
    getcwd(wd, MAXPATHLEN);
    snprintf(buf, MAXPATHLEN, "%s/%s", wd, name);
    name = buf;
  }
  machine_set_const(m, "RUBY_BIN_PATH", string_new(m->s, name));
  m->interpreter = strdup(name);
}

void machine_setup_argv(machine m, int argc, char **argv) {
  OBJECT ary;
  int i;
  
  machine_set_const(m, "ARG0", string_new(m->s, argv[0]));
  
  ary = array_new(m->s, argc - 1);
  for(i = 0; i < argc - 1; i++) {
    array_set(m->s, ary, i, string_new(m->s, argv[i+1]));
  }
  
  machine_set_const(m, "ARGV", ary);
}

int is_number(char *str) {
  while(*str) {
    if(!isdigit(*str)) return FALSE;
    str++;
  }
  
  return TRUE;
}

static char *trim_str(char *str) {
  int i;
  while(*str && !isalnum(*str)) str++;
  
  for(i = strlen(str) - 1; str[i] && !isalnum(str[i]); i++) {
    str[i] = 0;
  }
  
  return str;
}

static void machine_parse_config_var(machine m, const char *input) {
  char *name, *val, *var, *eq;
  
  var = strdup(input);
  eq = strchr(var, '=');
  
  if(eq) {
    *eq++ = 0;
    
    name = trim_str(var);
    val =  trim_str(eq);
    
    if(!strcmp("include", name)) {
      machine_parse_config_file(m, val);
    } else {    
      if(m->show_config) {
        printf("[config] '%s' => '%s'\n", name, val);
      }
    
      ht_config_insert(m->s->config, cstr2bstr(name), cstr2bstr(val));
    }
  } else {
    if(m->show_config) {
      printf("[config] '%s' => '1'\n", var);
    }
    
    name = trim_str(var);
    ht_config_insert(m->s->config, cstr2bstr(name), cstr2bstr("1"));
  }
  
  XFREE(var);
}

void machine_parse_configs(machine m, const char *config) {
  char *semi;
  char tmp[1024];
  int sz;
  semi = strstr(config, ";");
  while(semi) {
    sz = semi - config;
    strncpy(tmp, config, sz);
    tmp[sz] = 0;
    machine_parse_config_var(m, tmp);
    config += (sz + 1);
    semi = strstr(config, ";");
  }
  
  machine_parse_config_var(m, config);
}

void machine_parse_config_file(machine m, const char *path) {
  FILE *fo;
  char line[1024];
  
  fo = fopen(path, "r");
  if(!fo) return;

  while (fgets(line, sizeof(line), fo)) {
    if(*line) {
      machine_parse_config_var(m, line);
    }
  }

  fclose(fo);
}

void machine_migrate_config(machine m) {
  struct hashtable_itr iter;
  rstate state = m->s;
  
  m->s->global->config = hash_new_sized(m->s, 500);
    
  if(hashtable_count(m->s->config) > 0) {
  
    hashtable_iterator_init(&iter, m->s->config);

    do {
      OBJECT ok, ov;
      bstring k = (bstring)hashtable_iterator_key(&iter);
      bstring v = (bstring)hashtable_iterator_value(&iter);
      ok = string_newfrombstr(m->s, k);
      if(is_number(bdata(v))) {
        ov = LL2N(strtoll(bdatae(v,""), NULL, 10));
      } else {
        ov = string_newfrombstr(m->s, v);
      }

      hash_set(m->s, m->s->global->config, ok, ov);
    } while (hashtable_iterator_advance(&iter));
  
  }
  
  machine_set_const(m, "RUBY_CONFIG", m->s->global->config);
  machine_setup_from_config(m);
}

void machine_setup_from_config(machine m) {
  bstring s;

  s = cstr2bstr("rbx.debug.trace");

  if(ht_config_search(m->s->config, s)) {
    m->s->excessive_tracing = 1;
  }

  bassigncstr (s, "rbx.debug.gc");
  
  if(ht_config_search(m->s->config, s)) {
    m->s->gc_stats = 1;
  }

  bdestroy (s);
}

void machine_setup_config(machine m) {
  OBJECT mod;
  STATE;
  
  state = m->s;
  
  mod = rbs_const_get(m->s, m->s->global->object, "Rubinius");
  machine_set_const(m, "RUBY_PLATFORM", string_new(m->s, CONFIG_HOST));
  machine_set_const(m, "RUBY_RELEASE_DATE", string_new(m->s, CONFIG_RELDATE));
  machine_set_const_under(m, "RBX_VERSION", string_new(m->s, CONFIG_VERSION), mod);
  machine_set_const_under(m, "RUBY_VERSION", string_new(m->s, CONFIG_RUBY_VERSION), mod);
  machine_set_const_under(m, "RUBY_PATCHLEVEL", string_new(m->s, CONFIG_RUBY_PATCHLEVEL), mod);
  machine_set_const_under(m, "VERSION", string_new(m->s, CONFIG_RUBY_VERSION), mod);
  machine_set_const_under(m, "RUBY_ENGINE", string_new(m->s, CONFIG_ENGINE), mod);
  machine_set_const_under(m, "BUILDREV", string_new(m->s, CONFIG_BUILDREV), mod);
  machine_set_const_under(m, "CODE_PATH", string_new(m->s, CONFIG_CODEPATH), mod);
  machine_set_const_under(m, "EXT_PATH", string_new(m->s, CONFIG_EXTPATH), mod);
  machine_set_const_under(m, "RBA_PATH", string_new(m->s, CONFIG_RBAPATH), mod);

  machine_set_const_under(m, "WORDSIZE", I2N(CONFIG_WORDSIZE), mod);
  
#if defined(__ppc__) || defined(__POWERPC__) || defined(_POWER)
  machine_set_const_under(m, "PLATFORM", SYM("ppc"), mod);
#elif defined(__amd64__)
  machine_set_const_under(m, "PLATFORM", SYM("amd64"), mod);
#elif defined(i386) || defined(__i386__)
  machine_set_const_under(m, "PLATFORM", SYM("x86"), mod);
#elif defined(__alpha) || defined(__alpha__)
  machine_set_const_under(m, "PLATFORM", SYM("alpha"), mod);
#elif defined(VAX) || defined(__VAX)
  machine_set_const_under(m, "PLATFORM", SYM("vax"), mod);
#elif defined(__hppa__)
  machine_set_const_under(m, "PLATFORM", SYM("hppa"), mod);
#elif defined(__sparc__)
  machine_set_const_under(m, "PLATFORM", SYM("sparc"), mod);
#elif defined(__s390__)
  machine_set_const_under(m, "PLATFORM", SYM("s390"), mod);
#elif (defined(TARGET_CPU_68K) || defined(__CFM68K__) || defined(m68k) || defined(_M_M68K))
  machine_set_const_under(m, "PLATFORM", SYM("m68k"), mod);
#else
  machine_set_const_under(m, "PLATFORM", SYM("unknown"), mod);
#endif
  
#if defined(__APPLE__) || defined(__MACH__)
  machine_set_const_under(m, "OS", SYM("darwin"), mod);
#elif defined(__linux__) || defined(linux) || defined(__linux)
  machine_set_const_under(m, "OS", SYM("linux"), mod);
#elif defined(__FreeBSD__)
  machine_set_const_under(m, "OS", SYM("freebsd"), mod);
#elif defined(__CYGWIN__)
  machine_set_const_under(m, "OS", SYM("cygwin"), mod);
#elif defined(__OS2__)
  machine_set_const_under(m, "OS", SYM("os2"), mod);
#elif defined(__NT__) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
  machine_set_const_under(m, "OS", SYM("win32"), mod);
#elif defined(__WINDOWS__)
  machine_set_const_under(m, "OS", SYM("windows_3x"), mod);
#elif defined(__NETWARE_386__)
  machine_set_const_under(m, "OS", SYM("netware"), mod);
#elif defined(__MSDOS__)
  machine_set_const_under(m, "OS", SYM("dos"), mod);
#elif defined(VMS) || defined(__VMS__)
  machine_set_const_under(m, "OS", SYM("vms"), mod);
#elif defined(__hpux__)
  machine_set_const_under(m, "OS", SYM("hpux"), mod);
#elif defined(__sun__) || defined(__sun)
  machine_set_const_under(m, "OS", SYM("solaris"), mod);
#elif defined(__svr4__)
  machine_set_const_under(m, "OS", SYM("unixware"), mod);
#elif defined(_AIX)
  machine_set_const_under(m, "OS", SYM("aix"), mod);
#elif (defined(_SCO_DS) && defined(_SCO_ELF) && defined(_SCO_XPG_VERS) && defined(_SCO_C_DIALECT))
  machine_set_const_under(m, "OS", SYM("openserver"), mod);
#elif defined(__unix__)
  machine_set_const_under(m, "OS", SYM("decunix"), mod);
#else
  machine_set_const_under(m, "OS", SYM("unknown"), mod);
#endif

#if defined(__VERSION__)
  machine_set_const_under(m, "COMPILER_VERSION", string_new(m->s, __VERSION__), mod);
#else
  machine_set_const_under(m, "COMPILER_VERSION", Qnil, mod);
#endif

#if defined(_MSC_VER)
  machine_set_const_under(m, "COMPILER", SYM("microsoft"), mod);  
#elif defined(__DECC) || defined(VAXC)
  machine_set_const_under(m, "COMPILER", SYM("digital"), mod);  
#elif defined(__BORLANDC__)
  machine_set_const_under(m, "COMPILER", SYM("borland"), mod);
#elif defined(__WATCOMC__)
  machine_set_const_under(m, "COMPILER", SYM("watcom"), mod);
#elif defined(__GNUC__)
  machine_set_const_under(m, "COMPILER", SYM("gcc"), mod);
#elif defined(__MWERKS__)
  machine_set_const_under(m, "COMPILER", SYM("metrowerks"), mod);
#elif defined(__IBMC__) || defined(__IBMCPP__)
  machine_set_const_under(m, "COMPILER", SYM("ibm"), mod);
#elif defined(__SUNPRO_C)
  machine_set_const_under(m, "COMPILER", SYM("sunpro"), mod);
#else
  machine_set_const_under(m, "COMPILER", SYM("unknown"), mod);
#endif

#if CONFIG_BIG_ENDIAN
  machine_set_const_under(m, "ENDIAN", SYM("big"), mod);
#else
  machine_set_const_under(m, "ENDIAN", SYM("little"), mod);
#endif

  if(sizeof(long) == 8) {
    machine_set_const_under(m, "L64", Qtrue, mod);
  } else {
    machine_set_const_under(m, "L64", Qfalse, mod);
  }  

#if defined(_WIN32) || defined(__NT__) || defined(WIN32) || defined(__WIN32__)
#define LIBSUFFIX "dll"
#elif defined(__APPLE__)
#define LIBSUFFIX "bundle"
#else
#define LIBSUFFIX "so"
#endif

  machine_set_const_under(m, "LIBSUFFIX", string_new(m->s, LIBSUFFIX), mod);
  machine_set_const_under(m, "COMPILER_PATH", string_new(m->s, CONFIG_CC), mod);
  
  if(isatty(0)) {
    machine_set_const_under(m, "Terminal", string_new(m->s, ttyname(0)), mod);
  } else {
    machine_set_const_under(m, "Terminal", Qfalse, mod);    
  }
  
  machine_set_const_under(m, "DEBUG_INST", I2N(CPU_INSTRUCTION_YIELD_DEBUGGER), mod);

  machine_set_const_under(m, "VM_ID", I2N(m->id), mod);
  machine_set_const_under(m, "VM_INFERIOR", m->sub ? Qtrue : Qfalse, mod);

  /* This feels like the wrong place for this, but it works. */
  machine_set_const_under(m, "MESSAGE_IO", io_new(m->s, m->message_read_fd, "r"), mod);
}

void machine_config_env(machine m) {
  char *config;
  if(getenv("RDEBUG")) {
    debug_enable();
  }
  
  if(getenv("RBX_CONFIG")) {
    m->show_config = 1;
  }
  
  config = getenv("RBX");
  if(config) {
    machine_parse_configs(m, config); 
  }
  
  config = getenv("RBX_CONFFILE");
  if(config) {
    machine_parse_config_file(m, config);
  }  
}

int machine_load_directory(machine m, const char *prefix) {
  char *path;
  char *file;
  FILE *fp;
  size_t buf_siz = 1024, prefix_len;

  prefix_len = strlen(prefix);
  if(prefix_len > (buf_siz - 16)) return FALSE;

  path = ALLOC_N(char, buf_siz);
  memcpy(path, prefix, prefix_len);
  strcpy(path + prefix_len, "/.load_order.txt");
  
  fp = fopen(path, "r");
  if(!fp) {
    printf("Unable to open directory '%s'\n", prefix);
    XFREE(path);
    return FALSE;
  }

  file = &path[prefix_len + 1];
  
  while (fgets(file, buf_siz - prefix_len, fp)) {
    size_t file_len;

    /* Get rid of the \n on the end. */
    file_len = strlen(file);

    if (path[prefix_len + file_len] == '\n')
      path[prefix_len + file_len] = 0;

    if(!machine_run_file(m, path)) {
      XFREE(path);
      fclose(fp);
      return FALSE;
    }
  }
  
  fclose(fp);
  XFREE(path);
  
  return TRUE;
}

int machine_load_object(machine m, char *name, uint8_t *data, long length) {
  OBJECT cm;

  if(m->s->excessive_tracing) {
    printf("[ Loading archived file %s]\n", name);
  }

  cm = cpu_unmarshal(m->s, data, length, 0);

  if(!RTEST(cm)) {
    return FALSE;
  }

  /* We push this on the stack so it's properly seen by the GCs */
  cpu_stack_push(m->s, m->c, cm, FALSE);
  cpu_run_script(m->s, m->c, cm);

  if(!machine_run(m)) {
    printf("Unable to run '%s'\n", name);
    return FALSE;
  }

  /* Pop the return value and script object. */
  (void)cpu_stack_pop(m->s, m->c);
  (void)cpu_stack_pop(m->s, m->c);

  return TRUE;
}

int machine_load_ar(machine m, const char *path) {
  int ret = FALSE;

  if(m->s->excessive_tracing) {
    printf("[ Loading ar rba %s]\n", path);
  }

  ret = ar_each_file(m, path, machine_load_object);

  if(m->s->excessive_tracing) {
    printf("[ Finished loading ar rba %s]\n", path);
  }

  return ret;
}

int machine_load_rba(machine m, const char *path) {
  return machine_load_ar(m, path);
}

int machine_load_bundle(machine m, const char *path) {
  struct stat sb;

  if(stat(path, &sb) != 0) return FALSE;

  if(S_ISDIR(sb.st_mode)) {
    return machine_load_directory(m, path);
  }

  return machine_load_rba(m, path);
}

void machine_setup_normal(machine m, int argc, char **argv) {
  machine_save_args(m, argc, argv);
  machine_setup_config(m);
  machine_config_env(m);
  machine_setup_standard_io(m);
}

int *machine_setup_thread(machine m, int argc, char **argv) {
  m->sub = TRUE;
  machine_save_args(m, argc, argv);
  machine_setup_config(m);
  machine_config_env(m);
  return machine_setup_piped_io(m);
}

