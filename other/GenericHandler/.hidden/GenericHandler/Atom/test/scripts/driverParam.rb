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

$driver_path = generic_prober + "/Atom"
$config_path = generic_prober + "/Atom/test/config/DBDESIGN-GPIB-1-SITE.cfg"

$simulator_path = generic_prober +  "/Atom/bin/simulator"
$simulator_option = "-d 1 -s 1 -A 60 1>/dev/null 2>&1 &"


