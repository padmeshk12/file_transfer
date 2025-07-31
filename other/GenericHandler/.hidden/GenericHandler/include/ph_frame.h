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
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 28 May 1999, Michael Vogt, created
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
#ifdef __cplusplus
extern "C" {
#endif

/* fixed directory paths or portions of them. During compile time, the
   location and the name of the global configuration file is
   defined. The pathname to this file is something like
   /etc/opt/hp??000/.../GenericHandler/Global-handler-config.cfg. 
   It should be build like
   PHLIB_CONF_PATH/PHLIB_CONF_DIR/PHLIB_GB_CONF_FILE */

#define PHLIB_CONF_PATH            "/opt/hp93000/testcell/phcontrol/"
#define PHLIB_CONF_DIR             "drivers/Generic_93K_Driver/GenericHandler/"
#define PHLIB_GB_CONF_FILE         "Global-handler-config.cfg"
#define PHLIB_S_ATTR_FILE          "shattrib"

/* if the following environment variable is defined during program
   execution, its content defines the global configuration file */
#define PHLIB_ENV_GCONF            "GLOBAL_HANDLER_CONFIGURATION"

/*--- typedefs --------------------------------------------------------------*/

typedef enum {
    PHFRAME_ACT_DRV_START = 0     /* ph_driver_start(3)                     */,
    PHFRAME_ACT_DRV_DONE          /* ph_driver_done(3)                      */,
    PHFRAME_ACT_LOT_START         /* ph_lot_start(3)                        */,
    PHFRAME_ACT_LOT_DONE          /* ph_lot_done(3)                         */,
    PHFRAME_ACT_STRIP_START       /* ph_strip_start(3)                      */,
    PHFRAME_ACT_STRIP_DONE        /* ph_strip_done(3)                       */,
    PHFRAME_ACT_DEV_START         /* ph_device_start(3)                     */,
    PHFRAME_ACT_DEV_DONE          /* ph_device_done(3)                      */,
    PHFRAME_ACT_PAUSE_START       /* ph_pause_start(3)                      */,
    PHFRAME_ACT_PAUSE_DONE        /* ph_pause_done(3)                       */,
    PHFRAME_ACT_HANDTEST_START    /* ph_handtest_start(3)                   */,
    PHFRAME_ACT_HANDTEST_STOP     /* ph_handtest_stop(3)                    */,
    PHFRAME_ACT_STEPMODE_START    /* ph_stepmode_start(3)                   */,
    PHFRAME_ACT_STEPMODE_STOP     /* ph_stepmode_stop(3)                    */,
    PHFRAME_ACT_REPROBE           /* ph_reprobe(3)                          */,
    PHFRAME_ACT_LEAVE_LEVEL       /* ph_leave_level(3)                      */,
    PHFRAME_ACT_CONFIRM_CONFIG    /* ph_confirm_configuration(3)            */,
    PHFRAME_ACT_ACT_CONFIG        /* ph_interactive_configuration(3)        */,
    PHFRAME_ACT_SET_CONFIG        /* ph_set_configuration(3)                */,
    PHFRAME_ACT_GET_CONFIG        /* ph_get_configuration(3)                */,
    PHFRAME_ACT_DATALOG_START     /* ph_datalog_start(3)                    */,
    PHFRAME_ACT_DATALOG_STOP      /* ph_datalog_stop(3)                     */,
    PHFRAME_ACT_GET_ID            /* ph_get_id(3)                           */,
    PHFRAME_ACT_GET_INS_COUNT     /* ph_get_insertion_count(3)              */,
    PHFRAME_ACT_TIMER_START       /* ph_timer_start(3)                      */,
    PHFRAME_ACT_TIMER_STOP        /* ph_timer_stop(3)                       */,
    PHFRAME_ACT_GET_STATUS        /* ph_get_status(3)                       */,
    PHFRAME_ACT_SET_STATUS        /* ph_set_status(3)                       */,
    PHFRAME_ACT_EXEC_GPIB_CMD     /* ph_exec_gpib_cmd(3)                    */,
    PHFRAME_ACT_EXEC_GPIB_QUERY   /* ph_exec_gpib_query(3)                  */,
    PHFRAME_ACT_GET_SRQ_STATUS_BYTE   /* ph_get_srq_status_byte(3)          */,
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
 * Start a handler driver session (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_driver_start(3) and ph_driver_done(3) are supposed to be called only
 * ONCE per testprogram execution. ph_driver_start(3) will establish the
 * connection to the handler. Therefore the handler configuration is
 * read, the interface session is opened, the handlers identity is
 * checked and the preconfigured data is dowloaded to the
 * handler. ph_driver_done(3) is the counterpart to close the connection.
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
 * Stop a handler driver session (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_driver_start(3) and ph_driver_done(3) are supposed to be called only
 * ONCE per testprogram execution. ph_driver_start(3) will establish the
 * connection to the handler. Therefore the handler configuration is
 * read, the interface session is opened, the handlers identity is
 * checked and the preconfigured data is dowloaded to the
 * handler. ph_driver_done(3) is the counterpart to close the connection.
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
 * handler. They can be used for later versions of the driver solution
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
 * handler. They can be used for later versions of the driver solution
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
 * Wait for device insertion (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_device_start(3) and ph_device_end(3) are mandatory for handler based
 * testing. ph_device_start(3) tells the handlers to plug in the next
 * (set of) device(s) into the sockets, i.e. it waits until the
 * handler notified this action. ph_device_done(3) will get the binning
 * and pass/fail information from the tester, apply a binning map on
 * this data and tell the handler how to bin the tested devices.
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
 * Bin devices (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_device_start(3) and ph_device_end(3) are mandatory for handler based
 * testing. ph_device_start(3) tells the handlers to plug in the next
 * (set of) device(s) into the sockets, i.e. it waits until the
 * handler notified this action. ph_device_done(3) will get the binning
 * and pass/fail information from the tester, apply a binning map on
 * this data and tell the handler how to bin the tested devices.
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
 * change the internal state of the handler driver. Usually no special
 * commands are sent to the handler while entering or leaving pause
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
 * change the internal state of the handler driver. Usually no special
 * commands are sent to the handler while entering or leaving pause
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
 * operator will simulate the handler actions manually (on driver's
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
 * operator will simulate the handler actions manually (on driver's
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
 * Issue a reprobe command to the handler (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_reprobe(3) causes the driver to issue a reprobe command to the
 * handler. This operation becomes void, if the handler does not
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
 * The only valid input parameters for handler drivers are:
 *
 * device - to leave the device level
 *
 * lot - to leave the lot level
 *
 * driver - to leave the driver level
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
 * Confirm the handler configuration (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function causes a complete printout of the handler
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
 * call is material handler specific. If a configuration setting is
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
 * Another example to set the SmarTest bin to handler bin map could
 * look like:
 *
 * PRB_HND(ph_set_configuration smartest_bin_to_handler_bin_map: 
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
 * be used for improved datalog functionality through the handler
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
 * be used for improved datalog functionality through the handler
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
 * i.e. from the DUT board, the handler, the handlers software
 * revision, or the handler driver revision. These IDs may only be
 * retrieved, if the HW supports this feature. Otherwise nothing
 * happens (an empty ID string is returned and an error message is
 * printed).  
 *
 * Input parameter format:
 *
 * The input parameter is a key name for the requested ID. The
 * following IDs may be requested:
 *
 * driver: the drivers version number and suported handler family
 *
 * handler: the connected handler model and family
 *
 * firmware: the handlers firmware revision
 *
 * dut_board: the dut board ID
 *
 * If an ID is totally unknown to the driver for whateever reason, an
 * empty ID response is created and a warning is printed to the user
 * interface. Availability depends on the handler model, dut board,
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
 * Get STATUS information (test cell client call)
 *
 * Authors: Ken Ward
 *
 * Description:
 * 
 * The ph_get_status function can be used to query various operating states
 * of the handler, such as VERSION, NAME, TESTSET, SETTEMP, etc.
 * i.e. from the DUT board, the handler, the handlers software
 * revision, or the handler driver revision. These IDs may only be
 * retrieved, if the HW supports this feature. Otherwise nothing
 * happens (an empty ID string is returned and an error message is
 * printed).  
 *
 * Input parameter format:
 *
 * The input parameter is a key name for the requested ID. The
 * following IDs may be requested:
 *
 * driver: the drivers version number and suported handler family
 *
 * handler: the connected handler model and family
 *
 * firmware: the handlers firmware revision
 *
 * dut_board: the dut board ID
 *
 * If an ID is totally unknown to the driver for whateever reason, an
 * empty ID response is created and a warning is printed to the user
 * interface. Availability depends on the handler model, dut board,
 * etc.
 *
 * Output result format: 
 * The output format will repeat the key name
 * followed by an arbitrary string of identification data
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
 * Set STATUS information (test cell client call)
 *
 * Authors: Ken Ward
 *
 * Description:
 * 
 * The ph_set_status function can be used to set operating conditions
 * for the handler, such as LOADER, START, STOP, and PLUNGER, as well as 
 * TESTSET, RUNMODE, SETTEMP, SETBAND, SETSOAK, TEMPCTRL, SITESEL, and SETNAME.
 * i.e. from the DUT board, the handler, the handlers software
 * revision, or the handler driver revision. These IDs may only be
 * retrieved, if the HW supports this feature. Otherwise nothing
 * happens (an empty ID string is returned and an error message is
 * printed).  
 *
 * Input parameter format:
 *
 * The input parameter is a key name for the requested ID. The
 * following IDs may be requested:
 *
 * driver: the drivers version number and suported handler family
 *
 * handler: the connected handler model and family
 *
 * firmware: the handlers firmware revision
 *
 * dut_board: the dut board ID
 *
 * If an ID is totally unknown to the driver for whateever reason, an
 * empty ID response is created and a warning is printed to the user
 * interface. Availability depends on the handler model, dut board,
 * etc.
 *
 * Output result format: 
 * The output format will repeat the key name
 * followed by an arbitrary string of identification data
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
 * Get site insertion count (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * ph_get_insertion_count(3) reads out the insertion counter from the
 * DUT board if applicable. An warning message is produced and an
 * empty string is returned, if the DUT board does not provide this
 * feature.
 * 
 * Input parameter format: 
 * No input parameters are expected for this call
 *
 * Output result format:
 *
 * The function returns a per site insertion count, i.e. a list of
 * numbers representing all available sites.
 * 
 ***************************************************************************/
void ph_get_insertion_count(
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
 * timer counters. They may be used for datalog or detailed handler
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
 * timer counters. They may be used for datalog or detailed handler
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


/*****************************************************************************
 *
 * Execute handler's setup and action command by GPIB
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * This fucntion is used to send handler setup and action command to setup handler
 * initial parameters and control handler.
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter is GPIB command of specfified handler.
 *
 * Output result format:
 *   Depend on GIPB command, the corresponding answer will be
 *   the string. It's the caller's responsibility to parse the answer
 *
 * Example:
 *    answer = PROB_HND_CALL(ph_exec_gpib_cmd O);
 *    answer = PROB_HND_CALL(ph_exec_gpib_cmd GAA -27.0,3.6);
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
 * Send protocol command to the equipment
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * This function is used to send protocol command (GPIB/RS232/LAN) to the equipment
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 * The first parameter is the protocol command to be sent and the second parameter
 * is the timeout value in the unit of millisecond. If the timeout value is 0, the
 * function will keep sending the command until it succeeds.
 *
 * Output result format:
 * None.
 *
 ***************************************************************************/
void ph_send_protocol_cmd(
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
 * Execute handler's query command by GPIB
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * This fucntion is used to send handler's query command to get handler parameters
 * during handler driver runtime.
 *
 * ------------------------------------------------------------------------
 *
 * Input parameter format:
 *  The input parameter is GPIB command of specfified handler.
 *
 * Output result format:
 *   Depend on GIPB command, the corresponding answer will be
 *   the string which contains the responsed message which has
 *   revmoed "\r\n" from handler.
 *
 *
 * Example:
 *    answer = PROB_HND_CALL(ph_exec_gpib_query site?);
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
