#!/bin/bash
# The only purpose of this script is to allow tplc to run Eclipse
# with the pristine LD_LIBRARY_PATH environment variable
# which is set by "run" script unconditionally. See  CR-110531

unset LD_LIBRARY_PATH
unset CLASSPATH
unset LD_PRELOAD

# run the command
$@
exitCode=$?

echo "$exitCode" > $TPLC_EXIT_CODE