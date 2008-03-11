require File.dirname(__FILE__) + '/../spec_helper'
require File.dirname(__FILE__) + '/subtend_helper'

compile_extension('subtend_safe_level')
require File.dirname(__FILE__) + '/ext/subtend_safe_level'

describe "SubtendSafeLevel" do
  before :each do
    @f = SubtendSafeLevel.new
  end
  
  it "has a default safe level of 0" do
    @f.sb_safe_level.should == 0
  end

  it "throws an error when rb_secure is called with argument >= SAFE" do
    lambda { @f.sb_secure(0) }.should raise_error(SecurityError)
  end

end
