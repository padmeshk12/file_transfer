#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2023
#
# CONTENTS: Utils 
#
# AUTHORS:  Jon Jin
#
#------------------------------------------------------------
OPT_DIR = '/opt/hp93000'.freeze
OPT_LINK = "#{OPT_DIR}/soc".freeze
XOC_WORKSPACE =ENV["WORKSPACE"].freeze
TESTBED_PATH = "#{XOC_WORKSPACE}/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericProber/test/testbed".freeze
SCRIPTS_PATH = "#{XOC_WORKSPACE}/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericProber/test/scripts".freeze
TC_CLIENT_WAFER = "#{TESTBED_PATH}/tc_client_wafer/tc_client_wafer".freeze
WORKSPACE = "#{TESTBED_PATH}/workspace".freeze
TP_PATH = "sampleproject/src/exampleTests/Example.prog".freeze
PNAMES = ['simulator', 'hp93000_PH_guiProcess', 'vxi11'].freeze

#############################################################
# check smartest 8 version
#############################################################
def check_current_smartest8_version 
  unless File.exists?(OPT_LINK)
    raise "ERROR: #{OPT_LINK} does not exist."
  end
  if File.symlink?(OPT_LINK) && File.directory?(OPT_LINK)
    current_version =  File.basename(File.readlink(OPT_LINK))
    unless current_version.gsub(/soc64_/, "")[0] == "8"
      raise "ERROR: Current smartest version is: #{current_version}, please switch to smartest 8."
    end
  else
    raise "ERROR: #{OPT_LINK} is not a symbolic link. Unable to get current smartest version."
  end
end

#************************************************************
# kill process
#************************************************************
def kill_process
  PNAMES.each do |pname|
    output = `ps -ef | grep #{pname} | grep -v 'grep\\|gvim\\|ruby'` 
    next if output.empty?
    pid = output.split(/\s+/)[1]
    `kill -9 #{pid}` unless pid.nil? && pid.empty? 
  end
end

