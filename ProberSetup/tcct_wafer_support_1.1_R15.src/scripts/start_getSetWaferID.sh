#!/bin/sh
#*************************************************************************************
# NOTICE: PLEASE READ CAREFULLY: ADVANTEST PROVIDES THIS SOFTWARE TO YOU ONLY UPON 
# YOUR ACCEPTANCE OF ADVANTEST'S TERMS OF USE. THE SOFTWARE IS PROVIDED "AS IS" 
# WITHOUT WARRANTY OF ANY KIND AND ADVANTEST'S LIABILITY FOR SUCH SOFTWARE LIMITED 
# TO THE FULLEST EXTENT PERMITTED UNDER THE LAW.
#*************************************************************************************

########################################################
#
#  Startup script for the load_wafer_description application
#
#  Copyright Advantest 2017
#
########################################################

SMT_BITS=64
TCCT_WAFER_SUPPORT_HOME=/opt/hp93000/tcct_wafer_support/bin

#----------------------------------------------------------------------
# Function:  isSmarTest64bit
#
# Description:  determines if the currently active SmarTest version
#               is 64bit or not.  It will set the variable 'SMT_BITS'
#               with either "64" or "32".
#-----------------------------------------------------------------------
function isSmarTest64Bit ()
{
	TYPE=`file /opt/hp93000/soc |awk '{print $2}'`

	if [ $TYPE == "symbolic" ]
	then
		LINK_TO_64bit=`readlink /opt/hp93000/soc |grep soc64 |wc -l`
		
		if [ $LINK_TO_64bit == "1" ] 
		then
			SMT_BITS=64
		else
		
			SMT_BITS=32
		fi
	
	else
	
		#echo "  ERROR!  /opt/hp93000/soc is NOT a symbolic link! exit" > stderr
		exit 1
	fi
}


#===============================
# main script execution section
#===============================


# check if the currently selected SmarTest version is a 64bit release
isSmarTest64Bit

# Define the correct executable name based on the SMT_BITS variable.
if [ $SMT_BITS == "64" ]
then
	EXEC=getSetWaferID-64bit
else
	EXEC=getSetWaferID
fi 

# echo "Starting getSetWaferID with 'GUI' option..." 
EXEC_OUTPUT=`/opt/hp93000/soc/system/bin/run -x ${TCCT_WAFER_SUPPORT_HOME}/${EXEC} -- GUI`

if [ $? -ne 0 ]
then
	echo "$EXEC_OUTPUT"
	exit 1
fi
