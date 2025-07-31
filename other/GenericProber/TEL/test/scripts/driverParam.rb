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

xoc_workspace = ENV["WORKSPACE"]
generic_prober = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericProber"

$driver_path = generic_prober + "/TEL"
$config_path = generic_prober + "/TEL/test/config/P12-GPIB-Prober-2-die-256-bin.cfg"

$simulator_path = generic_prober +  "/TEL/bin/simulator"
$simulator_option = "-d 1 -w 2,2,8,8,5,5 -m P12 -S 20 -p [0,0][1,1] -s 1,2,3,4 -S 60 -C 1 1>/dev/null 2>&1 &"
$simulator_option_help = "-h"


