#!/bin/sh
#
# ===============================================================================================
#
# Script that will extract the requested information from the specified Wafer-Desciption-File.
#
# Call syntax:	 getWaferInfo  <WDFname> <waferParamName>
#
#                where <waferParamName> : [ MIN_X, MIN_Y, MAX_X, MAX_Y, POS_X, POS_Y, WF_FLAT ]
#
# NOTE 1:  In the case where POS_X or POS_Y are requested, the script will return a value based
#          on the WDF parameter "QUAD#" using a specific mapping.
#
# NOTE 2:  In the case wher WF_FLAT is requested, the srcipt will return a value based on the
#          WDF parameter "PHASE: using a specific value range mapping.
#   
# returns
#  value of <waferParamName>
#
# -------------------------------------------------------------
# Author:	Jean Lemaire, P.Eng. -- Advantest Corp.
#
# 02-13-2017 -- First release
#
# ===============================================================================================


# ---------------------------------------------------------------------
# Function that will be called when <waferParamName> = POS_X or POS_Y
# that will return a value based on the following mapping of the 
# WDF "QUAD#" field value:
#
#  QUAD# = 1 --> POS_X = R; POS_Y = U
#  QUAD# = 2 --> POS_X = L; POS_Y = U
#  QUAD# = 3 --> POS_X = L; POS_Y = D
#  QUAD# = 4 --> POS_X = R; POS_Y = D
#
# output: Value of either POS_X or POS_Y
#
# ---------------------------------------------------------------------
function processQuadrant ()
{
	# extract the WDF line that contains the Wafer Parameter name
	INFO=`grep "QUAD#" ${WDF_FILE}`
	
	# Tokenize the answer string using ":" and <space> as input field separators
	IFS=": "; read -a fields <<< "$INFO"

	# Extract the parameter value from the second element of the 'fields' array
	VAR_VALUE=${fields[1]}

	if [ $VAR_VALUE == "1" ]
	then
		POS_X=R
		POS_Y=U
	elif [ $VAR_VALUE == "2" ]
	then
		POS_X=L
		POS_Y=U
	elif [ $VAR_VALUE == "3" ]
	then
		POS_X=L
		POS_Y=D
	else  
		#  $VAR_VALUE == "4" 
		POS_X=R
		POS_Y=D
	fi
	
	if [ ${WAF_PARAM} == "POS_X" ]
	then
		echo "$POS_X"
	else
		echo "$POS_Y"
	fi
	
}


# ---------------------------------------------------------------------
# Function that will be called when <waferParamName> = WF_FLAT
# that will return a value based on the following mapping of the 
# WDF "PHASE" field value:
#
#  45  <  PHASE <  136 --> WF_FLAT = L
#  135 <  PHASE <  226 --> WF_FLAT = U
#  225 <  PHASE <  315 --> WF_FLAT = R
#  316 <= PHASE <= 360
#  OR
#  0   <= PHASE <= 45  --> WF_FLAT = D
#
# output: Value of either POS_X or POS_Y
#
# ---------------------------------------------------------------------
function processPhase()
{
	# extract the WDF line that contains the Wafer Parameter name
	INFO=`grep "PHASE" ${WDF_FILE}`
	
	# Tokenize the answer string using ":" and <space> as input field separators
	IFS=": "; read -a fields <<< "$INFO"

	# Extract the parameter value from the second element of the 'fields' array
	VAR_VALUE=${fields[1]}

	# if  45> Phase < 136
	#   WF_FLAT = L
	# if  135> Phase < 226
	#   WF_FLAT = U
	# if  225> Phase < 315
	#   WF_FLAT = R
	# ELSE Must be down ( 316 ~ 45 )
	#   WF_FLAT = D
	if [[ $VAR_VALUE -gt 45 && $VAR_VALUE -lt 136 ]]
	then
		WF_FLAT=L
	elif [[ $VAR_VALUE -gt 135 && $VAR_VALUE -lt 226 ]]
	then
		WF_FLAT=U
	elif [[ $VAR_VALUE -gt 225 && $VAR_VALUE -lt 315 ]]
	then
		WF_FLAT=R
	else
		WF_FLAT=D
	fi	
	
	echo "${WF_FLAT}"
}

# --------------------------------------------------------------------------------------
# Function that will be called when <waferParamName> =  [ MIN_X, MIN_Y, MAX_X, MAX_Y ]
# and will return the value of the specified field, extracted from the WDF file.
# --------------------------------------------------------------------------------------
function getVarValue ()
{

	# extract the WDF line that contains the Wafer Parameter name
	INFO=`grep ${WAF_PARAM} ${WDF_FILE}`
	
	# Tokenize the answer string using ":" and <space> as input field separators
	IFS=": "; read -a fields <<< "$INFO"

	# Extract the parameter value from the second element of the 'fields' array
	VAR_VALUE=${fields[1]}
	
	# output the var value
	echo $VAR_VALUE
}

# --------------------------------------------------------------------
# function that will validate that the WDF exists and is readable 
# --------------------------------------------------------------------
function validateWDF ()
{
	if [ -r $WDF_FILE ]
	then
		return 0
	else
		echo "WDF_READ_ERROR"
		exit 1
	fi
}

#-------------------------------------
# main execution section
#-------------------------------------

# Must have 2 parameters:  <WDF_file_name> <WaferParameterName>

if [ $# -ne 2 ]
then
	exit 1
fi

WDF_FILE=$1
WAF_PARAM=$2

validateWDF

if [[ ${WAF_PARAM} == "POS_X" || ${WAF_PARAM} == "POS_Y" ]]
then 
	VAR_VALUE=`processQuadrant`
	
elif [ ${WAF_PARAM} == "WF_FLAT" ]
then
	VAR_VALUE=`processPhase`
	
else
	VAR_VALUE=`getVarValue`
fi

# Output the value of the requested parameter
echo "$VAR_VALUE"
