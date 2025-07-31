/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_frame.h
 * CREATED  : 28 May 1999
 *
 * CONTENTS : Entry point to driver framework
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR 27092 & CR 25172
 *            fabarca & kunxiao, R&D Shanghai, CR21589
 *            Dangln Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 28 May 1999, Michael Vogt, created
 *            Aug 2005, CR27092 & CR 25172
 *              Implement the "ph_get_status" and "ph_set_status" function
 *              call. The former could be used to request parameter/information
 *              from Prober. Thus more datalog are possible, like WCR(Wafer 
 *              Configuration Record),Wafer_ID, Wafer_Number, Cassette Status 
 *              and Chuck Temperature. The latter could be used to set operating
 *              parameter for Prober, now it is only an interface without real
 *              functionality.
 *           July 2006, CR21589
 *              declare the "ph_contact_test" function call. This function could be 
 *              used to perform contact test to get z contact height automatically.
 *           Nov 2008, CR41221 & CR42599
 *              Declare the "ph_exec_gpib_cmd" and "ph_exec_gpib_query" functions call.
 *              The function "ph_exec_gpib_cmd" could be used to send prober setup and
 *              action commands to setup prober initial parameters and control prober.
 *              The function "ph_exec_gpib_query" could be used to inquiry prober status
 *              and parameters.
 *
 * Instructions:
 *
 * 1) Copy this template to as many .h files as you require
 *
 * 2) Use the command 'make depend' to make the new
 *    source files visible to the makefile utility
 *
 * 3) To support automatic man page (documentation) generation, follow the
 *    instructions given for the function template below.
 * 
 * 4) Put private functions and other definitions into separate header
 * files. (divide interface and private definitions)
 *
 *****************************************************************************/
                                                                     
#ifndef _PH_FRAME_H_
#define _PH_FRAME_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/* fixed directory paths or portions of them. During compile time, the
   location and the name of the global configuration file is
   defined. The pathname to this file is something like
   /etc/opt/hp??000/.../GenericProber/Global-prober-config.cfg. 
   It should be build like
   PHLIB_CONF_PATH/PHLIB_CONF_DIR/PHLIB_GB_CONF_FILE */

#ifndef HP83000_ROOT
#define HP83000_ROOT
#endif

#define PHLIB_CONF_PATH            "/etc" HP83000_ROOT
#define PHLIB_CONF_DIR             "PH_libs/GenericProber"
#define PHLIB_GB_CONF_FILE         "Global-prober-config.cfg"
#define PHLIB_S_ATTR_FILE          "spattrib"
#define PHLIB_CONTACT_TEST_FILE    "contact-test.cfg"

/* if the following environment variable is defined during program
   execution, its content defines the global configuration file */
#define PHLIB_ENV_GCONF            "GLOBAL_PROBER_CONFIGURATION"

/*--- typedefs --------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PHFRAME_ACT_DRV_START = 0     /* ph_driver_start(3)                     */,
    PHFRAME_ACT_DRV_DONE          /* ph_driver_done(3)                      */,
    PHFRAME_ACT_LOT_START         /* ph_lot_start(3)                        */,
    PHFRAME_ACT_LOT_DONE          /* ph_lot_done(3)                         */,
    PHFRAME_ACT_CASS_START        /* ph_cassette_start(3)                   */,
    PHFRAME_ACT_CASS_DONE         /* ph_cassette_done(3)                    */,
    PHFRAME_ACT_WAF_START         /* ph_wafer_start(3)                      */,
    PHFRAME_ACT_WAF_DONE          /* ph_wafwe_done(3)                       */,
    PHFRAME_ACT_DIE_START         /* ph_device_start(3)                        */,
    PHFRAME_ACT_DIE_DONE          /* ph_device_done(3)                         */,
    PHFRAME_ACT_SUBDIE_START      /* ph_subdie_start(3)                     */,
    PHFRAME_ACT_SUBDIE_DONE       /* ph_subdie_done(3)                      */,

    PHFRAME_ACT_PAUSE_START       /* ph_pause_start(3)                      */,
    PHFRAME_ACT_PAUSE_DONE        /* ph_pause_done(3)                       */,

    PHFRAME_ACT_HANDTEST_START    /* ph_handtest_start(3)                   */,
    PHFRAME_ACT_HANDTEST_STOP     /* ph_handtest_stop(3)                    */,
    PHFRAME_ACT_STEPMODE_START    /* ph_stepmode_start(3)                   */,
    PHFRAME_ACT_STEPMODE_STOP     /* ph_stepmode_stop(3)                    */,

    PHFRAME_ACT_REPROBE           /* ph_reprobe(3)                          */,
    PHFRAME_ACT_CLEAN             /* ph_clean_probe(3)                      */,
    PHFRAME_ACT_PMI               /* ph_pmi(3)                              */,
    PHFRAME_ACT_INSPECT           /* ph_inspect_wafer(3)                    */,
    PHFRAME_ACT_LEAVE_LEVEL       /* ph_leave_level(3)                      */,
    PHFRAME_ACT_CONFIRM_CONFIG    /* ph_confirm_configuration(3)            */,
    PHFRAME_ACT_ACT_CONFIG        /* ph_interactive_configuration(3)        */,
    PHFRAME_ACT_SET_CONFIG        /* ph_set_configuration(3)                */,
    PHFRAME_ACT_GET_CONFIG        /* ph_get_configuration(3)                */,
    PHFRAME_ACT_DATALOG_START     /* ph_datalog_start(3)                    */,
    PHFRAME_ACT_DATALOG_STOP      /* ph_datalog_stop(3)                     */,
    PHFRAME_ACT_GET_ID            /* ph_get_id(3)                           */,
    PHFRAME_ACT_GET_STATUS        /* ph_get_status(3)                       */,
    PHFRAME_ACT_SET_STATUS        /* ph_set_status(3)                       */,

    PHFRAME_ACT_TIMER_START       /* ph_timer_start(3)                      */,
    PHFRAME_ACT_TIMER_STOP        /* ph_timer_stop(3)                       */,
    /*Begin CR21589, fabarca & Kun Xiao , 6/12/2006*/
    PHFRAME_ACT_CONTACT_TEST      /* ph_contact_test(3)                     */,
    PHFRAME_ACT_EXEC_GPIB_CMD     /* ph_exec_gpib_cmd(3)                    */,
    PHFRAME_ACT_EXEC_GPIB_QUERY   /* ph_exec_gpib_query(3)                  */,
    PHFRAME_ACT_GET_SRQ_STATUS_BYTE /* ph_get_srq_status_byte */,
    PHFRAME_ACT_READ_STATUS_BYTE  /* ph_read_status_byte(3)                 */,
    PHFRAME_ACT_SEND_MESSAGE      /* ph_send(3)                             */,
    PHFRAME_ACT_RECEIVE_MESSAGE   /* ph_receive(3)                          */,

    PHFRAME_ACT__END              /* end of enum list marker, don't change  */
} phFrameAction_t;

/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

/*****************************************************************************
 * To allow a consistent interface definition and documentation, the
 * documentation is automatically extracted from the comment section
 * of the below function declarations. All text within the comment
 * region just in front of a function header will be used for
 * documentation. Additional text, which should not be visible in the
 * documentation (like this text), must be put in a separate comment
 * region, ahead of the documentation comment and separated by at
 * least one blank line.
 *
 * To fill the documentation with life, please fill in the angle
 * bracketed text portions (<>) below. Each line ending with a colon
 * (:) or each line starting with a single word followed by a colon is
 * treated as the beginning of a separate section in the generated
 * documentation man page. Besides blank lines, there is no additional
 * format information for the resulting man page. Don't expect
 * formated text (like tables) to appear in the man page similar as it
 * looks in this header file.
 *
 * Function parameters should be commented immediately after the type
 * specification of the parameter but befor the closing bracket or
 * dividing comma characters.
 *
 * To use the automatic documentation feature, c2man must be installed
 * on your system for man page generation.
 *****************************************************************************/

/*****************************************************************************
 *
 * Start a prober driver session (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_driver_start(3) and ph_driver_done(3) are supposed to be called only
 * ONCE per testprogram execution. ph_driver_start(3) will establish the
 * connection to the prober. Therefore the prober configuration is
 * read, the interface session is opened, the probers identity is
 * checked and the preconfigured data is dowloaded to the
 * prober. ph_driver_done(3) is the counterpart to close the connection.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_driver_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Stop a prober driver session (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_driver_start(3) and ph_driver_done(3) are supposed to be called only
 * ONCE per testprogram execution. ph_driver_start(3) will establish the
 * connection to the prober. Therefore the prober configuration is
 * read, the interface session is opened, the probers identity is
 * checked and the preconfigured data is dowloaded to the
 * prober. ph_driver_done(3) is the counterpart to close the connection.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_driver_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Start a new lot of devices (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_lot_start(3) and ph_lot_end(3) are optional calls to the
 * driver. These functions usually do not communicate to the
 * prober. They can be used for later versions of the driver solution
 * that incorporate improved internal data log solutions.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_lot_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Complete a lot of devices (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_lot_start(3) and ph_lot_end(3) are optional calls to the
 * driver. These functions usually do not communicate to the
 * prober. They can be used for later versions of the driver solution
 * that incorporate improved internal data log solutions.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_lot_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Start a new cassette of untested wafers
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_cassette_start(3) and ph_cassette_end(3) are optional calls to
 * the driver, depending on the configuration and setup of the prober
 * and the customer needs. The default test cell client does not
 * include these levels. They may be added by the AE to fullfill
 * specific customer needs (e.g. log files on a per cassette level,
 * etc.).
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
 ***************************************************************************/
void ph_cassette_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * End a new cassette of untested wafers
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_cassette_start(3) and ph_cassette_end(3) are optional calls to
 * the driver, depending on the configuration and setup of the prober
 * and the customer needs. The default test cell client does not
 * include these levels. They may be added by the AE to fullfill
 * specific customer needs (e.g. log files on a per cassette level,
 * etc.).
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
 ***************************************************************************/
void ph_cassette_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Load a new wafer
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_wafer_start(3) and ph_wafer_end(3) are mandatory calls to the
 * driver. They must appear in each test cell client used for wafer
 * probing. Usually, this function will load a new untested wafer from
 * the input cassette (if existent). If no more wafers are available,
 * the function will return and set the LEVEL END flag to return to
 * the next higher level of operation in the test cell client. In
 * case of optional wafer retest or test on a reference wafer, the
 * function will load the correct wafer instead of the next untested
 * wafer, based on the flags that were set during the last call to
 * ph_pause_done(3).
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_wafer_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Unload the current wafer
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_wafer_start(3) and ph_wafer_end(3) are mandatory calls to the
 * driver. They must appear in each test cell client used for wafer
 * probing. Usually, this function will unload the current wafer from
 * the chuck (if not already done automatically by the prober after
 * testing the last die) to the output cassette (if existent). If no
 * more wafers are available and the prober notifies this fact, the
 * LEVEL END flag will be set to go up one level in the application
 * model. In case of optional wafer retest or test on a reference
 * wafer, the function will unload the current wafer to some special
 * storage area (or leave it on the chuck) instead of unloading to the
 * output cassette.  This is based on the flags that were set during
 * the last call to ph_pause_done(3).
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_wafer_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Wait for die probing (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_device_start(3) and ph_device_end(3) are mandatory for prober based
 * testing. ph_device_start(3) tells the probers to step to the next
 * die(s) i.e. it waits until the prober or the prober driver plugin
 * notified this action. ph_device_done(3) will get the binning and
 * pass/fail information from the tester, apply a binning map on this
 * data and tell the prober how to bin the tested dies.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_device_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Bin dies (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_device_start(3) and ph_device_end(3) are mandatory for prober based
 * testing. ph_device_start(3) tells the probers to step to the next
 * die(s) i.e. it waits until the prober or the prober driver plugin
 * notified this action. ph_device_done(3) will get the binning and
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
 ***************************************************************************/
void ph_device_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Wait for die probing (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_subdie_start(3) and ph_subdie_end(3) are optional for prober
 * based testing. The default test cell client does contain a subdie
 * level. It may be used, even if no subdie probing must be performed
 * (i.e. there are no subdies on the wafer). The subdie probing only
 * gets affective, if the configuration value 'perform_subdie_probing'
 * is set to "yes". ph_subdie_start(3) tells the probers to step to
 * the next subdie(s) on the current die, i.e. it waits until the
 * prober or the prober driver plugin notified this
 * action. ph_subdie_done(3) will get the binning and pass/fail
 * information from the tester, apply a binning map on this data and
 * tell the prober how to bin the tested subdies.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_subdie_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Bin subdies (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_subdie_start(3) and ph_subdie_end(3) are optional for prober
 * based testing. The default test cell client does contain a subdie
 * level. It may be used, even if no subdie probing must be performed
 * (i.e. there are no subdies on the wafer). The subdie probing only
 * gets affective, if the configuration value 'perform_subdie_probing'
 * is set to "yes". ph_subdie_start(3) tells the probers to step to
 * the next subdie(s) on the current die, i.e. it waits until the
 * prober or the prober driver plugin notified this
 * action. ph_subdie_done(3) will get the binning and pass/fail
 * information from the tester, apply a binning map on this data and
 * tell the prober how to bin the tested subdies.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_subdie_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Enter pause mode in test cell client (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The driver must be notified whenever pause actions in the
 * test cell client are entered or left. This is to ensure, that the
 * driver can correctly interprete CI system flags (e.g. for device
 * check or retest, wafer retest, etc...). Entering and leaving the
 * pause mode (i.e. calling ph_pause_start(3) and ph_pause_done(3)) will
 * change the internal state of the prober driver. Usually no special
 * commands are sent to the prober while entering or leaving pause
 * mode. CI flags are read when leaving the pause mode. The flags may
 * affect behavior of subsequent calls to ph_device_start(3) and other
 * functions.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_pause_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Leave puase mode in test cell client (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The driver must be notified whenever pause actions in the
 * test cell client are entered or left. This is to ensure, that the
 * driver can correctly interprete CI system flags (e.g. for device
 * check or retest, wafer retest, etc...). Entering and leaving the
 * pause mode (i.e. calling ph_pause_start(3) and ph_pause_done(3)) will
 * change the internal state of the prober driver. Usually no special
 * commands are sent to the prober while entering or leaving pause
 * mode. CI flags are read when leaving the pause mode. The flags may
 * affect behavior of subsequent calls to ph_device_start(3) and other
 * functions.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_pause_done(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Go into hand test mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Hand test mode may be entered and left explicitely through calls to
 * the functions ph_handtest_start(3) and ph_handtest_stop(3). Hand
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
 ***************************************************************************/
void ph_handtest_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Leave hand test mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Hand test mode may be entered and left explicitely through calls to
 * the functions ph_handtest_start(3) and ph_handtest_stop(3). Hand
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
 ***************************************************************************/
void ph_handtest_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Go into single step mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Single step mode may be entered and left explicitely through calls
 * to the functions ph_stepmode_start(3) and
 * ph_stepmode_stop(3). Single Step mode may be initiated from the
 * pause dialog. During single step mode the driver breaks before each
 * execution of a prober/prober command initiated by the application
 * model. During each break the operator is asked to confirm the next
 * step.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_stepmode_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Leave single step mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Single step mode may be entered and left explicitely through calls
 * to the functions ph_stepmode_start(3) and
 * ph_stepmode_stop(3). Single Step mode may be initiated from the
 * pause dialog. During single step mode the driver breaks before each
 * execution of a prober/handler command initiated by the application
 * model. During each break the operator is asked to confirm the next
 * step.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_stepmode_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Issue a reprobe command to the prober (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_reprobe(3) causes the driver to issue a reprobe command to the
 * prober. This operation becomes void, if the prober does not
 * support this functionality on its interface. It can still occur in
 * the test cell client but the call will then have no effect.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_reprobe(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Perform a probe clean action on the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * If the prober and the prober driver plugin supports this feature,
 * the probe needles are cleaned by this call. In case the feature is
 * not available, a message is printed to the log output. This
 * function may be called in any place of the test cell client, but
 * not before ph_driver_start(3).
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_clean_probe(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Perform a probe mark inspection
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * If the prober and the prober driver plugin supports this feature, a
 * probe mark inspection followed by a probe to pad optimization is
 * performed by the prober. In case the feature is not available, a
 * message is printed to the log output. This function may be called
 * in any place of the test cell client, but not before
 * ph_driver_start(3).
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_pmi(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Move current wafer to the inspection tray
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Not yet designed.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_inspect_wafer(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Leave an test cell client level (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_leave_level is a universal facility to force the application
 * model to leave a certain number of levels. The driver sets the QUIT
 * flag and keeps it set until the ph_..._done operation of the given
 * level is reached. On that level the LEVEL END flag is set. These
 * operations force the test cell client to keep on leaving levels
 * until the driver resets the QUIT flag and leaves the final level by
 * a single LEVEL END instruction. This functionality may be used
 * e.g. within a pause dialog to enforce the test cell client to
 * start a new lot (by leaving the "device" level). Note that this
 * function can be used to retest rejected devices within an extra
 * lot.
 *
 * Input parameter format: 
 * The only valid input parameters for prober drivers are:
 *
 * subdie   - to leave the subdie level
 *
 * die      - to leave the die level
 *
 * wafer    - to leave the wafer level
 *
 * cassette - to leave the casset te level
 *
 * lot      - to leave the lot level
 *
 * driver   - to leave the driver level
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_leave_level(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Confirm the prober configuration (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function causes a complete printout of the prober
 * configuration to the user interface. It then asks the operator for
 * confirmation of the settings. If the operator accepts the
 * configuration, this function returns succesfully. Otherwise, the
 * function returns with an error condition, stopping the application
 * model.
 *
 * A call of this function is optional. The driver would work even
 * without configuration confirmation.
 *
 * Note:
 * A possible enhancement of this function would be to allow an
 * interactive reconfiguration, if some changes are necessary to the
 * current configuration settings.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_confirm_configuration(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Reconfigure the driver interactively (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_interactive_reconfiguration(3) provides a mechanism to change
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
 ***************************************************************************/
void ph_interactive_configuration(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Set or change a configuration value (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function can be used to change certain configuration values at
 * runtime from calls issued by the test cell client. It is not
 * recommended to make a lot of configuration setting calls for two
 * reasons: 1. The test cell client becomes handler/prober specific,
 * which should be avoided. 2. Missuse of this function may cause
 * malfunction of the driver. However, the function exists to provide
 * the highest level of flexibility to developper of the application
 * model. 
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
 * An example call from the test cell client to set the
 * configuration key "temperature:" to the value "42" would look
 * like:
 *
 * PRB_HND(ph_set_configuration temperature: 42)
 *
 * Another example to set the SmarTest bin to prober bin map could
 * look like:
 *
 * PRB_HND(ph_set_configuration smartest_bin_to_prober_bin_map: 
 * [ [ 1 ] [ 2 ] [ 2 ] [ 3 ] [ 3 ] [ 97 98 ] [ 97 98 ] [ 99 ] ] )
 *
 ***************************************************************************/
void ph_set_configuration(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Get a configuration value (test cell client call)
 *
 * Authors: Michael Vogt
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
 * temp = PRB_HND(ph_get_configuration temperature:)
 *
 * After the call, the test cell client variable will be set to the
 * value "temperature: 42"
 *
 * Warning: 
 * The length of the return string is restricted by the CPI
 * to CI_MAX_COMMENT_LEN. If the result needs to be truncated, an
 * error message is produced and the call to this function returns
 * with CI_CALL_ERROR.
 * 
 ***************************************************************************/
void ph_get_configuration(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Start or resume the data logging (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Not yet implemented
 *
 * The functions ph_datalog_start(3) and ph_datalog_stop(3) are planned to
 * be used for improved datalog functionality through the prober
 * driver. It is not clear know, how this datalog would look like.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_datalog_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Stop (suspend) the data logging  (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Not yet implemented
 *
 * The functions ph_datalog_start(3) and ph_datalog_stop(3) are planned to
 * be used for improved datalog functionality through the prober
 * driver. It is not clear know, how this datalog would look like.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_datalog_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Get ID information (test cell client call)
 *
 * Authors: Michael Vogt
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
 ***************************************************************************/
void ph_get_id(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Get current prober driver parameters
 *
 * Authors: Michael Vogt
 *          Jiawei Lin
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
 * ------------------------------------------------------------------------
 *
 * By CR27092 & CR25172, below information could be retrieved and datalog:
 *
 * WCR(Wafer Configruation Record): retrieve below information belongs
 * to WCR:
 *    Wafer Size, Die Height, Die Width, Wafer Units, Wafer Flat,
 *    Center X, Center Y, POS X, POS Y
 * Lot Number, Wafer ID, Wafer Number, Cassette Status, Chuck Temperature
 *    These requests are only applicable for TSK Prober
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format: 
 *  The input parameter is a key name for information request.
 *  The following key name is implementated for CR27092 & CR25172
 *      wafer_size, die_height, die_width, wafer_units, wafer_flat,
 *      center_x, center_y, pos_x, pos_y, lot_number, wafer_id, wafer_number,
 *      cass_status, chuck_temp
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
 ***************************************************************************/
void ph_get_status(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Set current prober driver parameters
 *
 * Authors: Jiawei Lin
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
 *
 ***************************************************************************/
void ph_set_status(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Start a nested timer (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_timer_start(3) and ph_timer_stop(3) provide a functionality of nested
 * timer counters. They may be used for datalog or detailed prober
 * driver profiling. ph_timer_start(3) sets up a new nested
 * counter. ph_timer_stop(3) stops the inner most timer and returns the
 * elapsed time measured in seconds.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 * This call does not produce an output result string
 * 
 ***************************************************************************/
void ph_timer_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Stop a nested timer (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_timer_start(3) and ph_timer_stop(3) provide a functionality of nested
 * timer counters. They may be used for datalog or detailed prober
 * driver profiling. ph_timer_start(3) sets up a new nested
 * counter. ph_timer_stop(3) stops the inner most timer and returns the
 * elapsed time measured in seconds.
 *
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format: 
 * The function will return a floating number,
 * representing the passed time of the timer.
 * 
 ***************************************************************************/
void ph_timer_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

/*Begin of Huatek Modifications, Donnie Tu, 04/22/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free the driver framework
 *
 * Authors: Donnie Tu
 *
 * Description:
 * Free the driver framework just before exiting the program
 *
 * Returns: None
 *
 ***************************************************************************/
void freeDriverFrameworkF(void);
/*End of Huatek Modifications*/


/*****************************************************************************
 *
 * Retrieve contact test setup file
 *
 * Authors: Fabrizio Arca
 *
 * Description:
 * Retrieve contact test setup file from test cell client and forward to
 * phFrameContacttest function in ph_contacttest.c
 * Returns: None
 *
 ***************************************************************************/
void ph_contact_test(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Execute prober setup and action command by GPIB
 *
 * Authors: Danglin Li
 *
 * Description:
 *
 * This fucntion is used to send prober setup and action command to setup prober
 * initial parameters and control prober.
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter is GPIB command of specfified prober.
 *
 * Output result format:
 *   Depend on GIPB command, the corresponding answer will be
 *   the string which is hexadecimal SRQ number or SRQ plus "," plus "MC"
 *   or "MF" which means "completion" or "failure". 
 *
 *
 * Example:
 *    PROB_HND_CALL(ph_exec_gpib_cmd O);
 *
 ***************************************************************************/
void ph_exec_gpib_cmd(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Execute prober query command by GPIB
 *
 * Authors: Danglin Li
 *
 * Description:
 *
 * This fucntion is used to send prober query command to get prober parameters
 * during prober driver runtime.
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter is GPIB command of specfified prober.
 *
 * Output result format:
 *   Depend on GIPB command, the corresponding answer will be
 *   the string which contains the responsed message which has
 *   revmoed "\r\n" from prober.
 *
 *
 * Example:
 *    PROB_HND_CALL(ph_exec_gpib_query site?);
 *
 ***************************************************************************/
void ph_exec_gpib_query(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);



/*****************************************************************************
 *
 * get an expected SRQ status byte
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * This fucntion is used to get a SRQ status byte
 *
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter is an expected SRQ list. It's optional.
 *
 * Output result format:
 *  The SRQ acquired is written in the comment_out parameter.
 *
 ***************************************************************************/
void ph_get_srq_status_byte(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * get an expected SRQ status byte with timeout
 *
 * Authors: Adam Huang
 *
 * Description:
 *
 * This fucntion is used to get a SRQ status byte with timeout
 * 
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter is an expected SRQ list with token "," and timeout value in millisecond. It's optional.
 *
 * Output result format:
 *  The SRQ acquired is written in the comment_out parameter.
 *
 ***************************************************************************/
void ph_read_status_byte(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * send command to equipment
 *
 * Authors: Adam Huang
 *
 * Description:
 *
 * This fucntion is used send command to equipment
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter is the command which will be sent to handler and timeout value in millisecond. It's optional.
 *
 ***************************************************************************/
void ph_send(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * receive reply from equipment
 *
 * Authors: Adam Huang
 *
 * Description:
 *
 * This fucntion is used receive reply from equipment
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter the timeout value in millisecond. It's optional.
 *
 ***************************************************************************/
void ph_receive(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Clean up all the data strucures created by ph_driver_start.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 * This function is just a CI function-type shell for freeDriverFrameworkF.
 * This is not a "tradintional" PH driver call and it is supposed to support
 * the Tangram project only.
 *
 * Input parameter format:
 * No input parameters are expected for this call
 *
 * Output result format:
 * The function will return a floating number,
 * representing the passed time of the timer.
 *
 ***************************************************************************/
void ph_driver_free(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);


/*****************************************************************************
 *
 * Get the site related information from  the driver. This function should
 * be called after the ph_device_start() function.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Input parameter format:
 * No input parameters are expected for this call
 *
 * Output result format:
 * The return string (via comment_out parameter) is a string with format like this:
 * "EQPSITE:A1 A2 A3 A4 A5 A6|SITE:B1 B2 B3 B4 B5|DIEX:C1 C2 C3 C4 C5|DIEY:D1 D2 D3 D4 D5",
 * Ax representing the equipment site id of an enabled site;
 * Bx representing the tester site number of an enabled site;
 * Cx representing the X coordinate value of an enabled site;
 * Dx representing the Y coordinate value of an enabled site;
 *
 ***************************************************************************/
void ph_get_site_config(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Set the test result to  the driver.
 * This function should be called before the ph_device_done() function.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Input parameter format:
 * The format of the input string of this function looks like:
 * "A1 A2 A3 A4 A5|B1 B2 B3 B4 B5|C1 C2 C3 C4 C5"
 * Ax representing the site number of an enabled tester site
 * Bx representing the hard bin id of the corresponding tester site
 * Cx representing the soft bin id of the corresponding tester site
 *
 * Output result format:
 * None.
 ***************************************************************************/
void ph_set_test_results(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Set the equipment test result to  the driver.
 * This function should be called before the ph_device_done() function.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Input parameter format:
 * The format of the input string of this function looks like:
 * "A1 A2 A3 A4 A5|B1 B2 B3 B4 B5"
 * Ax representing the site id of an enabled equipment site
 * Bx representing the bin id of the corresponding equipment site
 *
 * Output result format:
 * None.
 ***************************************************************************/
void ph_set_equipment_test_results(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Get the error messages from  the driver. During driver execution, the fatal
 * and error messages are buffered. When this function is called, the buffered
 * error messages will be returned and the buffer will be cleared.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Input parameter format:
 * No input parameters are expected for this call
 *
 * Output result format:
 * A string which contains all the error messages accumulated since last call
 * of this function.
 ***************************************************************************/
void ph_get_error_message(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Interrpt the current driver function execution.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Input parameter format:
 * None.
 *
 * Output result format:
 * None.
 ***************************************************************************/
void ph_interrupt(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Get the stepping pattern file path.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Input parameter format:
 * None.
 *
 * Output result format:
 * The stepping pattern file path is written in the comment_out parameter.
 ***************************************************************************/
void ph_get_stepping_pattern_file(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

/*****************************************************************************
 *
 * Set the stepping pattern file path. This function should be called before
 * the ph_driver_start() function is triggered.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Input parameter format:
 * The path of the stepping pattern file is passed via the parm_list_input parameter.
 *
 * Output result format:
 * None.
 ***************************************************************************/
void ph_set_stepping_pattern_file(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

#ifdef __cplusplus
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/

#endif /* ! _PH_FRAME_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
