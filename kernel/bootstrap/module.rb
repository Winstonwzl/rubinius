# depends on: vm.rb compiled_method.rb

class Module
  def method_table   ; @method_table ; end
  def constant_table ; @constants    ; end
  def encloser       ; @encloser     ; end
  def name           ; @name.to_s    ; end

  def self.allocate
    Ruby.primitive :module_allocate
    raise PrimitiveFailure, "Module.allocate primitive failed"
  end

  # Ultra simple private
  def private(name)
    if entry = @method_table[name]
      unless entry.kind_of? Executable
        entry.visibility = :private
      else
        cmv = CompiledMethod::Visibility.new entry, :private
        @method_table[name] = cmv
      end
    end
  end

  # Ultra simple protected
  def protected(name)
    if entry = @method_table[name]
      unless entry.kind_of? Executable
        entry.visibility = :protected
      else
        cmv = CompiledMethod::Visibility.new entry, :protected
        @method_table[name] = cmv
      end
    end
  end

  # Ultra simple module_function
  def module_function(name)
    if cm = @method_table[name]
      if cm.kind_of? Tuple
        cm = cm[1]
      end
      meta = class << self; self; end
      meta.method_table[name] = cm
      private name
    end
  end

  def __find_method(namesym)
    Ruby.primitive :find_method
    raise PrimitiveFailure, "primitive failed"
  end

  def alias_method(new_name, current_name)
    unless meth = @method_table[current_name]
      mod = direct_superclass()
      while !meth and mod
        meth = mod.method_table[current_name]
        mod = mod.direct_superclass
      end
    end

    unless meth
      raise NoMethodError, "Unable to find method '#{current_name}' to alias to '#{new_name}'"
    end
    @method_table[new_name] = meth
    Rubinius::VM.reset_method_cache(new_name)
  end

  # 'superclass' method defined in class.rb,
  # because it is more complex than a mere accessor
  def superclass=(other)
    @superclass = other
  end

  # This may be either an included Module or then
  # an inherited Class.
  def direct_superclass
    @superclass
  end

  def append_features(mod)
    im = IncludedModule.new(self)
    im.attach_to mod
  end

  def included(mod); end

  def include(mod)
    mod.append_features(self)
    mod.included(self)
    self
  end

  def attr_reader(name)
    meth = AccessVariable.get_ivar name
    @method_table[name] = meth
    return nil
  end

  def attr_writer(name)
    meth = AccessVariable.set_ivar name
    @method_table["#{name}=".to_sym] = meth
    return nil
  end

  def attr_accessor(name)
    attr_reader(name)
    attr_writer(name)
    return true
  end

end

