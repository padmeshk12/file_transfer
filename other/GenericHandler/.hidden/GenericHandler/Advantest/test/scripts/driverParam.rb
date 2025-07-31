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
generic_prober = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler"

$driver_path = generic_prober + "/Advantest"
$config_path = generic_prober + "/Advantest/test/config/M48-GPIB-2-site-16-bin.cfg"

$simulator_path = generic_prober +  "/Advantest/bin/simulator"
$simulator_option = "-d 1 -c 16 -s 2 -A 60 1>/dev/null 2>&1 &"


