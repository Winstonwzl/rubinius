require File.dirname(__FILE__) + '/../../spec_helper'

describe "LookupTable#size" do
  it "returns the number of key, value pairs in the LookupTable" do
    lt = LookupTable.new
    lt.size.should == 0
    lt[:a] = 1
    lt.size.should == 1
    lt[:b] = 2
    lt.size.should == 2
    lt.delete :b
    lt.size.should == 1
    lt[:c] = 3
    lt.size.should == 2
  end
end
