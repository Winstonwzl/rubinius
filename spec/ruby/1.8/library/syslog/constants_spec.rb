require File.dirname(__FILE__) + '/../../spec_helper'
require 'syslog'

describe "Syslog::Constants" do
  it 'should be included' do
    Syslog::Constants::LOG_USER.should == Syslog::LOG_USER
    Syslog::Constants::LOG_EMERG.should == Syslog::LOG_EMERG
    Syslog::Constants::LOG_CRIT.should == Syslog::LOG_CRIT
    Syslog::Constants::LOG_ERR.should == Syslog::LOG_ERR
    Syslog::Constants::LOG_MAIL.should == Syslog::LOG_MAIL
    Syslog::Constants::LOG_WARNING.should == Syslog::LOG_WARNING
  end
end