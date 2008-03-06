require File.dirname(__FILE__) + '/../../../spec_helper'
require File.dirname(__FILE__) + '/../fixtures/classes'

describe "TCPServer.new" do
  after(:each) do
    @server.close if @server && !@server.closed?
  end

  it "binds to a host and a port" do
    @server = TCPServer.new('127.0.0.1', SocketSpecs.port)
    addr = @server.addr
    addr[0].should == 'AF_INET'
    addr[1].be_kind_of Fixnum
    # on some platforms (Mac), MRI
    # returns comma at the end.
    addr[2].should =~ /^localhost\b/
    addr[3].should == '127.0.0.1'
  end

  it "binds to localhost and a port with either IPv4 or IPv6" do
    @server = TCPServer.new('localhost', SocketSpecs.port)
    addr = @server.addr
    if addr[0] == 'AF_INET'
      addr[1].be_kind_of Fixnum
      addr[2].should =~ /^localhost\b/
      addr[3].should == '127.0.0.1'
    else
      addr[1].be_kind_of Fixnum
      addr[2].should =~ /^localhost\b/
      addr[3].should == '::1'
    end
  end

  it "coerces port to string, then determines port from that number or service name" do
    t = Object.new
    lambda { TCPServer.new('localhost', t) }.should raise_error(TypeError)

    def t.to_str; SocketSpecs.port.to_s; end
    
    @server = TCPServer.new('localhost', t)
    addr = @server.addr
    addr[1].should == SocketSpecs.port

    # TODO: This should also accept strings like 'https', but I don't know how to
    # pick such a service port that will be able to reliably bind...
  end
end
