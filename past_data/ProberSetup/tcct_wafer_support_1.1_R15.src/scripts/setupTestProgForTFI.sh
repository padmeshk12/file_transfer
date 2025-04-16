#!/bin/sh
#*************************************************************************************
# NOTICE: PLEASE READ CAREFULLY: ADVANTEST PROVIDES THIS SOFTWARE TO YOU ONLY UPON 
# YOUR ACCEPTANCE OF ADVANTEST'S TERMS OF USE. THE SOFTWARE IS PROVIDED "AS IS" 
# WITHOUT WARRANTY OF ANY KIND AND ADVANTEST'S LIABILITY FOR SUCH SOFTWARE LIMITED 
# TO THE FULLEST EXTENT PERMITTED UNDER THE LAW.
#*************************************************************************************

########################################################
#
#  Run script used to setup a special "TCCT" test program file
#  for use with Advantest TFI-WaferMap display that needs a TP
#  file in order to get the name of the TestFlow file and then
#  extract the bining information from it.
#
#  Copyright Advantest 2017
#
########################################################


if [ $# -ne 2 ]
then
	echo "$0  *** ERROR! missing Device Path and/or TestFlow name ***"
	exit 1
fi

DEV_PATH=$1
TF_NAME=$2

# Temporary Test program file that will be created with the testflow name inside.
# This will enable the TFI-WaferMap display to properly get the binning information
# from the testflow file and minimize the errors in /var/opt/tfi/log/wafermap.log
#
TP_PATH=/projects/ibm/padmesh/Prober_Setup/output

# The first echo line will overwrite any existing 'TCCT' test program file.
echo "hp93000,testprog,0.1" > $TP_PATH
echo "Testflow:    ${TF_NAME}" >> $TP_PATH

exit 0
