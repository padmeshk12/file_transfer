#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2023
#
# AUTHORS:  Zuria Zhu
#
#------------------------------------------------------------

OPT_DIR = '/opt/hp93000'.freeze
OPT_LINK = "#{OPT_DIR}/soc".freeze

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
def kill_process(pnames)
  pnames.each do |pname|
    output = `ps -ef | grep #{pname} | grep -v 'grep\\|gvim\\|ruby'` 
    next if output.empty?
    pid = output.split(/\s+/)[1]
    `kill -9 #{pid}` unless pid.nil? && pid.empty? 
  end
end

#############################################################
# Main
#############################################################
begin
  xoc_workspace = ENV["WORKSPACE"]
  testbed_path = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler/test/testbed"
  scripts_path = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler/test/scripts"
  
  tc_client_package = testbed_path + "/tc_client_package/tc_client_package" 
  workspace = testbed_path + "/workspace" 
  tp_path = "sampleproject/src/exampleTests/Example.prog"
  tp_path_2 = "sampleproject2/src/exampleTests/Example.prog"
  log_path = "/tmp/messages"
  
  # check smartest version
  check_current_smartest8_version
  
  # load driver_path config_path simulator_path simulator_option
  $driver_path = ""
  $config_path = ""
  $simulator_path = ""
  $simulator_option = ""
  driver_param = File.expand_path("../scripts/driverParam.rb", __FILE__)
  load driver_param
  
  ##############################################################################################################
  # ART_TC1: 
  # @Covers VC1 of Software Requirement(270788): the GPIB connection test between simulator and driver.
  # @Covers VC1 of Software Requirement(267005): the basic functions of Atom handler driver.
  # @Covers VC1 of Software Requirement(270638): the tests of offline simulator.
  # @TestCase
  # @Refs(requirements={"https://advantest.codebeamer.com/issue/270788", "https://advantest.codebeamer.com/issue/267005", "https://advantest.codebeamer.com/issue/270638"}) 
  ##############################################################################################################  
  #build workspace
  tplc_command = scripts_path + "/doTplc.sh" + " " + workspace + " /tmp/.tplc_#{ENV["USER"]} 1>/dev/null 2>&1 &"
  system(tplc_command)
  
  # stop simulator
  puts ">> Stop PH simulator"
  $pnames = ['simulator', 'hp93000_PH_guiProcess', 'vxi11']
  kill_process $pnames 
  
  # start simulator
  puts ">> Start PH simulator"
  simulator_run = xoc_workspace + "/system/phcontrol/bin/run"
  run_simulator_command = simulator_run + " " +  $simulator_path + " " + $simulator_option
  system(run_simulator_command)
  
  # run test
  puts ">>Case 1 Test Start..."
  tcct_run = xoc_workspace + "/system/bin/run"
  run_test_command = tcct_run + " " + tc_client_package + " " + workspace + " " + tp_path + " " + $driver_path + " " + $config_path
  rtn = system(run_test_command)
  puts ">> Test Completed."
  unless rtn == true
    raise "ERROR: Run test failed."
  end
  
  ##############################################################################################################
  # ART_TC2: 
  # @Covers VC2 of Software Requirement(267005): the basic functions of Atom handler driver.
  # @TestCase
  # @Refs(requirements={"https://advantest.codebeamer.com/issue/267005"}) 
  ############################################################################################################## 
  #build workspace
  tplc_command = scripts_path + "/doTplc.sh" + " " + workspace + " /tmp/.tplc_#{ENV["USER"]} 1>/dev/null 2>&1 &"
  system(tplc_command)
  
  # stop simulator
  puts ">> Stop PH simulator"
  $pnames = ['simulator', 'hp93000_PH_guiProcess', 'vxi11']
  kill_process $pnames 
  
  # start simulator
  puts ">> Start PH simulator"
  simulator_run = xoc_workspace + "/system/phcontrol/bin/run"
  run_simulator_command = simulator_run + " " +  $simulator_path + " " + $simulator_option
  system(run_simulator_command)
  
  # run test
  puts ">>Case 2 Test Start..."
  tcct_run = xoc_workspace + "/system/bin/run"
  run_test_command = tcct_run + " " + tc_client_package + " " + workspace + " " + tp_path_2 + " " + $driver_path + " " + $config_path
  rtn = system(run_test_command)
  puts ">> Test Completed."
  unless rtn == true
    raise "ERROR: Run test failed."
  end
  
  # check the binning result
  match_string="will bin device at handler site"
  match_word_1="hardbin"
  match_word_2="handler bin"
  File.open(log_path,'r') do |log|
    log.each_line do |line|
     if line.include?(match_string)
        if line.include?(match_word_1)
            pattern_1=/#{match_word_1}\s+(\d+)/
            string_hardbin_number=line.match(pattern_1)
            if string_hardbin_number
                hardbin_number=string_hardbin_number[1].to_i
                if hardbin_number!=1
                    pattern_2=/#{match_word_2}\s+\"(\d+)\"/
                    string_handlerbin_number=line.match(pattern_2)
                    if string_handlerbin_number
                        handlerbin_number=string_handlerbin_number[1].to_i
                        if handlerbin_number!=2
                            raise "ERROR: Bin unconfigured SMT bin failed."
                        end
                    end
                end
            end
            
        end
        break
     end
    end
  end
  
  
rescue => ex
  puts ex.message
  exit 1
ensure
  # stop simulator
  kill_process $pnames
end