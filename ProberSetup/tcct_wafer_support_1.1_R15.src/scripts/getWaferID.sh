#!/bin/sh
NUM=$#
echo "num of params = $NUM"

ID_WAS_INPUT=0
WAFER_ID=""

while [[ $ID_WAS_INPUT -eq 0 ]]
do 
	variable=`kdialog --inputbox "Please input the Wafer-ID" 2>/dev/null`

	if [ "$?" = 0 ]
	then
		WAFER_ID=$variable
		ID_WAS_INPUT=1
	fi

done

echo "$WAFER_ID"
