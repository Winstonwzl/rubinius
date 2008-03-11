# depends on: class.rb

class IO

  ivar_as_index :__ivars__ => 0, :descriptor => 1, :buffer => 2, :mode => 3

  BufferSize = 8096

  class Buffer < String
    ivar_as_index :bytes => 0, :characters => 1, :encoding => 2, :data => 3, :hash => 4, :shared => 5

    # Create a buffer of +size+ bytes. The buffer contains an internal Channel
    # object it uses to fill itself.
    def initialize(size)
      @data = ByteArray.new(size)
      @bytes = 0
      @characters = 0
      @encoding = :buffer

      @total = size
      @channel = Channel.new
    end

    # Block until the buffer receives more data
    def process
      @channel.receive
    end

    attr_reader :channel

    # Indicates how many bytes are left
    def unused
      @total - @bytes
    end

    # Remove +count+ bytes from the front of the buffer and return them.
    # All other bytes are moved up.
    def shift_front(count)
      count = @bytes if count > @bytes

      str = String.allocate
      str.initialize_from count, @data.fetch_bytes(0, count)

      rest = @bytes - count

      @data.move_bytes count, rest, 0

      @bytes -= count

      return str
    end

    # Empty the contents of the Buffer into a String object and return it.
    def as_str
      str = String.allocate
      str.initialize_from @bytes, @data
      @data = ByteArray.new(@total)
      @bytes = 0
      return str
    end

    def empty?
      @bytes == 0
    end

    # Indicates if the Buffer has no more room.
    def full?
      @total == @bytes
    end

    # Empty the buffer.
    def reset!
      @bytes = 0
    end

    # Fill the buffer from IO object +io+. The buffer requests +unused+
    # bytes, but may not receive that many. Any new data causes this to
    # return.
    def fill_from(io)
      Scheduler.send_on_readable @channel, io, self, unused()
      obj = @channel.receive
      if obj.kind_of? Class
        raise IOError, "error occured while filling buffer (#{obj})"
      end

      io.eof! unless obj

      return obj
    end

    def inspect # :nodoc:
      "#<IO::Buffer:0x%x total=%p bytes=%p characters=%p data=%p>" % [
        object_id, @total, @bytes, @characters, @data
      ]
    end

    # Match the buffer against Regexp +reg+, and remove bytes starting
    # at the beginning of the buffer, up to the end of where the Regexp
    # matched.
    def clip_to(reg)
      if m = reg.match(self)
        idx = m.end(0)
        return shift_front(idx)
      else
        nil
      end
    end
  end

  module Constants
    F_GETFL  = Rubinius::RUBY_CONFIG['rbx.platform.fcntl.F_GETFL']
    F_SETFL  = Rubinius::RUBY_CONFIG['rbx.platform.fcntl.F_SETFL']
    ACCMODE  = Rubinius::RUBY_CONFIG['rbx.platform.fcntl.O_ACCMODE']

    SEEK_SET = Rubinius::RUBY_CONFIG['rbx.platform.io.SEEK_SET']
    SEEK_CUR = Rubinius::RUBY_CONFIG['rbx.platform.io.SEEK_CUR']
    SEEK_END = Rubinius::RUBY_CONFIG['rbx.platform.io.SEEK_END']

    RDONLY   = Rubinius::RUBY_CONFIG['rbx.platform.file.O_RDONLY']
    WRONLY   = Rubinius::RUBY_CONFIG['rbx.platform.file.O_WRONLY']
    RDWR     = Rubinius::RUBY_CONFIG['rbx.platform.file.O_RDWR']

    CREAT    = Rubinius::RUBY_CONFIG['rbx.platform.file.O_CREAT']
    EXCL     = Rubinius::RUBY_CONFIG['rbx.platform.file.O_EXCL']
    NOCTTY   = Rubinius::RUBY_CONFIG['rbx.platform.file.O_NOCTTY']
    TRUNC    = Rubinius::RUBY_CONFIG['rbx.platform.file.O_TRUNC']
    APPEND   = Rubinius::RUBY_CONFIG['rbx.platform.file.O_APPEND']
    NONBLOCK = Rubinius::RUBY_CONFIG['rbx.platform.file.O_NONBLOCK']
    SYNC     = Rubinius::RUBY_CONFIG['rbx.platform.file.O_SYNC']

    # TODO: these flags should probably be imported from Platform
    LOCK_SH  = 0x01
    LOCK_EX  = 0x02
    LOCK_NB  = 0x04
    LOCK_UN  = 0x08
    BINARY   = 0x04
  end
  include Constants
  
  def self.select(read_array, write_array = nil, error_array = nil,
                  timeout = nil)
    chan = Channel.new

    if read_array then
      read_array.each do |readable|
        Scheduler.send_on_readable chan, readable, nil, nil
      end
    end

    raise NonImplementedError, "write_array is not supported" if write_array
    raise NonImplementedError, "error_array is not supported" if error_array

    # HACK can't do this yet
    #if write_array then
    #  write_array.each do |writable|
    #    Scheduler.send_on_writable chan, writable, nil, nil
    #  end
    #end
    #
    #if errore_array then
    #  errore_array.each do |errorable|
    #    Scheduler.send_on_error chan, errorable, nil, nil
    #  end
    #end

    Scheduler.send_in_microseconds chan, (timeout * 1_000_000).to_i if timeout

    value = chan.receive

    return nil if value == 1 # timeout

    io = read_array.find { |readable| readable.fileno == value }

    return nil if io.nil?

    [[io], [], []]
  end

  ##
  # Creates a new IO object to access the existing stream referenced by the
  # descriptor given. The stream is not copied in any way so anything done on
  # one IO will affect any other IOs accessing the same descriptor.
  #
  # The mode string given must be compatible with the original one so going
  # 'r' from 'w' cannot be done but it is possible to go from 'w+' to 'r', for
  # example (since the stream is not being "widened".)
  #
  # The initialization will verify that the descriptor given is a valid one.
  # Errno::EBADF will be raised if that is not the case. If the mode is
  # incompatible, it will raise Errno::EINVAL instead.

  def initialize(fd, mode)
    fd = Type.coerce_to fd, Integer, :to_int

    # Descriptor must be an open and valid one
    raise Errno::EBADF, "Invalid descriptor #{fd}" if fd < 0

    cur_mode = Platform::POSIX.fcntl(fd, F_GETFL, 0)
    raise Errno::EBADF, "Invalid descriptor #{fd}" if cur_mode < 0

    # Must support the desired mode.
    # O_ACCMODE is /undocumented/ for fcntl() on some platforms
    # but it should work. If there is a problem, check it though.
    new_mode = IO.parse_mode(mode) & ACCMODE
    cur_mode = cur_mode & ACCMODE

    if cur_mode != RDWR and cur_mode != new_mode
      raise Errno::EINVAL, "Invalid mode '#{mode}' for existing descriptor #{fd}"
    end

    setup fd, mode
  end

  def self.open(*args)
    io = self.new(*args)

    return io unless block_given?

    begin
      yield io
    ensure
      io.close unless io.closed?
    end
  end

  def self.sysopen(path, mode = "r", perm = 0666)
    if mode.kind_of?(String)
      mode = parse_mode(mode)
    end

    return open_with_mode(path, mode, perm)
  end

  def self.for_fd(fd)
    self.new(fd)
  end

  def self.pipe
    lhs = IO.allocate
    rhs = IO.allocate
    out = create_pipe(lhs, rhs)
    lhs.setup
    rhs.setup
    return [lhs, rhs]
  end

  ##
  # Obtains a new duplicate descriptor for the current one.

  def initialize_copy(original) # :nodoc:
    @descriptor = Platform::POSIX.dup(@descriptor)
  end

  def setup(desc = nil, mode = nil)
    @descriptor = desc if desc
    @mode = mode if mode
    @buffer = IO::Buffer.new(BufferSize)
    @eof = false
    @lineno = 0
  end

  private :initialize_copy

  attr_accessor :lineno

  alias_method :isatty, :tty?

  def self.popen(str, mode = "r")
    raise "TODO make this support more than r" if mode != "r"

    if str == "+-+" and !block_given?
      raise ArgumentError, "this mode requires a block currently"
    end

    pa_read, ch_write = IO.pipe

    pid = Process.fork

    if pid
      ch_write.close
      # See bottom for definition
      rp = BidirectionalPipe.new(pid, pa_read, nil)
      if block_given?
        begin
          yield rp
        ensure
          pa_read.close
        end
      else
        return rp
      end
    else
      pa_read.close
      STDOUT.reopen ch_write
      if str == "+-+"
        yield nil
      else
        Process.replace "/bin/sh", ["sh", "-c", str]
      end
    end
  end

  def binmode
    # HACK what to do?
  end

  def eof!
    @eof = true
  end

  def eof?
    raise IOError if closed? # HACK only for read
    read 0 # HACK force check
    @eof and @buffer.empty?
  end

  def getc
    raise IOError if closed? # HACK only for read
    char = read 1
    return nil if char.nil?
    char[0]
  end

  def wait_til_readable
    chan = Channel.new
    Scheduler.send_on_readable chan, self, nil, nil
    chan.receive
  end

  def __ivars__ ; @__ivars__  ; end

  def inspect
    "#<#{self.class}:0x#{object_id.to_s(16)}>"
  end

  def fileno
    @descriptor
  end

  alias_method :to_i, :fileno

  def printf(fmt, *args)
    write Sprintf.new(fmt, *args).parse
  end

  def puts(*args)
    if args.empty?
      write DEFAULT_RECORD_SEPARATOR
    else
      args.each do |arg|
        if arg.nil?
          str = "nil"
        elsif RecursionGuard.inspecting?(arg)
          str = "[...]"
        elsif arg.kind_of?(Array)
          RecursionGuard.inspect(arg) do
            arg.each do |a|
              puts a
            end
          end
        else
          str = arg.to_s
        end

        if str
          write str
          write DEFAULT_RECORD_SEPARATOR unless str.suffix?(DEFAULT_RECORD_SEPARATOR)
        end
      end
    end

    nil
  end

  ##
  # Writes each given argument.to_s to the stream or $_ (the result of last
  # IO#gets) if called without arguments. Appends $\.to_s to output. Returns
  # nil.

  def print(*args)
    if args.empty?
      write $_.to_s
    else
      args.each {|o| write o.to_s }
    end

    write $\.to_s
    nil
  end

  def <<(obj)
    write(obj.to_s)
    return self
  end

  def sysread(size, buf=nil)
    raise IOError if closed?
    buf = String.new(size) unless buf
    chan = Channel.new
    Scheduler.send_on_readable chan, self, buf, size
    raise EOFError if chan.receive.nil?
    return buf
  end

  alias_method :readpartial, :sysread

  def breadall(buffer=nil)
    return "" if @eof and @buffer.empty?

    output = ''

    buf = @buffer

    while true
      bytes = buf.fill_from(self)

      if !bytes or buf.full?
        output << buf
        buf.reset!
      end

      break unless bytes
    end

    if buffer then
      buffer = StringValue buffer
      buffer.replace output
    else
      buffer = output
    end

    buffer
  end

  def read(size=nil, buffer=nil)
    raise IOError if closed?
    return breadall(buffer) unless size

    return nil if @eof and @buffer.empty?

    buf = @buffer

    output = ''

    needed = size

    while true
      bytes = buf.fill_from(self)

      if bytes
        done = needed - bytes <= 0
      else
        done = true
      end

      if done or buf.full?
        output << buf.shift_front(needed)
        needed = size - output.length
      end

      break if done or needed == 0
    end

    if buffer then
      buffer = StringValue buffer
      buffer.replace output
    else
      buffer = output
    end

    buffer
  end

  alias_method :prim_write, :write
  
  def write(data)
    # If we have buffered data, rewind.
    unless @buffer.empty?
      seek 0, SEEK_CUR
    end

    data = String data

    prim_write(data)
  end
  
  alias_method :syswrite, :write

  def seek(amount, whence=SEEK_SET)
    # Unseek the still buffered amount
    unless @buffer.empty?
      prim_seek -@buffer.size, SEEK_CUR
      @buffer.reset!
      @eof = false
    end

    prim_seek amount, whence
  end

  def pos
    seek 0, SEEK_CUR
  end

  alias_method :tell, :pos

  def pos=(offset)
    seek offset, SEEK_SET
  end

  def rewind
    seek 0
    @lineno = 0
    @eof = false
    return 0
  end

  def close()
    raise IOError, "Instance of IO already closed" if closed?

    io_close or raise SystemCallError, "Invalid file descriptor"
  end

  def descriptor
    @descriptor
  end

  def sysseek(amount, whence=SEEK_SET)
    Platform::POSIX.lseek(@descriptor, amount, whence)
  end
    
  def closed?
    @descriptor == -1
  end

  ##
  #--
  # The current implementation does no write buffering, so we're always in
  # sync mode.

  def sync=(v)
  end

  def sync
    true
  end

  def flush
    true
  end

  def fsync
    err = Platform::POSIX.fsync @descriptor

    Errno.handle 'fsync(2)' if err < 0

    err
  end

  def gets(sep=$/)
    @lineno += 1
    $_ = gets_helper(sep)
  end

  def readline(sep=$/)
    out = gets(sep)
    raise EOFError, "end of file" unless out
    return out
  end

  def each(sep=$/)
    while line = gets_helper(sep)
      yield line
    end
  end

  alias_method :each_line, :each

  ##
  #--
  # Several methods use similar rules for reading strings from IO, but differ
  # slightly. This helper is an extraction of the code.

  def gets_helper(sep=$/)
    raise IOError if closed?
    return nil if @eof and @buffer.empty?

    return breadall() unless sep

    buf = @buffer

    if sep.empty?
      return gets_stripped($/ + $/)
    end
    
    reg = /#{sep}/m

    if str = buf.clip_to(reg)
      return str
    end

    # Do an initial fill.
    return nil if !buf.fill_from(self) and buf.empty?

    output = nil
    while true
      if str = buf.clip_to(reg)
        if output
          return output + str
        else
          return str
        end
      end

      if !buf.fill_from(self)
        if buf.empty?
          rest = nil
        else
          rest = buf.as_str
        end

        if output
          if rest
            return output << buf.as_str
          else
            return output
          end
        else
          return rest
        end
      end

      if buf.full?
        if output
          output << buf
          buf.reset!
        else
          output = buf.as_str
        end
      end
    end
  end

  def gets_stripped(sep)
    buf = @buffer

    if m = /^\n+/m.match(buf)
      buf.shift_front(m.end(0)) if m.begin(0) == 0
    end

    str = gets_helper(sep)

    if m = /^\n+/m.match(buf)
      buf.shift_front(m.end(0)) if m.begin(0) == 0
    end

    return str
  end

  def readlines(sep=$/)
    ary = Array.new
    while line = gets(sep)
      ary << line
    end
    return ary
  end
  
  def self.readlines(name, sep_string = $/)
    io = File.open(StringValue(name), 'r')
    return if io.nil?

    begin
      io.readlines(sep_string)
    ensure
      io.close
    end
  end

  def self.foreach(name, sep_string = $/,&block)
    io = File.open(StringValue(name), 'r')
    sep = StringValue(sep_string)
    begin
      while(line = io.gets(sep))
        yield line
      end
    ensure
      io.close
    end
  end

  def self.read(name, length = Undefined, offset = 0)
    name = StringValue(name)

    offset = Type.coerce_to(offset, Fixnum, :to_int)

    if offset < 0
      raise Errno::EINVAL, "offset must not be negative"
    end

    unless length.equal?(Undefined)
      length = Type.coerce_to(length, Fixnum, :to_int)

      if length < 0
        raise ArgumentError, "length must not be negative"
      end
    end

    File.open(name) do |f|
      f.seek(offset) unless offset.zero?

      if length.equal?(Undefined)
        f.read
      else
        f.read(length)
      end
    end
  end

  def fcntl(command, arg)
    if arg.kind_of? Fixnum then
      Platform::POSIX.fcntl(descriptor, command, arg)
    else
      raise NotImplementedError, "cannot handle #{arg.class}"
    end
  end

  def self.parse_mode(mode)
    ret = 0

    case mode[0]
    when ?r
      ret |= RDONLY
    when ?w
      ret |= WRONLY | CREAT | TRUNC
    when ?a
      ret |= WRONLY | CREAT | APPEND
    else
      raise ArgumentError, "invalid mode -- #{mode}"
    end

    return ret if mode.length == 1

    case mode[1]
    when ?+
      ret &= ~(RDONLY | WRONLY)
      ret |= RDWR
    when ?b
      ret |= BINARY
    else
      raise ArgumentError, "invalid mode -- #{mode}"
    end
  
    return ret if mode.length == 2

    case mode[2]
    when ?+
      ret &= ~(RDONLY | WRONLY)
      ret |= RDWR
    when ?b
      ret |= BINARY
    else
      raise ArgumentError, "invalid mode -- #{mode}"
    end

    ret
  end
  
  private :io_close

  def self.after_loaded()
    # Nothing to do right now
  end
end

class BidirectionalPipe < IO
  def initialize(pid, read, write)
    super(read.fileno, 'r')
    @pid = pid
    @write = write
  end

  def pid
    @pid
  end

  def <<(str)
    @write << str
  end

  alias_method :write, :<<

  def syswrite(str)
    @write.syswrite str
  end
end
