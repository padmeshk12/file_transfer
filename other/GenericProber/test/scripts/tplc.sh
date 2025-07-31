#!/bin/bash

# TODO in future call tplc in eclipse bin folder...
#$SHIPMENT_ROOT/segments/eclipse/bin/tplc $1

function logHeader {
  echo -e "###############################################"
  echo -e "#       Test Program Language Compiler        #"
  echo -e "#              Version 1.0.1                  #"
  echo -e "###############################################"
}

function logDescription {
  echo -e \
"Usage: tplc WORKSPACE_DIRECTORY [CONFIG_AREA_DIRECTORY] [path/to/.options/file] [debugVM] [incremental|noclean]\
  -incremental [optional] run an incremental build, without cleaning the workspace\
  -noclean     [optional] do not clean the workspace\
"
}


function cleanBuild {
  # See CR-108505 / CR-105710. To avoid too excessive memory allocation for JVM
  # threads, we advise the glibc to use only 2 allocation areas, and not as much
  # areas as the number of application threads.
  export MALLOC_ARENA_MAX=2
  export TPLC_WSP=$1
  export TPLC_CONF_ROOT=$2
  if [[ -z $BUILDER_TIMEOUT ]]
  then
    export BUILDER_TIMEOUT=7200
  else 
    echo "BUILDER_TIMEOUT was already set to $BUILDER_TIMEOUT"
  fi
  export EWC_BUILDER_LOG=$TPLC_CONF_ROOT/log/standalone_builder_errors.log
  export EWC_BUILDER_OUTPUT=$TPLC_CONF_ROOT/log/standalone_builder_output.log
  export TPLC_EXIT_CODE=$TPLC_CONF_ROOT/log/tplc_exitcode.log
  mkdir -p $TPLC_CONF_ROOT/log
  rm -f $TPLC_EXIT_CODE
  touch $EWC_BUILDER_LOG
  rm -f $EWC_BUILDER_OUTPUT
  touch $EWC_BUILDER_OUTPUT

  if [ -f "$3" ]
  then
    DEBUG_OPTIONS="-debug $3"
    echo -e "Using options from $3"
    echo -e "-------------------"
    cat $3
    echo -e "-------------------"
  else
    if [ "X$3" != "X" ]
    then
        echo -e "Error: options file '$3' is not a valid file!"
    fi
    DEBUG_OPTIONS=
  fi

  if [ "X$USERNAME" == "X" ]
  then
    USERNAME=$USER
  fi

  if [ "X$4" != "X" ]
  then
    # -agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=8000
    DEBUG_VM=$4
  fi

  if [ "X$5" != "X" ]
  then
    # either -tplc.incremental or -tplc.clean
    if [ "$5" == "-noclean" ]
    then
      COMPILE_OPTIONS=-tplc.noclean=true
    fi
    if [ "$5" == "-incremental" ]
    then
      COMPILE_OPTIONS=-tplc.incremental=true
    fi
  fi
  
  #PROFILE_OPTIONS=-agentpath:/home/danblank/.p2/pool/plugins/com.yourkit.profiler_2017.2.53.1/bin/linux-x86-64/libyjpagent.so=sampling,onexit=snapshot,logdir=$BUILD_DIR,dir=$BUILD_DIR,sessionname=tplc$5-$(basename $(readlink -f $SHIPMENT_ROOT))

  TPLC_USER_DIR=${TPLC_CONF_ROOT}/tplc/_${USERNAME}

  echo -e "Building workspace: $TPLC_WSP"
  echo -e "Using configuration area: ${TPLC_USER_DIR}/_configuration"
  echo -e "Using log file: $EWC_BUILDER_OUTPUT"
  echo -e "Using errlog file: $EWC_BUILDER_LOG"
  echo -e "Using compile options: $COMPILE_OPTIONS"

  # make sure config area and p2 shit is never reused
  #rm -rf ${TPLC_USER_DIR}
  mkdir -p ${TPLC_USER_DIR}

  # make sure environment is built
  # $(dirname $0)/run -x

  # Make sure no one does crazy things, see CR-110531
  unset LD_LIBRARY_PATH
  unset CLASSPATH
  unset LD_PRELOAD

  # We start run script to get the environment prepared (libraries copied to system/lib)
  # and env. variable XOC_SYSTEM set to the right place
  # However we don't want to add anything to our environment which could affect JVM
  # Therefore we use tplc-wrapper.sh scrpt which will cleanup
  # LD_LIBRARY_PATH and CLASSPATH variables from the JVM environment
  START_COMMAND="$SHIPMENT_ROOT/system/bin//run -x \
$(dirname $0)/tplc-wrapper.sh \
-- \
$SHIPMENT_ROOT/segments/eclipse/eclipseRE/eclipse \
-data $TPLC_WSP \
-vm $XOC_JAVA_HOME/bin/java \
$DEBUG_OPTIONS \
-nosplash \
--launcher.suppressErrors \
-application com.advantest.itee.stpl.standalone.stplstandalone \
-configuration ${TPLC_USER_DIR}/_configuration \
$COMPILE_OPTIONS \
-vmargs \
-Xmx16g \
-Xss5m \
-XX:ErrorFile=$TPLC_WSP/tplc_vm_crash_%p.log \
-Dide.gc=false \
-Djava.awt.headless=true \
-Djava.util.Arrays.useLegacyMergeSort=true \
-Dosgi.bundlefile.limit=100 \
-Dxtext.qn.interning=true \
-XX:+UnlockDiagnosticVMOptions \
-XX:+PrintNMTStatistics \
-XX:NativeMemoryTracking=summary \
$PROFILE_OPTIONS \
-Dcom.verigy.itee.core/offline=true \
-Dcom.verigy.itee.core/headless=true \
-Declipse.filesystem.useNatives=false \
$DEBUG_VM"

  echo -e "Starting: $START_COMMAND"
  $START_COMMAND | tee -a $EWC_BUILDER_OUTPUT &

 # for profiling add this vm parameter:
 # -agentpath:/.automount/socadmin/root/data/rhel5/yourkit/current/bin/linux-x86-64/libyjpagent.so=sampling

  # the first (oldest) background process id (the tplc_wrapper script)
  export TPLC_WRAPPER_PID=`pgrep -o -P $$`
  if [ "X$TPLC_WRAPPER_PID" == "X" ]
  then
    echo -e "\n!!! Waiting 5 seconds for Eclipse to start..."
    sleep 5
    TPLC_WRAPPER_PID=`pgrep -o -P $$`
  fi

  if [ "X$TPLC_WRAPPER_PID" == "X" ]
  then
    echo -e "Waiting another 60 seconds for Eclipse to start..."
    sleep 60
    TPLC_WRAPPER_PID=`pgrep -o -P $$`
  fi

  if [ "X$TPLC_WRAPPER_PID" == "X" ]
  then
    echo -e "ERROR: failed to start Eclipse..."
    exit 1
  fi

  echo -e "\TPLC Wrapper process pid: $TPLC_WRAPPER_PID [$(date +"%d-%m-%Y %H:%M:%S")]"

  waitForProcess $TPLC_WRAPPER_PID
  wait $TPLC_WRAPPER_PID

  #checkTplcLogForMemoryIssues

  builderExitCode=`cat $TPLC_EXIT_CODE`

  if [ "X$builderExitCode" == "X" ]
  then
     echo -e "\ntplc failed (no exit code returned) [$(date +"%d-%m-%Y %H:%M:%S")]\n"
     exit 1
  fi

  if [ "X$builderExitCode" == "X0" ]
  then
     if grep -e "Standalone compiler finished successfully" $EWC_BUILDER_OUTPUT 2>/dev/null
     then
        echo -e "\ntplc succeeded [$(date +"%d-%m-%Y %H:%M:%S")]\n"
        exit 0
     else
        echo -e "\ntplc returned with exit code $builderExitCode but still didn't succeed [$(date +"%d-%m-%Y %H:%M:%S")]\n"
        exit 1
     fi
  else
     echo -e "\ntplc failed with exit code $builderExitCode [$(date +"%d-%m-%Y %H:%M:%S")]\n"
     exit $builderExitCode
  fi
}

function waitForProcess
{
    checkLogAndWait "isProcessAlive" $1
}


# parameter 1 : process_id to check for
# return 0 if the process is still alive, 1 otherwise
function isProcessAlive
{
    PROC_ID=$1
    still_alive=`ps --no-headers -p $PROC_ID`
    if [ "X$still_alive" != "X" ]
    then
        return 0
    else
        return 1
    fi
}

function checkLogAndWait
{
    startTime="$(date +"%d-%m-%Y %H:%M:%S")"
    notRunCount=0
    while [ 0 = 0 ];
    do
        # evaluate function, on "1" stop waiting
        $@
        if [ "$?" -eq 1 ]
        then
            break
        fi
        # trap signal CTRL+C and kill
        trap "killBuilder;exit" 2 15

        sleep 1
        notRunCount=`expr $notRunCount + 1`
            if [ "$notRunCount" -ge "$BUILDER_TIMEOUT" ]
            then
                echo -e "ERROR: Builder did not complete within $BUILDER_TIMEOUT seconds"
                endTime="$(date +"%d-%m-%Y %H:%M:%S")"
                echo -e "tplc was started at $startTime and is going to be killed at $endTime"
                printMemoryAndProcesses
                killBuilder
                exit 1
            fi
    done
}

function killBuilder  {
    pgrep -o -P $TPLC_WRAPPER_PID
    EWC_BUILDER_ECLIPSE_PID=`pgrep -o -P $TPLC_WRAPPER_PID`
    pgrep -P $EWC_BUILDER_ECLIPSE_PID
    javasub_pids=`pgrep -P $EWC_BUILDER_ECLIPSE_PID`
    if [ "X$javasub_pids" != "X" ]
    then
        for proc_id in $javasub_pids
        do
            echo -e "Killing the child Java process ($proc_id) of the main Java Eclipse process."
            generateJVMInfo $proc_id
            (kill $proc_id 2>&1) > /dev/null
        done
    fi
    echo -e "Killing the main Java Eclipse process ($EWC_BUILDER_ECLIPSE_PID)"
    (kill -9 $EWC_BUILDER_ECLIPSE_PID 2>&1) > /dev/null

    echo -e "Builder was killed! Full log from $EWC_BUILDER_LOG"
    echo -e "Full log from $EWC_BUILDER_LOG : "
    echo -e "##############################################"
    cat $EWC_BUILDER_LOG
    echo -e "##############################################"
    echo -e "Log from $TPLC_WSP/tplc_vm_crash_*.log (if any) : "
    echo -e "##############################################"
    cat $TPLC_WSP/tplc_vm_crash_*.log
    echo -e "##############################################"
}


#
# Generate JVM info for one pid.
#  jinfo   Configuration Info for Java - Prints configuration information for for a given process or core file or a remote debug server.
#  jmap    Memory Map for Java - Prints shared object memory maps or heap memory details of a given process or core file or a remote debug server.
#  jstack  Stack Trace for Java - Prints a stack trace of threads for a given process or core file or remote debug server.
#
function generateJVMInfo
{
    isJava=`ps --no-headers -p $1 | grep java`
    if [ "X$isJava" = "X" ]
    then
        echo -e "\nNot a java process: $1 "
        ps -f -p $1
        return 1
    fi

    # dump the pid jinfo information into the crash log file
    echo -e "\nBuilder pid $1 jinfo information " >> $EWC_BUILDER_LOG
    (jinfo $1 2>&1) >> $EWC_BUILDER_LOG

    # dump the pid jmap information into the crash log file
    echo -e "\nBuilder pid $1 jmap information" >> $EWC_BUILDER_LOG
    (jmap $1 2>&1) >> $EWC_BUILDER_LOG

    #dump the pid jstack information into the crash log file
    echo -e "\nBuilder pid" $1 "jstack information" >> $EWC_BUILDER_LOG
    (jstack $1 2>&1) >> $EWC_BUILDER_LOG
    echo -e "\n+End of the first dump\n" >> $EWC_BUILDER_LOG

    # In case the VM hangs completely, use -F to force dump
    echo -e "\n+Forced dump" >> $EWC_BUILDER_LOG
    (jstack -F $1 2>&1) >> $EWC_BUILDER_LOG
    echo -e "\n+END OF THE SECOND DUMP" >> $EWC_BUILDER_LOG
}

# check native memory statistics for tplc JVM and report to autoverify
function checkTplcLogForMemoryIssues
{
  # check "HS Compiler" memory and report to autoverify if it exceeds 5 digits (>= 100 MB)
  compilerMem=`grep -E 'Compiler \(reserved=[0-9]{6,}' $EWC_BUILDER_OUTPUT`
  if [ "X$compilerMem" != "X" ]
  then
     echo -e "+TEST : ERROR: hs compiler leaked memory:  $compilerMem"
     overallMem=`grep -E 'Total: reserved=[0-9]+' $EWC_BUILDER_OUTPUT`
     echo -e "TEST_INFO: Error: overall memory consumption:  $overallMem"
     heapMem=`grep -E 'Java Heap \(reserved=[0-9]+' $EWC_BUILDER_OUTPUT`
     echo -e "TEST_INFO: Error: heap memory consumption1:  $heapMem"
  fi

  # check total memory and report to autoverify if it exceeds 7 digits (>= 10 GB)
  overallMem=`grep -E 'Total: reserved=[0-9]{8,}' $EWC_BUILDER_OUTPUT`
  if [ "X$overallMem" != "X" ]
  then
     echo -e "+TEST : ERROR: possible memory leak:  $overallMem"
     heapMem=`grep -E 'Java Heap \(reserved=[0-9]+' $EWC_BUILDER_OUTPUT`
     echo -e "TEST_INFO: Error: heap memory consumption2:  $heapMem"
  fi
}

function printMemoryAndProcesses
{
  echo -e "free -k -t -g :"
  free -k -t -g

  echo -e "Process list sorted by RSS memory use:"
  echo -e "ps -o pid,s,%mem,%cpu,rss,sz,vsz,time,user,args -e -w  --sort -rss | head -n 50"
  ps -o pid,s,%mem,%cpu,rss,sz,vsz,time,user,args -e -w  --sort -rss | head -n 50

  echo -e "Process list sorted by CPU load:"
  echo -e "ps -o pid,s,%mem,%cpu,rss,sz,vsz,time,user,args -e -w  --sort -pcpu | head -n 50"
  ps -o pid,s,%mem,%cpu,rss,sz,vsz,time,user,args -e -w  --sort -pcpu | head -n 50

  echo
}

function main {

  logHeader

    if [ -z $1 ]
    then
        logDescription
        exit 1
    fi

    echo -e "tplc started at [$(date +"%d-%m-%Y %H:%M:%S")]"
    if [ -z $2 ]
    then
        cleanBuild "$1" "${HOME}/.eclipse"
    else
        cleanBuild "$1" "$2" "$3" "$4" "$5"
    fi
}

main "$1" "$2" "$3" "$4" "$5"

