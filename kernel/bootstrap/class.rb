# TODO - Needs ivar_as_index removal cleanup

class Class

  def self.allocate
    Ruby.primitive :class_s_allocate
    raise PrimitiveFailure, "Unable to create a new Class"
  end

  def allocate
    Ruby.primitive :class_allocate
    raise RuntimeError, "primitive 'class_allocate' failed on #{self.inspect}"
  end

  def new(*args)
    obj = allocate()

    Rubinius.asm(args, obj) do |args, obj|
      run obj
      run args
      push_block
      send_with_splat :initialize, 0, true
      pop
    end

    return obj
  end

  def instance_fields; @instance_fields ; end
  def instance_flags ; @instance_flags  ; end

  def instance_fields=(num)
    @instance_fields = num
  end

  def instance_type=(type)
    @instance_type = type
  end

  def instance_type
    @instance_type
  end
end

class RuntimePrimitive < Executable
  def initialize(prim_name)
    @primitive = prim_name
    @serial = 0
  end

  def variable=(var)
    @variable = var
  end
end

class AccessVariable
  def self.allocate
    Ruby.primitive :accessvariable_allocate
    raise PrimitiveFailure, "Unable to allocate"
  end

  def initialize(variable, write)
    @primitive = nil
    @serial = 0
    @name = "@#{variable}".to_sym
    @write = write
  end

  def self.get_ivar(name)
    new(name, false)
  end

  def self.set_ivar(name)
    new(name, true)
  end
end

##
# This is here because it uses instance_fields, which is defined above.  This
# is the ONLY way to force load order in the bootstrap (and the only instance
# of load order mattering)

class IncludedModule < Module
  def superclass; @superclass ; end
  def module    ; @module     ; end

  def self.allocate
    Ruby.primitive :included_module_allocate
    raise PrimitiveFailure, "IncludedModule.allocate primitive failed"
  end

  def initialize(mod)
    @method_table = mod.method_table
    @method_cache = nil
    @name = nil
    @constants = mod.constant_table
    @encloser = mod.encloser
    @module = mod
  end

  def attach_to(cls)
    @superclass = cls.direct_superclass
    cls.superclass = self
  end

  def name
    @module.name
  end

  def to_s
    @module.to_s
  end
end
