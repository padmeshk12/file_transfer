#!/bin/sh
#*************************************************************************************
# NOTICE: PLEASE READ CAREFULLY: ADVANTEST PROVIDES THIS SOFTWARE TO YOU ONLY UPON 
# YOUR ACCEPTANCE OF ADVANTEST'S TERMS OF USE. THE SOFTWARE IS PROVIDED "AS IS" 
# WITHOUT WARRANTY OF ANY KIND AND ADVANTEST'S LIABILITY FOR SUCH SOFTWARE LIMITED 
# TO THE FULLEST EXTENT PERMITTED UNDER THE LAW.
#*************************************************************************************

#############################################################################
#
#  Startup script for the print-wafermap (Xwindow snapshot) functionality
#  when using the Advantest TFI Wafermap Display tool.
#
#  Copyright Advantest 2017
#
#############################################################################

# Initialize variables
SMT_BITS=64

# IMPORTANT:  For use with TFI WaferMap display
WAFERMAP_WINDOW_NAME="Wafer Map - Test Program: TCCT"

# Set the TCCT wafer support directory
TCCT_WAFER_SUPPORT_HOME=/opt/hp93000/tcct_wafer_support

# Set the activity log file name
# Note, to disable activity logging, simply un-comment the /dev/null line and
#       comment-out the actual file name
ACTIVITY_LOG=/tmp/tcctPrintWaferMap.log
# ACTIVITY_LOG=/dev/null

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


function pr_wafer_function ()
{
	my_date=`date +%Y%m%d%H%M%S`

	WIN_NAME=$WAFERMAP_WINDOW_NAME

	OUT_FILE=WaferMap_${WAFER_ID}_${my_date}.xwd

	# get the hostname to build the .device_inuse_soc@<hostname> file name
	HOST=`hostname`

	# get the contents of the .device_inuse_soc@<hostname>
	dev_dir=`cat ~/.device_inuse_soc@${HOST}`

	# If the device path does not exist, default to /tmp 
	# If it does, create the output directory path using the device director's report 
	# sub-directory.
	if [ -z ${dev_dir} ];then 
	   out_dir=/tmp
	else 
	   out_dir="${dev_dir%%/waste}/report"  #write file to device /report sub dir  
	fi
	echo "INFO:  Output directory  is ${out_dir}" >> $ACTIVITY_LOG

	# By default, the xwd command will beep on completion. To make it silent, just
	# add the "-silent" option to the command call.
	/usr/bin/xwd  -name "${WAFERMAP_WINDOW_NAME}" > ${out_dir}/${OUT_FILE}

	if [ $? -eq 0 ]
	then
		echo "INFO:  Wafermap capture done;  ${out_dir}/${OUT_FILE}" >> $ACTIVITY_LOG
	else
		echo "ERROR:  xwd command failed!" >> $ACTIVITY_LOG
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

# Retrieve the current value of "WAFER_ID" as to add it to the wafermap snapshot file name. 
WAFER_ID=`/opt/hp93000/soc/system/bin/run -x ${TCCT_WAFER_SUPPORT_HOME}/bin/${EXEC} -- SMT`

if [ $? -ne 0 ]
then
	echo "ERROR:  ${EXEC} command failed!" >> $ACTIVITY_LOG
	exit 1
else
	echo "INFO:  WAFER_ID = \"$WAFER_ID\"" >> $ACTIVITY_LOG

	# ${TCCT_WAFER_SUPPORT_HOME}/scripts/pr_wafer.sh WaferMap ${WAFER_ID}
	
	
	# IMPORTANT! raise the window to the top before taking the snapshot
	echo "INFO:  Raising WaferMap window \"$WAFERMAP_WINDOW_NAME\"..." >> $ACTIVITY_LOG
	${TCCT_WAFER_SUPPORT_HOME}/bin/show_win -raise "${WAFERMAP_WINDOW_NAME}"

	sleep 1
	# now, capture the wafermap image
	echo "INFO:  Capturing WaferMap window snapshot..." >> $ACTIVITY_LOG
	pr_wafer_function
	
fi
