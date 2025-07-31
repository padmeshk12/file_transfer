#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2022
#
# CONTENTS: Main function
#
# AUTHORS:  Zoyi Yu
#
#------------------------------------------------------------


def check_happy_case 
  puts ">> Stop PH simulator" 
  kill_process  
  
  # start simulator
  puts ">> Start PH simulator"
  simulator_run = XOC_WORKSPACE + "/system/phcontrol/bin/run"
  run_simulator_command = simulator_run + " " +  $simulator_path + " " + $simulator_option
  system(run_simulator_command)
  
  # run test
  puts ">> Test Start..."
  tcct_run = XOC_WORKSPACE + "/system/bin/run"
  run_test_command = tcct_run + " " + TC_CLIENT_WAFER + " " + WORKSPACE + " " + TP_PATH + " " + $driver_path + " " + $config_path
  rtn = system(run_test_command)  
  unless rtn == true
    raise "ERROR: Run test failed."
  end
  puts ">> Test Completed."
end


##############################################################################################################
# ART_TC1: 
# @Covers VC1 of Software Requirement(276005): Check Help Information.
# @TestCase
# @Refs(requirements={"https://advantest.codebeamer.com/issue/276005"}) 
############################################################################################################## 
def check_help_Info 
  puts ">> Stop PH simulator" 
  kill_process  
  simulator_run = XOC_WORKSPACE + "/system/phcontrol/bin/run"
  run_simulator_command = simulator_run + " " +  $simulator_path + " -h > " + TESTBED_PATH +  "/outputfile/helpinfo 2>&1  " 
  system(run_simulator_command)
  run_compare_command = "diff " +   TESTBED_PATH + "/outputfile/helpinfo " + TESTBED_PATH + "/goldenfile/helpinfo > /dev/null 2>&1 "
  rtn = system(run_compare_command)  
  unless rtn == true
    raise "ERROR: helpInfo not as expected."
  end
  puts ">> Test Completed."
end



#############################################################
# Main
#############################################################
utils = File.expand_path("../../../test/scripts/utils.rb", __FILE__)
load utils
driver_param = File.expand_path("../scripts/driverParam.rb", __FILE__)
load driver_param
begin   
  check_current_smartest8_version
  #build workspace
  tplc_command = SCRIPTS_PATH + "/doTplc.sh" + " " + WORKSPACE + " /tmp/.tplc_#{ENV["USER"]} 1>/dev/null 2>&1 &"
  system(tplc_command)
  # stop case1
  puts ">> Start check_happy_case."
  check_happy_case
  puts ">> Start check_help_Info."
  check_help_Info
 
  
  
rescue => ex
  puts ex.message
  exit 1
ensure
  # stop simulator
  kill_process 
end