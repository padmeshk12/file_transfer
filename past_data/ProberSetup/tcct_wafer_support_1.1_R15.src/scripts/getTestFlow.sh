#!/bin/sh

if [ $# -ne 1 ]
then
	echo "ERROR_NO_PATH"
	exit 1
else
	BASE_DIR=$1
fi

FILE_WAS_CHOOSEN=0
TESTFLOW=""

while [[ $FILE_WAS_CHOOSEN -eq 0 ]]
do 
	variable=`kdialog --getopenfilename ${BASE_DIR} 2>/dev/null`

	if [ "$?" = 0 ]
	then
		TESTFLOW=$variable
		FILE_WAS_CHOOSEN=1
	fi

done

echo "$TESTFLOW"
