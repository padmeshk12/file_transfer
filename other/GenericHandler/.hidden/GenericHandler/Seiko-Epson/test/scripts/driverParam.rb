#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2023
#
# CONTENTS: Main function
#
# AUTHORS:  Zoyi Yu
#
#------------------------------------------------------------

xoc_workspace = ENV["WORKSPACE"]
generic_equipment = xoc_workspace + "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler"

$driver_path = generic_equipment + "/Seiko-Epson"
$config_path = generic_equipment + "/Seiko-Epson/test/config/CHROMA3180-GPIB-2.cfg"

$simulator_path = generic_equipment +  "/Seiko-Epson/bin/simulator"
$simulator_option = "-d 1 -s 2 -m CHROMA -A 60 1>/dev/null 2>&1 &"


