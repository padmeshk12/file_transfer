#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2023
#
# AUTHORS:  Runqiu Cao
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

#************************************************************
# match all strings of list in file
# returned value: 
#     success: 0
#     failure: array of string numbers
#************************************************************
def match_strings_in_file(file_name, string_list)
  file = File.open(file_name, 'r')
  file_content = file.read
  file.close

  failed_strings = []
  string_list.each do |str|
    unless file_content.include?(str)
      failed_strings << str
    end
  end

  failed_strings.empty? ? [] : failed_strings
end


#############################################################
# Main
#############################################################
begin
  ENV['DRIVER_IS_VERIFIED_WITH_REAL_HANDLER'] = 'true'
  xoc_workspace = ENV["WORKSPACE"]
  testbed_path = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler/test/testbed"
  scripts_path = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler/test/scripts"
  
  tc_client_recipe = testbed_path + "/tc_client_recipe/tc_client_recipe" 
  workspace = testbed_path + "/workspace" 
  
  # check smartest version
  check_current_smartest8_version
  
  # load driver_path config_path simulator_path simulator_option
  $driver_path = ""
  $config_path = ""
  $simulator_path = ""
  $simulator_option1 = ""
  $simulator_option2 = ""
  driver_param = File.expand_path("../scripts/driverParam.rb", __FILE__)
  load driver_param

  #build workspace
  tplc_command = scripts_path + "/doTplc.sh" + " " + workspace + " /tmp/.tplc_#{ENV["USER"]} 1>/dev/null 2>&1 &"
  puts tplc_command
  system(tplc_command)

  driver_string_list = []
  simulator_string_list = []
  #############################################################
  # ART_TC1:
  # @Covers VC1 of Software Requirement(270827): the TCP/IP connection test between simulator and driver.
  # @Covers VC1 and VC3 of Software Requirement(270828): the basic functions of Die Level handler driver.
  # @Covers VC1 and VC3 of Software Requirement(270829): the tests of offline simulator.
  # @Covers VC1 and VC2 of Software Requirement(270857): the tests of TTTTool commands.
  # @TestCase
  # @Refs(requirements={"https://advantest.codebeamer.com/issue/270827",
  # "https://advantest.codebeamer.com/issue/270828",
  # "https://advantest.codebeamer.com/issue/270829",
  # "https://advantest.codebeamer.com/issue/270857"}) 
  #############################################################
  puts ">> Test ART_TC1..."
  # stop simulator
  puts ">> Stop PH simulator"
  $pnames = ['simulator']
  kill_process $pnames 
  
  # start simulator
  puts ">> Start PH simulator"
  simulator_run = xoc_workspace + "/system/phcontrol/bin/run"
  run_simulator_command = $simulator_path + " " + $simulator_option1
  system(run_simulator_command)

  # run recipe
  puts ">> Run recipe"
  recipe_file = "./recipe/recipe_for_ART_TC1.xml"
  tcct_run = xoc_workspace + "/system/bin/run"
  run_test_command = tcct_run + " " + tc_client_recipe + " " + workspace + " " + recipe_file
  puts recipe_file
  rtn = system(run_test_command)
  unless rtn == true
    raise "ERROR: Run test failed."
  end

  puts ">> Validate log message"
  # Verify Software Requirement(270827): communication
  verify_vc1_of_270827_log1 = "Successfully connected to the server at address: 127.0.0.1:6688"
  verify_vc1_of_270827_log2 = "accept connection on host = 127.0.0.1"
  verify_vc1_of_270827_log3 = "server: establish a connection"
  verify_vc1_of_270827_log4 = "client: connect tester server successfully."
  driver_string_list.push(verify_vc1_of_270827_log1, verify_vc1_of_270827_log2)
  simulator_string_list.push(verify_vc1_of_270827_log3, verify_vc1_of_270827_log4)

  # Verify Software Requirement(270828): driver basic function
  verify_vc1_of_270828_log1 = "receive a message: StartRequest"
  verify_vc1_of_270828_log2 = "FULLSITES?\\r\\n"
  verify_vc1_of_270828_log3 = "ACK OK FULLSITES? Fullsites=00000003\\r\\n"
  verify_vc1_of_270828_log4 = "BINON Parameter=A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,1,1\\r\\n"
  verify_vc1_of_270828_log5 = "ACK OK BINON Parameter=A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,1,1\\r\\n"
  driver_string_list.push(verify_vc1_of_270828_log1, verify_vc1_of_270828_log2, 
      verify_vc1_of_270828_log3, verify_vc1_of_270828_log4, verify_vc1_of_270828_log5)

  # Verify Software Requirement(270829): simulator basic function
  verify_vc1_of_270829_log1 = "client: send message [StartRequest]"
  verify_vc1_of_270829_log2 = "client: received message [ACK StartRequest]"
  verify_vc1_of_270829_log3 = "server: received message [FULLSITES?]"
  verify_vc1_of_270829_log4 = "server: send message [ACK OK FULLSITES? Fullsites=00000003]"
  verify_vc1_of_270829_log5 = "server: received message [BINON Parameter=A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,1,1]"
  verify_vc1_of_270829_log6 = "server: send message [ACK OK BINON Parameter=A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,1,1]"
  verify_vc1_of_270829_log7 = "server: received message [ECHOOK]"
  verify_vc1_of_270829_log8 = "server: send message [ACK OK ECHOOK]"
  simulator_string_list.push(verify_vc1_of_270829_log1, verify_vc1_of_270829_log2, 
      verify_vc1_of_270829_log3, verify_vc1_of_270829_log4, verify_vc1_of_270829_log5, 
      verify_vc1_of_270829_log6, verify_vc1_of_270829_log7, verify_vc1_of_270829_log8)

  verify_vc3_of_270829_log1 = "server: received message [invalid_cmd]"
  verify_vc3_of_270829_log2 = "don't support the command [invalid_cmd]"
  simulator_string_list.push(verify_vc3_of_270829_log1, verify_vc3_of_270829_log2)

  # Verify Software Requirement(270857): TTTTool commands
  verify_vc1_of_270857_log1 = "ACK OK VERSION? ADVANTEST HAXXXX FULLSITEB Rev.1.0.3"
  verify_vc1_of_270857_log2 = "phFuncSetStatus, key ->TEMPLOGCYCLE<-, param ->Parameter=10<-"
  verify_vc1_of_270857_log3 = "privateSetStatus, TEMPLOGCYCLE returned: ACK OK TEMPLOGCYCLE Parameter=10"
  verify_vc1_of_270857_log4 = "phFuncGetStatus, key ->TEMPLOGCYCLE?<-, No Param"
  verify_vc1_of_270857_log5 = "privateGetStatus, TEMPLOGCYCLE? returned: ACK OK TEMPLOGCYCLE? Parameter=10"
  verify_vc1_of_270857_log6 = "phFuncSetStatus, key ->TEMPLOGLENGTH<-, param ->Parameter=100<-"
  verify_vc1_of_270857_log7 = "privateSetStatus, TEMPLOGLENGTH returned: ACK OK TEMPLOGLENGTH Parameter=100"
  verify_vc1_of_270857_log8 = "phFuncGetStatus, key ->TEMPLOGLENGTH?<-, No Param"
  verify_vc1_of_270857_log9 = "privateGetStatus, TEMPLOGLENGTH? returned: ACK OK TEMPLOGLENGTH? Parameter=100"
  verify_vc1_of_270857_log10 = "phFuncSetStatus, key ->TEMPLOGSTART<-, No Param"
  verify_vc1_of_270857_log11 = "privateSetStatus, TEMPLOGSTART returned: ACK OK TEMPLOGSTART"
  verify_vc1_of_270857_log12 = "phFuncSetStatus, key ->TEMPLOGSTOP<-, No Param"
  verify_vc1_of_270857_log13 = "privateSetStatus, TEMPLOGSTOP returned: ACK OK TEMPLOGSTOP"
  verify_vc1_of_270857_log14 = "phFuncGetStatus, key ->TEMPLOGSTS<-, No Param"
  verify_vc1_of_270857_log15 = "privateGetStatus, TEMPLOGSTS returned: ACK OK TEMPLOGSTS Parameter=1"
  verify_vc1_of_270857_log16 = "phFuncSetStatus, key ->TEMPLOGSEND<-, No Param"
  verify_vc1_of_270857_log17 = "privateSetStatus, TEMPLOGSEND returned: ACK OK TEMPLOGSEND Parameter=0"
  verify_vc1_of_270857_log18 = "phFuncSetStatus, key ->PRETRGINI<-, No Param"
  verify_vc1_of_270857_log19 = "privateSetStatus, PRETRGINI returned: ACK OK PRETRGINI"
  verify_vc1_of_270857_log20 = "phFuncSetStatus, key ->PRETRGENABLE<-, param ->Parameter=3,1<-"
  verify_vc1_of_270857_log21 = "privateSetStatus, PRETRGENABLE returned: ACK OK PRETRGENABLE Parameter=3,1"
  verify_vc1_of_270857_log22 = "phFuncSetStatus, key ->PRETRGSIGTYPE<-, param ->Parameter=3,3<-"
  verify_vc1_of_270857_log23 = "privateSetStatus, PRETRGSIGTYPE returned: ACK OK PRETRGSIGTYPE Parameter=3,3"
  verify_vc1_of_270857_log24 = "phFuncSetStatus, key ->PRETRGWAITTIME<-, param ->Parameter=3,500<-"
  verify_vc1_of_270857_log25 = "privateSetStatus, PRETRGWAITTIME returned: ACK OK PRETRGWAITTIME Parameter=3,500"
  verify_vc1_of_270857_log26 = "phFuncSetStatus, key ->PRETRGOUTTIME<-, param ->Parameter=4,421<-"
  verify_vc1_of_270857_log27 = "privateSetStatus, PRETRGOUTTIME returned: ACK OK PRETRGOUTTIME Parameter=4,421"
  verify_vc1_of_270857_log28 = "phFuncSetStatus, key ->PRETRGOUTVALUE<-, param ->Parameter=5,+125<-"
  verify_vc1_of_270857_log29 = "privateSetStatus, PRETRGOUTVALUE returned: ACK OK PRETRGOUTVALUE Parameter=5,+125"
  verify_vc1_of_270857_log30 = "phFuncSetStatus, key ->PRETRGRETMETHOD<-, param ->Parameter=6,1<-"
  verify_vc1_of_270857_log31 = "privateSetStatus, PRETRGRETMETHOD returned: ACK OK PRETRGRETMETHOD Parameter=6,1"
  verify_vc1_of_270857_log32 = "phFuncSetStatus, key ->PRETRGRETVALUE<-, param ->Parameter=2,-60.0<-"
  verify_vc1_of_270857_log33 = "privateSetStatus, PRETRGRETVALUE returned: ACK OK PRETRGRETVALUE Parameter=2,-60.0"
  verify_vc1_of_270857_log34 = "phFuncSetStatus, key ->PRETRGTESTEND<-, No Param"
  verify_vc1_of_270857_log35 = "privateSetStatus, PRETRGTESTEND returned: ACK OK PRETRGTESTEND"
  driver_string_list.push(verify_vc1_of_270857_log1, verify_vc1_of_270857_log2, verify_vc1_of_270857_log3, 
      verify_vc1_of_270857_log4, verify_vc1_of_270857_log5, verify_vc1_of_270857_log6,
      verify_vc1_of_270857_log7, verify_vc1_of_270857_log8, verify_vc1_of_270857_log9, 
      verify_vc1_of_270857_log10, verify_vc1_of_270857_log11, verify_vc1_of_270857_log12, 
      verify_vc1_of_270857_log13, verify_vc1_of_270857_log14, verify_vc1_of_270857_log15, 
      verify_vc1_of_270857_log16, verify_vc1_of_270857_log17, verify_vc1_of_270857_log18, 
      verify_vc1_of_270857_log19, verify_vc1_of_270857_log20, verify_vc1_of_270857_log21, 
      verify_vc1_of_270857_log22, verify_vc1_of_270857_log23, verify_vc1_of_270857_log24, 
      verify_vc1_of_270857_log25, verify_vc1_of_270857_log26, verify_vc1_of_270857_log27, 
      verify_vc1_of_270857_log28, verify_vc1_of_270857_log29, verify_vc1_of_270857_log30, 
      verify_vc1_of_270857_log30, verify_vc1_of_270857_log31, verify_vc1_of_270857_log32, 
      verify_vc1_of_270857_log33, verify_vc1_of_270857_log34, verify_vc1_of_270857_log35)

  verify_vc2_of_270857_log1 = "phFuncSetStatus, key ->set_invalid_cmd<-, param ->Parameter=111<-"
  verify_vc2_of_270857_log2 = "The key \"set_invalid_cmd\" is not available, or may not be supported yet"
  verify_vc2_of_270857_log3 = "phFuncGetStatus, key ->get_invalid_cmd<-, No Param"
  verify_vc2_of_270857_log4 = "The key \"get_invalid_cmd\" is not available, or may not be supported yet"
  driver_string_list.push(verify_vc2_of_270857_log1, verify_vc2_of_270857_log2, 
      verify_vc2_of_270857_log3, verify_vc2_of_270857_log4)

  file_name = "/tmp/driver.log"
  result = match_strings_in_file(file_name, driver_string_list)
  if result.empty? == true
    puts "[ART_TC1] OK: The log check in the driver is normal!"
  else
    raise "[ART_TC1] Error: The log check in the driver is unnormal. Mismatch strings:\n#{result.join("\n")}"
  end

  file_name = "/tmp/simulator.log"
  result = match_strings_in_file(file_name, simulator_string_list)
  if result.empty? == true
    puts "[ART_TC1] OK: The log check in the simulator is normal!"
  else
    raise "[ART_TC1] Error: The log check in the simulator is unnormal. Mismatch strings:\n#{result.join("\n")}"
  end

  #############################################################
  # ART_TC2:
  # @Covers VC2 of Software Requirement(270828): hard bin is out of range.
  # @TestCase
  # @Refs(requirements={"https://advantest.codebeamer.com/issue/270828"}) 
  ############################################################# 
  # run recipe
  puts ">> Test ART_TC2..."
  puts ">> Run recipe"
  recipe_file = "./recipe/recipe_for_ART_TC2.xml"
  tcct_run = xoc_workspace + "/system/bin/run"
  run_test_command = tcct_run + " " + tc_client_recipe + " " + workspace + " " + recipe_file
  puts recipe_file
  rtn = system(run_test_command)
  unless rtn == true
    raise "ERROR: Run test failed."
  end

  puts ">> Validate log message"
  driver_string_list.clear
  verify_vc2_of_270828_log1 = "SmarTest bin number 99 of current device not found in configuration \"hardbin_to_handler_bin_map\""
  verify_vc2_of_270828_log2 = "handler_bin_ids is defined and handler_bin_ids[16]=\"R\""
  verify_vc2_of_270828_log3 = "BINON Parameter=A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A\\r\\n"
  verify_vc2_of_270828_log4 = "ACK OK BINON Parameter=A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A,A\\r\\n"
  driver_string_list.push(verify_vc2_of_270828_log1, verify_vc2_of_270828_log2,
      verify_vc2_of_270828_log3, verify_vc2_of_270828_log4)

  file_name = "/tmp/driver.log"
  result = match_strings_in_file(file_name, driver_string_list)
  if result.empty? == true
    puts "[ART_TC2] OK: The log check in the driver is normal!"
  else
    raise "[ART_TC2] Error: The log check in the driver is unnormal. Mismatch strings:\n#{result.join("\n")}"
  end

  #############################################################
  # ART_TC3:
  # @Covers VC2 of Software Requirement(270829): site number is out of range(4).
  # @TestCase
  # @Refs(requirements={"https://advantest.codebeamer.com/issue/270829"}) 
  #############################################################
  puts ">> Test ART_TC3..."
  # stop simulator
  puts ">> Stop PH simulator"
  $pnames = ['simulator']
  kill_process $pnames 
  
  # start simulator
  puts ">> Start PH simulator"
  simulator_run = xoc_workspace + "/system/phcontrol/bin/run"
  run_simulator_command = $simulator_path + " " + $simulator_option2
  system(run_simulator_command)

  simulator_string_list.clear
  verify_vc2_of_270829_log1 = "The site number is out of range. It must be set between 1 and 4."
  simulator_string_list.push(verify_vc2_of_270829_log1)

  puts ">> Validate log message"
  sleep(2)
  file_name = "/tmp/simulator.log"
  result = match_strings_in_file(file_name, simulator_string_list)
  if result.empty? == true
    puts "[ART_TC3] OK: The log check in the simulator is normal!"
  else
    raise "[ART_TC3] Error: The log check in the simulator is unnormal. Mismatch strings:\n#{result.join("\n")}"
  end
  puts ">> Test Completed."

rescue => ex
  puts ex.message
  exit 1
ensure
  # stop simulator
  kill_process $pnames
end