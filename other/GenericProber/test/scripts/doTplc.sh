#!/bin/bash

WORKSPACE=$1
LOG_FILE=$2
compileOption=$3 #either -noclean or -incremental

function printUsage(){
  echo "Usage:"
  echo "$0 /path/to/workspace /path/to/logfile.log [-noClean|-incremental]"
}

if [[ ! -d $WORKSPACE ]]
then
  echo "Error: Workspace $WORKSPACE does not exist"
  printUsage
  exit 1
fi

scriptDir="$(cd "$(dirname "$0")" && pwd)"

export XOC_JAVA_HOME=/usr/lib/jvm/java-17

if [[ ! $SHIPMENT_ROOT ]]
then
  export SHIPMENT_ROOT=/opt/hp93000/soc
fi


echo "starting tplc compile of $WORKSPACE, sending log to $LOG_FILE"
if [ $compileOption ]
then
  echo "compile option: $compileOption"
fi

tplcConfigDir=/tmp/tplcConfig_$(basename $(readlink -f $SHIPMENT_ROOT))_$USER
mkdir -p $tplcConfigDir

SECONDS=0
$scriptDir/tplc.sh $WORKSPACE $tplcConfigDir /dev/null -D $compileOption &>> $LOG_FILE
if [ ! $? -eq 0 ]
then
  tplcFailed=1
  echo "tplc compile failed!"
  grep -A 1 -e "ERROR:" $LOG_FILE
fi


echo "compiling using tplc took $SECONDS seconds"
numberOfErrors=`grep -e "ERROR:" $LOG_FILE | wc -l`
numberOfWarnings=`grep -e "WARNING:" $LOG_FILE | wc -l`
echo "This tplc compile produces $numberOfErrors errors and $numberOfWarnings warnings"

eclipseLogFile=$WORKSPACE/.metadata/.log
if [[ -f $eclipseLogFile ]]
then
  grep -e "\!ENTRY" $eclipseLogFile | while read -r entryLine
  do
    entry=( $entryLine )
    
    if [[ ${#entry[@]} -lt 5 ]]
    then
      echo "Error entry without severity appeared: $entryLine"
      tplcFailed=1
    elif [[ ${entry[2]} -ge 2 ]]
    then
      echo "Error entry with severity >warning appeared: $entryLine"
      tplcFailed=1
    fi
  done
  
  echo -e "\nEclipse Error Log after tplc-run:" >> $LOG_FILE
  cat $eclipseLogFile >> $LOG_FILE
fi

if [ $tplcFailed ]
then
  exit 1
fi

echo "tplc was compiling successfully."
exit 0
