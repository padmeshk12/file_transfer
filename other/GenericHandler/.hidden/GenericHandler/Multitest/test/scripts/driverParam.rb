#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2022
#
# CONTENTS: Main function
#
# AUTHORS:  Zuria Zhu
#
#------------------------------------------------------------

xoc_workspace = ENV["WORKSPACE"]
generic_prober = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler"

$driver_path = generic_prober + "/Multitest"
$config_path = generic_prober + "/Multitest/test/config/MT9510-GPIB-2.cfg"

$simulator_path = generic_prober +  "/Multitest/bin/simulator"
$simulator_option = "-d 1 -s 2 -A 60 1>/dev/null 2>&1 &"


