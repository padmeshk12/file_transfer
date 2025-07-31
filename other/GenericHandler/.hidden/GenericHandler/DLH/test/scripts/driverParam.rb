#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2023
#
# CONTENTS: Main function
#
# AUTHORS:  Runqiu Cao
#
#------------------------------------------------------------

xoc_workspace = ENV["WORKSPACE"]
generic_prober = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler"

$driver_path = generic_prober + "/DLH"
$config_path = generic_prober + "/DLH/test/config/"

$simulator_path = generic_prober +  "/DLH/bin/simulator"
$simulator_option1 = "-d 1 -p 6688 -s 2 -i 127.0.0.1:6689 &> /tmp/simulator.log &"
$simulator_option2 = "-d 1 -p 6688 -s 5 -i 127.0.0.1:6689 &> /tmp/simulator.log &"


