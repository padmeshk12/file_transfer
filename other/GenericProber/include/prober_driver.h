/**
 * Copyright by Advantest 2015
 * 
 * Date:      10 July 2008 
 *
 * History:   Initial Verision, 10 July 2008 
 *          
 * Description: This file should be included when calling the prober 
 *              dirver functions in BLC or other c/c++ program. 
 *              The corresponding shared library is   
 *              /opt/hp93000/soc/PH_libs/GenericProber/libcifset.so
 **/

#ifndef _PROBER_DRIVER_HEADER_FILE_
#define _PROBER_DRIVER_HEADER_FILE_



/*----------------macro definition----------------*/

/**
 * <!! DO NOT change this value !!>
 *
 * This macro defines the maximum length (including the '\0' character) of 
 * the parameter "parm_list_input" 
 * 
 * If the length of the parameter "param_list_input" parameter exceeds this
 * value, it will be truncated by the prober driver function, which leads to
 * incorrect driver function behavior.
 *
 * This means, if prober driver function is called in the BLC or C/C++ program,
 * the length of "param_list_input" can't exceeds PH_DRIVER_MAX_COM_LEN.  
 *
 * <!! DO NOT change this value !!>
 **/
#define PH_DRIVER_MAX_PARM_LEN 257

/**
 * <!! DO NOT change this value !!>
 *
 * This macro defines the minimum length (including the '\0' character) of 
 * the parameter "comment_out"
 *
 * If the length of the parameter "comment_out" is less then this value and
 * there are something to be returned via this parameter, there would be
 * memory fault error.
 *
 * This means, if prober driver function is called in the BLC or C/C++ program,
 * the length of the parameter "comment_out" must be larger than 
 * PH_DRIVER_MIN_COMMENT_LEN.
 *
 * <!! DO NOT change this value !!>
 **/
#define PH_DRIVER_MIN_COMMENT_LEN 45001


/*----------------driver function declaration----------------*/

/**
 * Important: the functions declared below follow the CI function format 
 * defintion. Currently the prober driver function doesn't utilize the 
 * "parmcount" parameter, so an abitrary "parmcount" value is ok for the 
 * driver function execution.
 *
 * If you want to assign the correct value to "parmcount", just use white
 * space as token to caculate the number of non-space substring of the parameter
 * "parm_list_input".
 **/

/**
 *
 * Start a prober driver session 
 *
 * Description:
 *
 * ph_driver_start and ph_driver_done are supposed to be called only
 * ONCE per testprogram execution. ph_driver_start will establish the
 * connection to the prober. Therefore the prober configuration is
 * read, the interface session is opened, the probers identity is
 * checked and the preconfigured data is dowloaded to the
 * prober. ph_driver_done is the counterpart to close the connection.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_driver_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Stop a prober driver session 
 *
 * Description:
 *
 * ph_driver_start and ph_driver_done are supposed to be called only
 * ONCE per testprogram execution. ph_driver_start will establish the
 * connection to the prober. Therefore the prober configuration is
 * read, the interface session is opened, the probers identity is
 * checked and the preconfigured data is dowloaded to the
 * prober. ph_driver_done is the counterpart to close the connection.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_driver_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Start a new lot of devices
 *
 * Description:
 *
 * ph_lot_start and ph_lot_end are optional calls to the
 * driver. These functions usually do not communicate to the
 * prober. 
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_lot_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Complete a lot of devices 
 *
 * Description:
 *
 * ph_lot_start and ph_lot_end are optional calls to the
 * driver. These functions usually do not communicate to the
 * prober. 
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_lot_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Start a new cassette of untested wafers
 *
 * Description:
 *
 * ph_cassette_start and ph_cassette_end are optional calls to
 * the driver, depending on the configuration and setup of the prober
 * and the customer needs. 
 *
 * In case the prober and/or the prober driver plugin does not support
 * handling of wafer cassettes, a call to this function is gracefully
 * ignored and a message is printed to th elog output.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_cassette_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * End a new cassette of untested wafers
 *
 * Description:
 *
 * ph_cassette_start and ph_cassette_end are optional calls to
 * the driver, depending on the configuration and setup of the prober
 * and the customer needs. 
 *
 * In case the prober and/or the prober driver plugin does not support
 * handling of wafer cassettes, a call to this function is gracefully
 * ignored and a message is printed to th elog output.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_cassette_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Load a new wafer
 *
 * Description:
 *
 * ph_wafer_start and ph_wafer_end are mandatory calls to the
 * driver. They must appear in the program used for wafer
 * probing. Usually, this function will load a new untested wafer from
 * the input cassette (if existent). If no more wafers are available,
 * the function will return and set the LEVEL END flag. 
 * In case of optional wafer retest or test on a reference wafer, the
 * function will load the correct wafer instead of the next untested
 * wafer, based on the flags that were set during the last call to
 * ph_pause_done.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_wafer_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Unload the current wafer
 *
 * Description:
 *
 * ph_wafer_start and ph_wafer_end are mandatory calls to the
 * driver. They must appear in the program used for wafer
 * probing. Usually, this function will unload the current wafer from
 * the chuck (if not already done automatically by the prober after
 * testing the last die) to the output cassette (if existent). If no
 * more wafers are available and the prober notifies this fact, the
 * LEVEL END flag will be set. In case of optional wafer retest or 
 * test on a reference wafer, the function will unload the current 
 * wafer to some special storage area (or leave it on the chuck) 
 * instead of unloading to the output cassette.  This is based on 
 * the flags that were set during the last call to ph_pause_done.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_wafer_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Wait for die probing 
 *
 * Description:
 *
 * ph_die_start and ph_die_done are mandatory for prober based
 * testing. ph_die_start tells the probers to step to the next
 * die(s) i.e. it waits until the prober or the prober driver plugin
 * notified this action. ph_die_done will get the binning and
 * pass/fail information from the tester, apply a binning map on this
 * data and tell the prober how to bin the tested dies.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_die_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Bin dies
 *
 * Description:
 *
 * ph_die_start and ph_die_end are mandatory for prober based
 * testing. ph_die_start tells the probers to step to the next
 * die(s) i.e. it waits until the prober or the prober driver plugin
 * notified this action. ph_die_done will get the binning and
 * pass/fail information from the tester, apply a binning map on this
 * data and tell the prober how to bin the tested dies.
 * 
 * Note:
 * In case of subdie probing, this function is of no effect.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_die_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Wait for die probing 
 *
 * Description:
 *
 * ph_subdie_start and ph_subdie_done are optional for prober
 * based testing. It may be used, even if no subdie probing must be performed
 * (i.e. there are no subdies on the wafer). The subdie probing only
 * gets affective, if the configuration value 'perform_subdie_probing'
 * is set to "yes". ph_subdie_start tells the probers to step to
 * the next subdie(s) on the current die, i.e. it waits until the
 * prober or the prober driver plugin notified this
 * action. ph_subdie_done will get the binning and pass/fail
 * information from the tester, apply a binning map on this data and
 * tell the prober how to bin the tested subdies.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_subdie_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Bin subdies
 *
 * Description:
 *
 * ph_subdie_start and ph_subdie_done are optional for prober
 * based testing. It may be used, even if no subdie probing must be performed
 * (i.e. there are no subdies on the wafer). The subdie probing only
 * gets affective, if the configuration value 'perform_subdie_probing'
 * is set to "yes". ph_subdie_start tells the probers to step to
 * the next subdie(s) on the current die, i.e. it waits until the
 * prober or the prober driver plugin notified this
 * action. ph_subdie_done will get the binning and pass/fail
 * information from the tester, apply a binning map on this data and
 * tell the prober how to bin the tested subdies.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_subdie_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Enter pause mode 
 *
 * Description:
 *
 * The driver must be notified whenever pause actions in the
 * BLC or C/C++ program are entered or left. This is to ensure, that the
 * driver can correctly interprete CI system flags (e.g. for device
 * check or retest, wafer retest, etc...). Entering and leaving the
 * pause mode (i.e. calling ph_pause_start and ph_pause_done) will
 * change the internal state of the prober driver. Usually no special
 * commands are sent to the prober while entering or leaving pause
 * mode. CI flags are read when leaving the pause mode. The flags may
 * affect behavior of subsequent calls to ph_die_start and other
 * functions.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_pause_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Leave puase mode 
 *
 * Description:
 *
 * The driver must be notified whenever pause actions in the
 * BLC or C/C++ program are entered or left. This is to ensure, that the
 * driver can correctly interprete CI system flags (e.g. for device
 * check or retest, wafer retest, etc...). Entering and leaving the
 * pause mode (i.e. calling ph_pause_start and ph_pause_done) will
 * change the internal state of the prober driver. Usually no special
 * commands are sent to the prober while entering or leaving pause
 * mode. CI flags are read when leaving the pause mode. The flags may
 * affect behavior of subsequent calls to ph_die_start and other
 * functions.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_pause_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Go into hand test mode 
 *
 * Description:
 *
 * Hand test mode may be entered and left explicitely through calls to
 * the functions ph_handtest_start and ph_handtest_stop. Hand
 * test mode may be initiated from the pause dialog. During hand test
 * mode, the operator physically disconnects the equipment from the
 * testhead and plugs in devices by hand. No communication takes place
 * between driver and equipment while operating in hand test mode. The
 * operator will simulate the prober actions manually (on driver's
 * request), e.g. telling the tester to start the test and binning the
 * device into the appropriate bin.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_handtest_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Leave hand test mode 
 *
 * Description:
 *
 * Hand test mode may be entered and left explicitely through calls to
 * the functions ph_handtest_start and ph_handtest_stop. Hand
 * test mode may be initiated from the pause dialog. During hand test
 * mode, the operator physically disconnects the equipment from the
 * testhead and plugs in devices by hand. No communication takes place
 * between driver and equipment while operating in hand test mode. The
 * operator will simulate the prober actions manually (on driver's
 * request), e.g. telling the tester to start the test and binning the
 * device into the appropriate bin.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_handtest_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Go into single step mode 
 *
 * Description:
 *
 * Single step mode may be entered and left explicitely through calls
 * to the functions ph_stepmode_start and
 * ph_stepmode_stop. Single Step mode may be initiated from the
 * pause dialog. During single step mode the driver breaks before each
 * execution of a prober/prober command initiated by the BLC or C/C++ 
 * program. During each break the operator is asked to confirm the next
 * step.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_stepmode_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Leave single step mode 
 *
 * Description:
 *
 * Single step mode may be entered and left explicitely through calls
 * to the functions ph_stepmode_start and
 * ph_stepmode_stop. Single Step mode may be initiated from the
 * pause dialog. During single step mode the driver breaks before each
 * execution of a prober/handler command initiated by the BLC or C/C++ 
 * program. During each break the operator is asked to confirm the next
 * step.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_stepmode_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Issue a reprobe command to the prober 
 *
 * Description:
 *
 * ph_reprobe causes the driver to issue a reprobe command to the
 * prober. This operation becomes void, if the prober does not
 * support this functionality on its interface. It can still occur in
 * the BLC or C/C++ program but the call will then have no effect.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_reprobe(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Perform a probe clean action on the prober
 *
 * Description:
 *
 * If the prober and the prober driver plugin supports this feature,
 * the probe needles are cleaned by this call. In case the feature is
 * not available, a message is printed to the log output. This
 * function may be called in any place of the BLC or C/C++ program, but
 * not before ph_driver_start.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_clean_probe(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Reconfigure the driver interactively 
 *
 * Description:
 *
 * ph_interactive_reconfiguration provides a mechanism to change
 * every configuration definition, which normally is set through the
 * configuration file. Examples are: changing site and bin control,
 * control of handtest and single step mode, etc. This is a very
 * powerfull command, which gives complete configuration controll into
 * the hands of the operator. A first implementation will just pop up
 * the default editor (env. variable EDITOR) with a copy of the
 * current valid configuration. This may be changed and activated on
 * demand. 
 *
 * Future releases may incorporate a GUI and user mode dependent
 * permissions to change only parts of the configuration.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_interactive_configuration(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Set or change a configuration value 
 *
 * Description:
 *
 * This function can be used to change certain configuration values at
 * runtime from calls issued by the BLC or C/C++ program. It is not
 * recommended to make a lot of configuration setting calls for two
 * reasons: 1. The BLC or C/C++ program becomes handler/prober specific,
 * which should be avoided. 2. Missuse of this function may cause
 * malfunction of the driver. However, the function exists to provide
 * the highest level of flexibility to developper.
 *
 * Input parameter format: 
 * The input parameter can be any
 * configuration definition as used in the driver configuration
 * file. Several configuration values can be set at once with one
 * single call. 
 *
 * Please refer to the description of configurations for more
 * detail. The set of configuration keys that can be set through this
 * call is material prober specific. If a configuration setting is
 * not accepted (e.g. value out of range or configuration key invalid)
 * a message is printed to the report window and an error code is
 * returned.
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 * Examples:
 *
 * An example call to set the configuration key "temperature:" to 
 * the value "42" would look
 * like:
 *
 * ph_set_configuration("temperature:42", 1, comment_out, &comlen, &state_out)
 *
 **/
void ph_set_configuration(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Get a configuration value 
 *
 * Description:
 *
 * This call may be used to retrieve (parts) of the current
 * configuration. This information may be used for dataloging purposes
 * or for temporary storage of configuration parts for subsequent
 * change and recovery.
 *
 * Input parameter format: 
 * The input parameter must be one or more known configuration keys
 *
 * Output result format: 
 * The output format will be the configuration
 * definition for the given keys. The keys are included. The returned
 * format is suitable for subsequent calls to ph_set_configuration().
 *
 * Examples:
 *
 * To get the definition of the configuration with the key
 * "temperature:", the following call must be made:
 *
 * ph_get_configuration("temperature", 1, &comment_out, &comlen, &state_out)
 *
 **/
void ph_get_configuration(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Get ID information 
 *
 * Description:
 * 
 * The ph_get_id function can be used to get several ID information,
 * i.e. from the DUT board, the prober, the probers software
 * revision, or the prober driver revision. These IDs may only be
 * retrieved, if the HW supports this feature. Otherwise nothing
 * happens (an empty ID string is returned and an error message is
 * printed).  
 *
 * Input parameter format:
 *
 * The input parameter is a key name for the requested ID. The
 * following IDs may be requested:
 *
 * driver: the generic prober drivers version number
 *
 * plugin: the activated prober driver plugin
 *
 * equipment: the connected prober (including the firmware revision)
 *
 * probe-card: the probe card ID
 *
 * wafer: the current wafer ID
 *
 * If an ID is totally unknown to the driver for whateever reason, an
 * empty ID response is created and a warning is printed to the user
 * interface. Availability depends on the prober model, dut board,
 * etc.
 *
 * Output result format: 
 * The output format will repeat the key name
 * followed by an arbitrary string of identification data
 * 
 **/
void ph_get_id(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Get current prober driver parameters
 *
 * Description:
 *
 * This fucntion is used to retrieve status information that
 * changes during prober driver runtime. This information may be used
 * for datalog issues. The complete list of available status
 * information depends on the prober and the
 * prober driver plugin. Typical status information is e.g.:
 *
 * touchdown-count: the touchdown counter for the probe card
 *
 * temperature: the current chuck temperature (may differ from the
 * configured temperature)
 *
 * The requests below are only applicable for TSK Prober
 * Lot Number, Wafer ID, Wafer Number, Cassette Status, Chuck Temperature
 * WCR(Wafer Configruation Record): retrieve below information belongs
 * to WCR:
 *    Wafer Size, Die Height, Die Width, Wafer Units, Wafer Flat,
 *    Center X, Center Y, POS X, POS Y
 *
 * Input parameter format: 
 *  The input parameter is a key name for information request, including:
 *  touchdown-count, temperature, wafer_size, die_height, die_width, 
 *  wafer_units, wafer_flat, center_x, center_y, pos_x, pos_y, lot_number, 
 *  wafer_id, wafer_number, cass_status, chuck_temp
 *
 * Output result format: The function returns a string starting with
 * the requested parameter name, followed by the value (if it could be
 * retrieved). For multi site values, a list of values is returned.
 * 
 *
 * Example:
 *    WAFER_SIZE = PROB_HND_CALL(ph_get_status wafer_size);
 *    CHUCK_TEMP = PROB_HND_CALL(ph_get_status chuck_temp);
 *
 **/
void ph_get_status(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Set current prober driver parameters
 *
 * Description:
 *
 * This fucntion is used to set operating conditions (status, or parameter) 
 * for the Prober. It is the counterpart of "ph_get_status".
 * Currently the complete parameters are not implemented.
 *
 * Input parameter format: 
 *  The input parameter is a key name for information request.
 *
 * Output result format: The function returns a string starting with
 * the requested parameter name, followed by the value (if it could be
 * retrieved). For multi site values, a list of values is returned.
 *
 **/
void ph_set_status(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Start a nested timer 
 *
 * Description:
 *
 * ph_timer_start and ph_timer_stop provide a functionality of nested
 * timer counters. They may be used for datalog or detailed prober
 * driver profiling. ph_timer_start sets up a new nested
 * counter. ph_timer_stop stops the inner most timer and returns the
 * elapsed time measured in seconds.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 **/
void ph_timer_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed)  */
);

/**
 *
 * Stop a nested timer
 *
 * Description:
 *
 * ph_timer_start and ph_timer_stop provide a functionality of nested
 * timer counters. They may be used for datalog or detailed prober
 * driver profiling. ph_timer_start sets up a new nested
 * counter. ph_timer_stop stops the inner most timer and returns the
 * elapsed time measured in seconds.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format: 
 * The function will return a floating number,
 * representing the passed time of the timer.
 * 
 **/
void ph_timer_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

/**
 *
 * Perform the contact test 
 *
 * Description:
 * Retrieve contact test setup file and perform the contact test. 
 * Currently only TEL and TSK support this test.
 *
 * Input parameter format:
 * The contact test configuration file. If the file is not 
 * provided, the default configuration file 
 * /etc/opt/hp93000/soc/PH_libs/GenericProber/contact_test.cfg 
 * will be used 
 *
 * Output result format:
 * This call does not produce an output result string
 *
 **/
void ph_contact_test(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              0 (function completed successfully),
                              non-zero (function call failed) */
);

#endif 
