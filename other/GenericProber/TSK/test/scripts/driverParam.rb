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

$driver_path = generic_prober + "/TSK"
$config_path = generic_prober + "/TSK/test/config/GENERIC-GPIB-Prober-2-die.cfg"

$simulator_path = generic_prober +  "/TSK/bin/simulator"
$simulator_option = "-d 1 -i hpib2 -m APM90A -s 2 -c prober -C 1 -S 60 1>/dev/null 2>&1 &"


