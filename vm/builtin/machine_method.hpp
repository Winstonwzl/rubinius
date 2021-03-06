#ifndef RBX_BUILTIN_MACHINE_METHOD
#define RBX_BUILTIN_MACHINE_METHOD

#include "assembler/jit.hpp"
#include "assembler/code_map.hpp"
#include "assembler/relocation.hpp"

namespace rubinius {
  class MachineMethod : public Object {
  public:
    const static object_type type = MachineMethodType;

  private:
    VMMethod* vmmethod_;
    size_t code_size_;
    CodeMap* virtual2native_;
    assembler::Relocations* relocations_;

    void* function_;

  public:
    static void init(STATE);
    static MachineMethod* create(STATE, VMMethod* vmm, JITCompiler& jit);

    void* function() {
      return reinterpret_cast<void*>(function_);
    }

    void set_function(void* p) {
      function_ = p;
    }

    // Used for debugging. Gives us a place to break on before entering jit'd code
    static void run_code(VMMethod* const vmm, Task* const task, MethodContext* const ctx);

    // Ruby.primitive :machine_method_show
    Object* show();

    // Ruby.primitive :machine_method_activate
    Object* activate();
  };
}

#endif
