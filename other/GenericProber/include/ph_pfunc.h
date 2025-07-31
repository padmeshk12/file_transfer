/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_pfunc.h
 * CREATED  : 21 May 1999
 *
 * CONTENTS : General Interface to Prober Specific Plugins
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR27092 & CR25172
 *          : Garry  Xie, R&D Shanghai, CR28427
 *            Fabarca & Kun Xiao, R&D Shanghai, CR21589
 *            Danglin li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 21 May 1999, Michael Vogt, created
 *            11 Oct 1999, Michael Vogt, leveraged for prober access
 *                changed all IDs to avoid intermixing of prober and handler
 *                plugins
 *            24 Nov 1999, Michael Vogt, added prober plugin specific 
 *                interface functions
 *            24 Oct 2000, Chris Joyce, added new ping function to
 *                check GPIB communication
 *            August 2005, Jiawei Lin, CR27092 & CR25172:
 *              Add new plugin function type "phPFuncGetStatus_t";
 *              Expand "phPFuncAvailability_t" to 64 bits
 *            Jan 2006, Jiawei Lin, modify of phPFuncAvailability_t
 *              Add "unsigned long long" (ULL) before enum to support 64-bit
 *              under HPUX. Also note that "switch" statement fails to
 *              work for ULL, so we have to use "if" statement for compariation
 *              of phPFuncAvailability_t type.
 *            February 2006, Garry Xie , CR28427
 *              declare the "privateSetStatus" ."setSatusXXX" series functions
 *              are used to set parameters to the prober, like ALARM_BUZZER.      
 *            July 2006, fabarca & Kun Xiao, CR21589
 *              include file "ph_contacttest.h" 
 *              Add new plugin function type "phPFuncContacttest_t"                 
 *            Nov 2008, Danglin Li, CR41221 & CR42599
 *              declare the "privateExecGpibCmd" and "privateExecGpibQuery".
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

#ifndef _PH_PFUNC_H_
#define _PH_PFUNC_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "compile_compatible.h"
#include "ph_tcom.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_tools.h" 
#include "ph_contacttest.h"

/*--- defines ---------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

#define PHPFUNC_DIEPOS_NA (-9999999L)

#define MAX_STATUS_MSG_LEN  4096

/*--- typedefs --------------------------------------------------------------*/

/* Handle for a prober driver plugin */
typedef struct phPFuncStruct *phPFuncId_t;

/* Enumeration (flag) type, to define the existent functionality in
   the driver plugin: All functions should be implemented as stub
   functions. Dummy stubs must return a PHPFUNC_ERR_NA error code, if
   called.

   Insert more entries above the PHPFUNC_AV_DESTROY line. Never change
   existing entries */
typedef enum {
    PHPFUNC_AV_INIT        = 0x0000000000000001  /* used for phPFuncInit(3)         */,
    PHPFUNC_AV_RECONFIGURE = 0x0000000000000002  /* used for phPFuncReconfigure(3)  */,
    PHPFUNC_AV_RESET       = 0x0000000000000004  /* used for phPFuncReset(3)        */,
    PHPFUNC_AV_DRIVERID    = 0x0000000000000008  /* used for phPFuncDriverID(3)     */,
    PHPFUNC_AV_EQUIPID     = 0x0000000000000010  /* used for phPFuncEquipID(3)      */,
    PHPFUNC_AV_LDCASS      = 0x0000000000000020  /* used for phPFuncLoadCass(3)     */,
    PHPFUNC_AV_UNLCASS     = 0x0000000000000040  /* used for phPFuncUnloadCass(3)   */,
    PHPFUNC_AV_LDWAFER     = 0x0000000000000080  /* used for phPFuncLoadWafer(3)    */,
    PHPFUNC_AV_UNLWAFER    = 0x0000000000000100  /* used for phPFuncUnloadWafer(3)  */,
    PHPFUNC_AV_GETDIE      = 0x0000000000000200  /* used for phPFuncGetDie(3)       */,
    PHPFUNC_AV_BINDIE      = 0x0000000000000400  /* used for phPFuncBinDie(3)       */,
    PHPFUNC_AV_GETSUBDIE   = 0x0000000000000800  /* used for phPFuncGetSubDie(3)    */,
    PHPFUNC_AV_BINSUBDIE   = 0x0000000000001000  /* used for phPFuncBinSubDie(3)    */,
    PHPFUNC_AV_DIELIST     = 0x0000000000002000  /* used for phPFuncDieList(3)      */,
    PHPFUNC_AV_SUBDIELIST  = 0x0000000000004000  /* used for phPFuncSubDieList(3)   */,
    PHPFUNC_AV_REPROBE     = 0x0000000000008000  /* used for phPFuncReprobe(3)      */,
    PHPFUNC_AV_CLEAN       = 0x0000000000010000  /* used for phPFuncCleanProbe(3)   */,
    PHPFUNC_AV_PMI         = 0x0000000000020000  /* used for phPFuncPMI(3)          */,
    PHPFUNC_AV_PAUSE       = 0x0000000000040000  /* used for phPFuncSTPaused(3)     */,
    PHPFUNC_AV_UNPAUSE     = 0x0000000000080000  /* used for phPFuncSTUnpaused(3)   */,
    PHPFUNC_AV_TEST        = 0x0000000000100000  /* used for phPFuncTestCommand(3)  */,
    PHPFUNC_AV_DIAG        = 0x0000000000200000  /* used for phPFuncDiag(3)         */,
    PHPFUNC_AV_STATUS      = 0x0000000000400000  /* used for phPFuncStatus(3)       */,
    PHPFUNC_AV_UPDATE      = 0x0000000000800000  /* used for phPFuncUpdateState(3)  */,
    PHPFUNC_AV_CASSID      = 0x0000000001000000  /* used for phPFuncCassID(3)       */,
    PHPFUNC_AV_WAFID       = 0x0000000002000000  /* used for phPFuncWafID(3)        */,
    PHPFUNC_AV_PROBEID     = 0x0000000004000000  /* used for phPFuncProbeID(3)      */,
    PHPFUNC_AV_LOTID       = 0x0000000008000000  /* used for phPFuncLotID(3)        */,
    PHPFUNC_AV_STARTLOT    = 0x0000000010000000  /* used for phPFuncStartLot(3)     */,
    PHPFUNC_AV_ENDLOT      = 0x0000000020000000  /* used for phPFuncEndLot(3)       */,
    PHPFUNC_AV_COMMTEST    = 0x0000000040000000  /* used for phPFuncCommTest(3)     */,
    PHPFUNC_AV_GETSTATUS   = 0x0000000080000000  /* used for phPFuncGetStatus(3)    */,
    PHPFUNC_AV_SETSTATUS   = 0x0000000100000000LL/* used for phPFuncSetStatus(3)    */,
    PHPFUNC_AV_CONTACTTEST = 0x0000000200000000LL/* used for phPFuncContacttest(3), CR21589,fabarca&kunxiao */,
    PHPFUNC_AV_EXECGPIBCMD = 0x0000000400000000LL/* used for phPFuncExecGpibCmd(3)  */,
    PHPFUNC_AV_EXECGPIBQUERY=0x0000000800000000LL/* used for phPFuncExecGpibQuery(3)*/,
    PHPFUNC_AV_DESTROY     = 0x8000000000000000LL/* used for phPFuncDestroy(3)      */,

    PHPFUNC_AV_MINIMUM     = 0x0000000000400781  /* minimum functional set:
					   INIT, LDWAFER, UNLWAFER, 
					   GETDIE, BINDIE, STATUS */
} phPFuncAvailability_t;

typedef enum {
    PHPFUNC_SR_QUERY                     /* the last call to the
                                            driver plugin is returned,
                                            no further actions are
                                            done */,
    PHPFUNC_SR_RESET                     /* the plugin is informed,
                                            that the last plugin
                                            action (that has caused an
                                            error return value) will
                                            be re-tried by the framework
                                            and the plugin must perform
                                            transactions for this call from
                                            the start and any intermediate 
                                            transaction values being 
                                            discarded. */,
    PHPFUNC_SR_HANDLED                   /* the plugin is informed,
                                            that the last plugin
                                            action (that has caused an
                                            error return value) has
                                            been completed outside by
                                            some other component of
                                            the framework */,
    PHPFUNC_SR_ABORT                     /* the plugin is informed,
                                            that the last plugin call
                                            (that has caused an error
                                            return value) must finally
                                            be aborted by plugin, as
                                            soon as it is called again */
} phPFuncStatRequest_t;

typedef enum {
    PHPFUNC_ERR_OK = 0      /* no error */,
    PHPFUNC_ERR_NOT_INIT    /* not initialized */,
    PHPFUNC_ERR_INVALID_HDL /* the passed driver ID is not valid */,
    PHPFUNC_ERR_NA          /* plugin function is not available for this
			       prober type, see phPFuncAvailable(3) */,
    PHPFUNC_ERR_MEMORY      /* not enough memory available on heap */,
    PHPFUNC_ERR_CONFIG      /* invalid configuration */,
    PHPFUNC_ERR_MODEL       /* illegal prober model for this plugin */,
    PHPFUNC_ERR_WAITING     /* timeout during waiting for next die,
                               wafer, or cassette usually causing
                               SmarTest flags to checked and the
                               waiting hook function to be called, if
                               existent. The timeout occured while
			       waiting for a message coming from the
			       prober */,
    PHPFUNC_ERR_TIMEOUT     /* similar like PHPFUNC_ERR_WAITING, but the
			       timeout occured while trying to send
			       a message to the prober */,
    PHPFUNC_ERR_BINNING     /* unrecoverable error during binning */,
    PHPFUNC_ERR_GPIB        /* error during communication with GPIB intfc. */,
    PHPFUNC_ERR_PAT_DONE    /* the current stepping pattern has been
                               completed, the next higher level of
                               prober actions should now be performed:
                               sub-die probing -> die probing -> wafer
                               exchange -> cassette exchange. */,
    PHPFUNC_ERR_PAT_REPEAT  /* the current stepping pattern has been
			       stopped and should be repeated (e.g. a
			       wafer retest was enforced during a get
			       die operation), the next higher level
			       of prober actions should now be
			       performed: sub-die probing -> repeated
			       die probing -> wafer retest -> cassette
			       retest.  
			       This functionality is only
			       implemented to enforce a wafer retest */,
    PHPFUNC_ERR_ABORTED     /* the plugin's action was previously told
                               to be aborted by the event handler,
                               using the phPFuncStatus()
                               function. E.g. After waiting for a new
                               wafer timed out the event handler
                               called phPFuncStatus(ABORT). Than the
                               next call to the load wafer function
                               must return PHPFUNC_ERR_ABORTED */,
    PHPFUNC_ERR_EMPTY       /* a request was made to load a wafer from
                               a source that is empty (e.g. the
                               reference position) or if it was tried
                               to unload a wafer from an empty
                               chuck. If trying to load from an empty
                               input cassette, PHPFUNC_ERR_EMPTY or
                               PHPFUNC_ERR_PAT_DONE may be
                               returned. Both will cause similar
                               framework actions (trying to get a new
                               cassette) */,
    PHPFUNC_ERR_OCCUPIED    /* a request was made to unload a wafer to
                               a destination that is occupied
                               (e.g. the inspection tray) or if it was
                               tried to load a wafer to the already
                               occupied chuck */,
    PHPFUNC_ERR_PARAM       /* a passed parameter of the plugin call
                               is out of the valid range for this
                               plugin/prober */,
    PHPFUNC_ERR_ANSWER      /* a received answer from the prober does
                               not match the expected format or is out
                               of the range of expected SRQs */,
    PHPFUNC_ERR_FATAL       /* the prober is in a fatal state, the
                               test must be restarted from the
                               beginning */,    
    PHPFUNC_ERR_Z_LIMIT     /* prober Z height exceeded maximum limit, CR21589*/
} phPFuncError_t;

/*--- external variables ----------------------------------------------------*/

/*--- virtual function types ------------------------------------------------*/

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
 * The following function type definitions are used as virtual
 * prototypes for both, the driver functions declared in this file and
 * the plugin stubs used by the framework. This mechanism is used to
 * ensure a consistent interface definition as well as to provide an
 * easy to use plugin mechanism. If consistency is not met, errors
 * will occur during compilation time.
 *
 * From outside, the function names build out of the typename without
 * _t having an additional ...Plug..... string in the name may be
 * called. These calls are handled by the plugin module and sent to
 * internal functions of this module.
 *****************************************************************************/

/*****************************************************************************
 *
 * Check availability of plugin functions
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Each driver plugin must implement this function. The function is
 * independent from a currently active plugin instantiation (as
 * created with phPFuncInit(3)). It is specific for the implemented
 * driver plugin library. It does not require a plugin ID parameter as
 * all other functions. The task of this function is to return a flag
 * mask which summarizes the availability of plugin functions. The
 * result is a binary ORed mask of elements of the
 * phPFuncAvailability_t enumeration. Future plugins may increase the
 * number of provided functions but always must keep the existing
 * enumerator entries unchanged.
 *
 * Before calling a plugin function (e.g. phPFuncReprobe(3)), the
 * calling framework must check whether the plugin function is
 * implemented or not. E.g. 
 *
 * if (phPFuncAvailable() & PHPFUNC_AV_REPROBE) phPFuncReprobe(...);
 *
 * Note: To use this function, call phPlugPFuncAvailable()
 *
 * Returns: The availability mask
 *
 ***************************************************************************/
typedef phPFuncAvailability_t (phPFuncAvailable_t)(void);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncAvailable_t phPFuncAvailable;
#endif
phPFuncAvailable_t phPlugPFuncAvailable;

/*****************************************************************************
 *
 * Initialize prober specific plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Calling this function creates a new prober specific plugin, which
 * will be use for the direct prober interaction. The plugin will use
 * the passed <communication> (which is already active and which was
 * opened by the surrounding framework) for equipment
 * communication. The <logger> will be used for debug and error log of
 * plugin calls. The <estate> is used to log and change the current
 * prober state in a general manner. In principle only the prober
 * plugin is allowed to change the <estate> parameters. Other modules
 * of the framework will read out the <estate> from time to
 * time. The plugin therefore must ensure as best as possible that
 * the <estate> is up to date.
 *
 * After initializing the plugin, the <configuration> is checked by
 * the plugin against the prober specific capabilities without
 * performing real prober communications. Only tests according to the
 * plugin's capabilities may be tested here. During further startup
 * procedure, the surrounding driver framework will call the
 * communication test function phPlugPFuncCommTest() to check whether
 * it can talk to the prober and the reconfigure function
 * phPlugPFuncReconfigure() to perform all initial (or updating)
 * prober initialization steps.
 *
 * Note: To use this function, call phPlugPFuncInit()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncInit_t)(
    phPFuncId_t *driverID     /* the resulting driver plugin ID */,
    phTcomId_t tester         /* the tester/tcom session to be used */,
    phComId_t communication   /* the valid communication link to the HW
			         interface used by the prober */,
    phLogId_t logger          /* the data logger to be used */,
    phConfId_t configuration  /* the configuration manager */,
    phEstateId_t estate       /* the prober state */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncInit_t phPFuncInit;
#endif
phPFuncInit_t phPlugPFuncInit;

/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * During the driver's startup procedure and in case the configuration
 * has been changed by some other component of the system (through
 * operator input, hook functions or test cell client calls), the
 * plugin needs to be informed about these changes. Calling this
 * function causes a reread and validation of the configuration. Also
 * necessary reconfiguration steps for the prober will be performed,
 * e.g. change of chuck temperature, etc. It is the responsibility of
 * the plugin, to know the set of applicable configuration parameters
 * and to check for changes within this list of parameters.
 *
 * Note: To use this function, call phPlugPFuncReconfigure()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncReconfigure_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncReconfigure_t phPFuncReconfigure;
#endif
phPFuncReconfigure_t phPlugPFuncReconfigure;

/*****************************************************************************
 *
 * communication test
 *
 * Authors: Michael Vogt
 *          Chris Joyce
 *
 * Description:
 * 
 * This function may be used to test the communication between the prober
 * and the tester for message based probers.
 *
 * The plugin must first clear the event and message queues so no
 * events or messages are pending.  The plugin must try to send an ID
 * request to the prober the actual format of the request being plugin
 * specific.  If the sending of the ID fails the error code
 * PHPFUNC_ERR_TIMEOUT should be returned and *TestPassed set to
 * FALSE.  If the sending passes the plugin should wait the usual
 * heartbeat timeout.  If no answer is returned a PHPFUNC_ERR_WAITING
 * waiting should be returned and *TestPassed set to FALSE.  If any
 * kind of answer is returned then *TestPassed should be set to TRUE
 * and the error code PHPFUNC_ERR_OK returned.
 *
 * The expected model type should NOT be check for against known
 * configuration values.
 *
 * Note: To use this function, call phPlugPFuncCommTest()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncCommTest_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncCommTest_t phPFuncCommTest;
#endif
phPFuncCommTest_t phPlugPFuncCommTest;

/*****************************************************************************
 *
 * reset the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function may be used to reset the prober. The reset mechanism
 * and the reset actions are prober specific. The function could be
 * used during some error handling procedures or after recovery of a
 * lost connection.
 *
 * Note: This function is only available, if the HW interface of the
 * prober supports it. 
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncReset()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncReset_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncReset_t phPFuncReset;
#endif
phPFuncReset_t phPlugPFuncReset;

/*****************************************************************************
 *
 * return the driver identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function retrieves the identification string of the current
 * driver plugin. A typical identification string should include the
 * name of the plugin (repesenting the family of probers it is used
 * for) and a revision number or revision date. The ID string will be
 * set in an internal storage area of the plugin. The
 * parameter <*driverIdString> will point to this area after completion
 * of the call.
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Note: To use this function, call phPlugPFuncDriverID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncDriverID_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncDriverID_t phPFuncDriverID;
#endif
phPFuncDriverID_t phPlugPFuncDriverID;


/*****************************************************************************
 *
 * return the prober identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function retrieves the identification string from the
 * connected prober. A typical identification string should include
 * the name and correct type of the prober and its internal firmware
 * revision.
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the prober supports it. 
 * If it is not available, the driver
 * should guess a reasonable ID string, since it knows, to which kind
 * of prober it is talking (taken from the configuration).
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncEquipID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncEquipID_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncEquipID_t phPFuncEquipID;
#endif
phPFuncEquipID_t phPlugPFuncEquipID;


/*****************************************************************************
 *
 * Load an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The prober driver framework uses a unified prober model that
 * defines handling of cassettes, wafers, dice and sub-dice. Please
 * refer to the analysis document to find out more.
 *
 * This function loads a new input cassette of untested wafers. If an
 * input cassette is already loaded (due to operator intervention at
 * the prober) this function returns with the error code
 * PHPFUNC_ERR_OCCUPIED. It will be assumed, that wafer test can start
 * from this point as usual.
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and the cassette
 * has not been loaded yet.  It is the responsibility of the calling
 * function (the framework) to decide, whether the call to this
 * function should be repeated (and how often it should be repeated)
 * until the global waiting_for_cassette_timeout time is reached
 * (usually set to a value of 120 seconds or above). The
 * flag_check_interval may be used outside of this function to check
 * whether some user interaction took place at some other place of
 * SmarTest or to call a special 'waiting' hook function.
 *
 * Note: This function will only be called, if the test cell client
 * contains a cassette level (i.e. calls to ph_cassette_start and
 * ph_cassette_done)
 *
 * Conditions:
 *
 * The conditions describe the valid state of the abstract equipment
 * specific state model before and after this call has been
 * executed. The abstract equipment specific state does not need to
 * exactly reflect the real equipment state. It is the freedom of
 * plugin to delay visibility of real state changes in the abstract
 * prober model. The plugin function does not need to check the
 * preconditions, it may instead assume that they are valid. The
 * surrounding driver framework will ensure valid preconditions by
 * executing other plugin functions first and by controlling the
 * correct calling sequence. The postconditions must be assured by
 * this function call in case of successfull execution. In case of
 * unsuccessfull operation, the state must not be changed by the
 * plugin. The following terms use 'L' for 'lot is started', 'C' for
 * 'cassette is loaded', 'W' for 'wafer is loaded', 'P' for 'return
 * with PAT_DONE'
 *
 * Preconditions:
 *  L && !C && !W
 *
 * postconditions:
 * !P &&  L &&  C && (W || !W)    ||
 *  P && !W && !C && (L || !L)
 *
 * Note: To use this function, call phPlugPFuncLoadCass()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncLoadCass_t)(
    phPFuncId_t proberID       /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncLoadCass_t phPFuncLoadCass;
#endif
phPFuncLoadCass_t phPlugPFuncLoadCass;


/*****************************************************************************
 *
 * Unload an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The prober driver framework uses a unified prober model that
 * defines handling of cassettes, wafers, dice and sub-dice. Please
 * refer to the analysis document to find out more.
 *
 * This function unloads the input cassette. If the input cassette is
 * already unloaded (due to operator intervention at the prober, or
 * automatically after getting the last available wafer) this function
 * returns with the error code PHPFUNC_ERR_OK.
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and the cassette
 * has not been unloaded yet.  It is the responsibility of the calling
 * function (the framework) to decide, whether the call to this
 * function should be repeated (and how often it should be repeated)
 * until the global waiting_for_cassette_timeout time is reached
 * (usually set to a value of 120 seconds or above). The
 * flag_check_interval may be used outside of this function to check
 * whether some user interaction took place at some other place of
 * SmarTest or to call a special 'waiting' hook function.
 *
 * Note: This function will only be called, if the test cell client
 * contains a cassette level (i.e. calls to ph_cassette_start and
 * ph_cassette_done)
 *
 * Note: This function will only be called, if the test cell client
 * contains a cassette level (i.e. calls to ph_cassette_start and
 * ph_cassette_done)
 *
 * Conditions:
 *
 * The conditions describe the valid state of the abstract equipment
 * specific state model before and after this call has been
 * executed. The abstract equipment specific state does not need to
 * exactly reflect the real equipment state. It is the freedom of
 * plugin to delay visibility of real state changes in the abstract
 * prober model. The plugin function does not need to check the
 * preconditions, it may instead assume that they are valid. The
 * surrounding driver framework will ensure valid preconditions by
 * executing other plugin functions first and by controlling the
 * correct calling sequence. The postconditions must be assured by
 * this function call in case of successfull execution. In case of
 * unsuccessfull operation, the state must not be changed by the
 * plugin. The following terms use 'L' for 'lot is started', 'C' for
 * 'cassette is loaded', 'W' for 'wafer is loaded', 'P' for 'return
 * with PAT_DONE'
 *
 * Preconditions:
 *  L &&  C && !W
 *
 * postconditions:
 * !W && !C && !P &&  L           ||
 * !W && !C &&  P &&  (L || !L)
 *
 * Note: To use this function, call phPlugPFuncUnloadCass()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncUnloadCass_t)(
    phPFuncId_t proberID       /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncUnloadCass_t phPFuncUnloadCass;
#endif
phPFuncUnloadCass_t phPlugPFuncUnloadCass;


/*****************************************************************************
 *
 * Load a wafer to the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The prober driver framework uses a unified prober model that
 * defines handling of cassettes, wafers, dice and sub-dice. Please
 * refer to the analysis document to find out more.
 *
 * This function allows to load a wafer either from the input
 * cassette, the reference position or the inspection tray. A wafer
 * can only be loaded, if no other wafer is currently loaded.
 *
 * If loading is performed from source PHESTATE_WAFL_CHUCK, the plugin
 * is told that retesting of the current wafer will now start. The
 * plugin may need to index back to the first valid die/sub-die
 * position (dependent on the stepping mode).
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and the wafer has
 * not been loaded yet.  It is the responsibility of the calling
 * function (the framework) to decide, whether the call to this
 * function should be repeated (and how often it should be repeated)
 * until the global waiting_for_wafer_timeout time is reached (usually
 * set to a value of 60 seconds or above). The flag_check_interval may
 * be used outside of this function to check whether some user
 * interaction took place at some other place of SmarTest or to call a
 * special 'waiting' hook function.
 *
 * Conditions:
 *
 * The conditions describe the valid state of the abstract equipment
 * specific state model before and after this call has been
 * executed. The abstract equipment specific state does not need to
 * exactly reflect the real equipment state. It is the freedom of
 * plugin to delay visibility of real state changes in the abstract
 * prober model. The plugin function does not need to check the
 * preconditions, it may instead assume that they are valid. The
 * surrounding driver framework will ensure valid preconditions by
 * executing other plugin functions first and by controlling the
 * correct calling sequence. The postconditions must be assured by
 * this function call in case of successfull execution. In case of
 * unsuccessfull operation, the state must not be changed by the
 * plugin. The following terms use 'L' for 'lot is started', 'C' for
 * 'cassette is loaded', 'W' for 'wafer is loaded', 'P' for 'return
 * with PAT_DONE'
 *
 * Preconditions:
 *  L &&  C && !W               ||
 *  L &&  C &&  W               in case of a wafer retest with source
 *                              being the chuck
 *
 * postconditions:
 * !P &&  L &&  C && W          ||
 *  P && !W &&  C && L          ||
 *  P && !W && !C && (L || !L)  ||
 *
 * Note: To use this function, call phPlugPFuncLoadWafer()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncLoadWafer_t)(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t source        /* where to load the wafer
                                           from. PHESTATE_WAFL_OUTCASS
                                           is not a valid option! */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncLoadWafer_t phPFuncLoadWafer;
#endif
phPFuncLoadWafer_t phPlugPFuncLoadWafer;


/*****************************************************************************
 *
 * Unload a wafer from the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The prober driver framework uses a unified prober model that
 * defines handling of cassettes, wafers, dice and sub-dice. Please
 * refer to the analysis document to find out more.
 *
 * This function allows to unload a wafer either to the output
 * cassette, the reference position or the inspection tray. A wafer
 * can only be unloaded, if the chuck is currently loaded with a
 * wafer.
 *
 * If loading is performed to destination PHESTATE_WAFL_CHUCK, the
 * plugin is told that the current wafer is going to be retested. The
 * plugin may perform any prober specific tasks to initiate retesting
 * of a wafer, e.g. clear any binning information already sent to the
 * prober.
 *
 * In case of a wafer retest or a test on the reference wafer is
 * requested by SmarTest, it will happen that this function is called
 * just after a call to get die or get sub-die request. No binning
 * information will be sent to the plugin in this case. The plugin
 * must handle the situation correctly by performing the wafer unload
 * request and ignoring the missing bin call.
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and the wafer has
 * not been unloaded yet.  It is the responsibility of the calling
 * function (the framework) to decide, whether the call to this
 * function should be repeated (and how often it should be repeated)
 * until the global waiting_for_wafer_timeout time is reached (usually
 * set to a value of 60 seconds or above). The flag_check_interval may
 * be used outside of this function to check whether some user
 * interaction took place at some other place of SmarTest or to call a
 * special 'waiting' hook function.
 *
 * Conditions:
 *
 * The conditions describe the valid state of the abstract equipment
 * specific state model before and after this call has been
 * executed. The abstract equipment specific state does not need to
 * exactly reflect the real equipment state. It is the freedom of
 * plugin to delay visibility of real state changes in the abstract
 * prober model. The plugin function does not need to check the
 * preconditions, it may instead assume that they are valid. The
 * surrounding driver framework will ensure valid preconditions by
 * executing other plugin functions first and by controlling the
 * correct calling sequence. The postconditions must be assured by
 * this function call in case of successfull execution. In case of
 * unsuccessfull operation, the state must not be changed by the
 * plugin. The following terms use 'L' for 'lot is started', 'C' for
 * 'cassette is loaded', 'W' for 'wafer is loaded', 'P' for 'return
 * with PAT_DONE'
 *
 * Preconditions:
 *  L &&  C &&  W
 *
 * postconditions:
 * !W && !P &&  C &&  L           ||
 * !W &&  P &&  C &&  L           ||
 * !W &&  P && !C &&  (L || !L)   ||
 * !P &&  L &&  C &&  W           in case of a wafer retest with destination
 *                                being the chuck
 *
 * Note: To use this function, call phPlugPFuncUnloadWafer()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncUnloadWafer_t)(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer
                                           to. PHESTATE_WAFL_INCASS is
                                           not valid option! */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncUnloadWafer_t phPFuncUnloadWafer;
#endif
phPFuncUnloadWafer_t phPlugPFuncUnloadWafer;


/*****************************************************************************
 *
 * Step to next die
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This is the main handling function to initiate the probing of a
 * new (group of) die(s). This function must be implemented for every
 * driver plugin. Based on the configuration the driver asks the
 * prober to apply devices to all (or a subset) of possible sites. The
 * prober notifies, which sites are really used and which sites are
 * empty. The site population is then returned via the equipment
 * specific state controller (see phEstateGetSiteInfo(3) and
 * phEstateSetSiteInfo(3)).
 *
 * The return value of this function may be PHPFUNC_ERR_PAT_DONE to
 * indicate that the last die on the wafer has already been binned and
 * that a wafer exchange action should be initiated. In this case the
 * driver will indicate an empty site condition, set the LEVEL_END
 * flag and by this cause SmarTest to leave the die level and start
 * the wafer done actions.
 *
 * Multi Die Probing:
 * In case of multi die probing the plugin must work together with the
 * surrounding framework very carefully to avoid multiple die tests
 * and inconsistent binning data. There are several rules that must be
 * followed: 
 *
 * In case of the prober being in control to perform and chose the
 * stepping pattern (also in learnlist mode) the plugin is responsible
 * to set the correct site population in the equipment specific state
 * model. It must therefore retrieve the on-wafer information from the
 * prober. Since the framework does not necessarily log a wafermap in
 * this situation, it can not distinguish, whether dies have already
 * been probed and/or binned before. Therefore it will always report
 * the site population to SmarTest as set by the plugin in the
 * equipment specific state. SmarTest then will test all sites that
 * have been set populated. The plugin function phPfuncBinDie(3) will
 * receive binning and pass/fail information for all populated
 * sites. Sites that are currently not populated or deactivated for
 * other reasons, receive the bin number -1 and a PASS result by
 * default.
 *
 * In case of SmarTest being in control to perform the stepping
 * (explicit stepping mode) the framework knows how the wafer looks
 * like. The framework can therfore distinguish whether dies have
 * already been probed and avoid a retest by SmarTest to save time. It
 * performs this job according to the following algorithm: If a site
 * is set to be empty by the plugin (e.g. due to detected hang overs
 * of the probes at the edge of a wafer), SmarTest will not test this
 * site. If a site is set to be populated by the plugin, the framework
 * checks its internal wafermap and corrects the site state to be
 * empty, if the die has already been probed earlier on this
 * wafer. SmarTest will then not retest this device. In any case of
 * site information being empty (either set by the plugin or corrected
 * by the framework) the plugin will receive the bin code -1 and a
 * PASS result for an empty site during the call of phPfuncBinDie(3).
 *
 * Prober Pause:
 * If the prober is paused, while the plugin waits for the next die
 * and the the plugin receives an event, indicating that pause, it
 * must set the equipment specific state to paused and return with
 * PHPFUNC_ERR_WAITING. In that case the driver will either repeat the
 * call until the prober is out of pause again and the plugin returns
 * PHPFUNC_ERR_OK (if a prober pause should not lead to a pause of
 * SmarTest), or the driver will call the phPFuncSTPaused() function
 * followed by the phPFuncSTUNPaused() function and then repeat this
 * call (if a prober pause should lead to SmarTest pause). It is
 * possible, that any other functioin may be called while in SmarTest
 * pause (e.g. probe clean, etc.). As long as SmarTest does not leave
 * the die level of the applicatioin model, the call to this function
 * will be repeated after pause is done.
 *
 * Possible call sequences with pause operation (Smartest is not paused)
 *
 * Frame           Plugin           Estate
 *
 * ph_die_start
 *                 getDie (receives pause notification from prober)
 *                                  setPause(1)
 *                 return WAITING
 *                 getDie
 *                 return WAITING
 *                 getDie (receives unpause notification and next die)
 *                                  setPause(0)
 *                 return OK
 *
 *
 * Possible call sequences with pause operation (Smartest is paused)
 *
 * Frame           Plugin           Estate
 *
 * ph_die_start
 *                 getDie (receives pause notification from prober)
 *                                  setPause(1)
 *                 return WAITING
 * set empty sites and pause flag
 *
 * ph_pause_start
 *                 STPaused
 *
 * ph_pause_done
 *                 STUnpaused
 *                 synchronize unpause
 *                                  setPause(0)
 *
 * ph_die_done
 *
 * ph_die_start
 *                 getDie
 *                 return OK
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and no die(s) were
 * applied. A possible reason for this is that the prober has been set
 * into pause by the operator. This function would indicate the prober
 * pause condition in the equipment specific state description and let
 * the framework decide what to do about it. It is the responsibility
 * of the calling function (the framework) to decide, whether the call
 * to this function should be repeated (and how often it should be
 * repeated) until the global waiting_for_parts_timeout time is
 * reached (usually set to a value of 30 to 60 seconds). The
 * flag_check_interval may be used outside of this function to check
 * whether some user interaction took place at some other place of
 * SmarTest or to call a special 'waiting' hook function.
 *
 * Note: To use this function, call phPlugPFuncGetDie()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncGetDie_t)(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *dieX                /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the absolute X
                                 coordinate of the current die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems, etc.) must be performed by
                                 the calling framework */,
    long *dieY                /* same for Y coordinate */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncGetDie_t phPFuncGetDie;
#endif
phPFuncGetDie_t phPlugPFuncGetDie;


/*****************************************************************************
 *
 * Bin tested die
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is the main handling function, that sends the binning
 * information to the prober and initiates inking (if configured at
 * the prober). This function must be implemented for every driver
 * plugin. All decisions concerning the bins which are used must be
 * done by the calling framework (i.e. asking SmarTest for the test
 * results, remaping bin tables, taking care of untested devices,
 * etc.).
 *
 * The driver receives an array of bin numbers, which has a valid
 * bining information for each site that was originally populated
 * during the phPFuncGetDie(3) function call. The bin numbers count
 * from 0 to the maximum number of available bins at the prober. The
 * numbers 0 to max-1 refer to the bin names definition from the
 * configuration (prober_bin_ids). Bin number -1 is defined to be a
 * default retest bin. It may or may not be accepted by the
 * plugin. The feature of accepting a retest bin index of -1 is
 * related to the configuration prober_retest_bins. If this
 * configuration is given, the dramework will automatically refer to
 * it and select a bin index which is >=0. Therefore a -1 is only send
 * to the plugin, if prober_retest_bins is not defined and the
 * framework can not find out a valid binning information for the
 * current dies (e.g. during "skip device" operations, or if the
 * SmarTest bin code can not be mapped to a prober bin ID)
 *
 * In case, sub-die probing is performed, the binning information is
 * send to the plugin with the call phPFuncBinSubDie(3) and not by
 * this function. To ensure a symmetric plugin usage by the framework
 * and to give the plugin the chance to perform some end-of-die
 * operations, this function (phPFuncBinDie(3)) will still be called
 * but instead of a binning map a NULL pointer is transfered.
 *
 * For user convenience, error messages and other messages sent to the
 * user should always map the bin index to the defined prober bin
 * name. Accordingly, the bin index will only be used within the
 * driver framework for efficient SmarTest bin to prober bin mapping.
 *
 * In case of a wafer retest or a test on the reference wafer is
 * requested by SmarTest, it will happen that this function is NOT
 * called according to the expected call sequence. No binning
 * information will be sent to the plugin in this case. The plugin
 * must handle the situation correctly by performing the wafer unload
 * request that is issued instead and by ignoring the missing bin
 * call.
 *
 * If the prober is paused, while the plugin waits wants to bin the
 * die and the the plugin receives an event, indicating that pause,
 * and the prober has NOT yet binned the die it must set the equipment
 * specific state to paused and return with PHPFUNC_ERR_WAITING.  In
 * that case the driver will either repeat the call until the prober
 * is out of pause again and the plugin returns PHPFUNC_ERR_OK (if a
 * prober pause should not lead to a pause of SmarTest), or the driver
 * will initiate an empty device cycle to bring SmarTest into pause.
 * In that case the driver will call the phPFuncSTPaused() function
 * followed by the phPFuncSTUNPaused() function and then repeat this
 * call with the original binning information. Since this is not a
 * save operation mode, it should be avoided to accept the probers
 * pause, before the binning for the last devices was done. The driver
 * has no chance to find out, whether the operator has done something
 * else with the prober, that could have made the binning information
 * invalid. It is possible, that any other function may be called
 * while in SmarTest pause (e.g. probe clean, etc.). As long as
 * SmarTest does not leave the die level of the applicatioin model,
 * the call to this function will be repeated after pause is done.
 *
 * Possible call sequences with pause operation (Smartest is not paused)
 *
 * Frame           Plugin           Estate
 *
 * ph_die_done
 *                 binDie (receives pause notification from prober)
 *                                  setPause(1)
 *                 return WAITING
 *                 binDie
 *                 return WAITING
 *                 binDie (receives unpause notification and bins die)
 *                                  setPause(0)
 *                 return OK
 *
 *
 * Putting SmarTest into pause during a bin die operation is not
 * supported.
 *
 * Possible call sequences with pause operation (Smartest is paused)
 * Equipment State Conditions:
 *
 * On successfull completion of this function the site population must
 * be set to empty in the equipment specific state module, even if the
 * prober has already automatically stepped to the next die position.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout applies to this
 * function. 
 *
 * Note: To use this function, call phPlugPFuncBinDie()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncBinDie_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL, if
                                sub-die probing is active */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis or
                                NULL, if sub-die probing is active */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncBinDie_t phPFuncBinDie;
#endif
phPFuncBinDie_t phPlugPFuncBinDie;


/*****************************************************************************
 *
 * Step to next sub-die
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This is the main handling function to initiate the probing of a
 * new (group of) sub-die(s). Based on the configuration the driver
 * asks the prober to apply devices to all (or a subset) of possible
 * sites. The prober notifies, which sites are really used and which
 * sites are empty. The site population is then returned via the
 * equipment specific state controller (see phEstateGetSiteInfo(3) and
 * phEstateSetSiteInfo(3)).
 *
 * This function will only be called, if the configuration has
 * activated sub-die probing with parameter perform_subdie_probing set
 * to "yes". In this case, this plugin function must be implemented.
 *
 * The return value of this function may be PHPFUNC_ERR_PAT_DONE to
 * indicate that the last sub-die on the die has already been binned
 * and that a die step action should be initiated. In this case the
 * driver will indicate an empty site condition, set the LEVEL_END
 * flag and by this cause SmarTest to leave the sub-die level and
 * start the die done actions.
 *
 * Pause management works as described in phPfuncGetDie().
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and no sub-die(s)
 * were applied. A possible reason for this is that the prober has
 * been set into pause by the operator. This function would indicate
 * the prober pause condition in the equipment specific state
 * description and let the framework decide what to do about it. It is
 * the responsibility of the calling function (the framework) to
 * decide, whether the call to this function should be repeated (and
 * how often it should be repeated) until the global
 * waiting_for_parts_timeout time is reached (usually set to a value
 * of 30 to 60 seconds). The flag_check_interval may be used outside
 * of this function to check whether some user interaction took place
 * at some other place of SmarTest or to call a special 'waiting' hook
 * function.
 *
 * Note: To use this function, call phPlugPFuncGetSubDie()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncGetSubDie_t)(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *subdieX             /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the relative X
                                 coordinate of the current sub die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems) must be performed by the
                                 calling framework */,
    long *subdieY             /* same for Y coordinate */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncGetSubDie_t phPFuncGetSubDie;
#endif
phPFuncGetSubDie_t phPlugPFuncGetSubDie;


/*****************************************************************************
 *
 * Bin tested sub-dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is the main handling function, that sends the binning
 * information to the prober and initiates inking (if configured at
 * the prober). This function must be implemented in conjunction with
 * phPFuncGetSubDie(3). All decisions concerning the bins which are
 * used must be done by the calling framework (i.e. asking SmarTest
 * for the test results, remaping bin tables, taking care of untested
 * devices, etc.).
 *
 * The driver receives an array of bin numbers, which has a valid
 * bining information for each site that was originally populated
 * during the phPFuncGetSubDie(3) function call. The bin numbers count
 * from 0 to the maximum number of available bins at the prober. The
 * numbers 0 to max-1 refer to the bin names definition from the
 * configuration (prober_bin_ids). Bin number -1 is defined to be a
 * default retest bin. It may or may not be accepted by the
 * plugin. The feature of accepting a retest bin index of -1 is
 * related to the configuration prober_retest_bins. If this
 * configuration is given, the dramework will automatically refer to
 * it and select a bin index which is >=0. Therefore a -1 is only send
 * to the plugin, if prober_retest_bins is not defined and the
 * framework can not find out a valid binning information for the
 * current dies (e.g. during "skip device" operations, or if the
 * SmarTest bin code can not be mapped to a prober bin ID)
 *
 * In case, sub-die probing is performed, the binning information is
 * send to the plugin with this function phPFuncBinSubDie(3) and not
 * phPFuncBinDie(3) To ensure a symmetric plugin usage by the
 * framework and to give the plugin the chance to perform some
 * end-of-die operations, the phPFuncBinDie(3) function will still be
 * called but instead of a binning map a NULL pointer is transfered,
 * there.
 *
 * For user convenience, error messages and other messages sent to the
 * user should always map the bin index to the defined prober bin
 * name. Accordingly, the bin index will only be used within the
 * driver framework for efficient SmarTest bin to prober bin mapping.
 *
 * In case of a wafer retest or a test on the reference wafer is
 * requested by SmarTest, it will happen that this function is NOT
 * called according to the expected call sequence. No binning
 * information will be sent to the plugin in this case. The plugin
 * must handle the situation correctly by performing the wafer unload
 * request that is issued instead and by ignoring the missing bin
 * call.
 *
 * Pause management works as described in phPfuncBinDie().
 *
 * Equipment State Conditions:
 *
 * On successfull completion of this function the site population must
 * be set to empty in the equipment specific state module, even if the
 * prober has already automatically stepped to the next sub-die position.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncBinSubDie()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncBinSubDie_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncBinSubDie_t phPFuncBinSubDie;
#endif
phPFuncBinSubDie_t phPlugPFuncBinSubDie;


/*****************************************************************************
 *
 * Send die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * In learnlist probing mode (stepping_controlled_by is set to
 * "learnlist"), the plugin needs to teach the prober the stepping
 * pattern. Therefore this function is called. It is mandatory to be
 * implemented, if "learnlist" is valid value for the parameter
 * stepping_controlled_by for this plugin.
 *
 * The function will be called after phPFuncInit() and before any
 * phPFuncGetDie(3) function is called. The coordinates of the
 * stepping pattern are of the same format as for phPFuncGetDie(3).
 *
 * The plugin must not copy the learnlist given in <dieX> and <dieY>,
 * if it is needed later internally of the plugin (e.g. to step back
 * to the first die after wafer exchange). It is ensured by the
 * framework, that the learnlist remains stabel and accesible during
 * the driver lifetime. The plugin is not allowed to change any entry
 * of learnlist!
 *
 * This function may be implemented and functional even if the prober
 * does not provide learning mode. In this case, the plugin would take
 * over the responsibility to send explicit probe requests to the
 * prober and simulate a learnlist mode. The framework would not see a
 * difference.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncDieList()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncDieList_t)(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *dieX                /* this is the list of absolute X
                                 coordinates of the die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *dieY                 /* same for Y coordinate */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncDieList_t phPFuncDieList;
#endif
phPFuncDieList_t phPlugPFuncDieList;


/*****************************************************************************
 *
 * Send sub-die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * In learnlist probing mode (stepping_controlled_by is set to
 * "learnlist") in combination with perform_subdie_probing set to
 * "yes" and a subdie_step_pattern being defined, the plugin needs to
 * teach the prober the stepping pattern for the sub-dice. Therefore
 * this function is called. It is mandatory to be implemented, if
 * "learnlist" is valid value for the parameter stepping_controlled_by
 * for this plugin and "yes" is a valid parameter for the parameter
 * perform_subdie_probing
 *
 * The function will be called after phPFuncInit() and before any
 * phPFuncGetDie(3) or phPFuncGetSubDie(3) function is called. The
 * coordinates of the stepping pattern are of the same format as for
 * phPFuncGetSubDie(3).
 *
 * The plugin must not copy the learnlist given in <subdieX> and
 * <subdieY>, if it is needed later internally of the plugin (e.g. to
 * step back to the first subdie after die stepping). It is ensured by
 * the framework, that the learnlist remains stable and accesible
 * during the driver lifetime. The plugin is not allowed to change any
 * entry of learnlist!
 *
 * This function may be implemented and functional even if the prober
 * does not provide learning mode. In this case, the plugin would take
 * over the responsibility to send explicit probe requests to the
 * prober and simulate a learnlist mode. The framework would not see a
 * difference.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncSubDieList()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncSubDieList_t)(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *subdieX             /* this is the list of relative X
                                 coordinates of the sub-die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *subdieY             /* same for Y coordinate */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncSubDieList_t phPFuncSubDieList;
#endif
phPFuncSubDieList_t phPlugPFuncSubDieList;


/*****************************************************************************
 *
 * reprobe dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Calling this function asks the prober to reprobe the currently
 * applied dice. This operation may be neccessary if the continuity
 * test for some device tests fail.
 *
 * Note: This function is only available, if the HW interface of the
 * prober supports it. 
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncReprobe()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncReprobe_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncReprobe_t phPFuncReprobe;
#endif
phPFuncReprobe_t phPlugPFuncReprobe;


/*****************************************************************************
 *
 * Clean the probe needles
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function causes the prober to clean it's probe needles. This
 * function may be called during normal operations on a device counter
 * base, or initiated by an operator during SmarTest pause.
 *
 * It's the responsibility of the plugin to decide whether to put the
 * prober into pause or not before cleaning the probes. It must put
 * the prober back into its original state before returning.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncCleanProbe()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncCleanProbe_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncCleanProbe_t phPFuncCleanProbe;
#endif
phPFuncCleanProbe_t phPlugPFuncCleanProbe;


/*****************************************************************************
 *
 * Perform a probe mark inspection
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function causes a probe mark inspection and a probe to pad
 * optimization to be performed. This function may be called during
 * normal operations on a device counter base, or initiated by an
 * operator during SmarTest pause.
 *
 * It's the responsibility of the plugin to decide whether to put the
 * prober into pause or not before cleaning the probes. It must put
 * the prober back into its original state before returning.
 *
 * To perform its task the plugin may use the configuration value
 * z_overtravel_for_ptpo.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncPMI()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncPMI_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncPMI_t phPFuncPMI;
#endif
phPFuncPMI_t phPlugPFuncPMI;


/*****************************************************************************
 *
 * Notify SmarTest pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is ment to inform the plugin about SmarTest entering
 * th epause state (pause flag is set).  It's the responsibility of
 * the plugin to decide whether to put the prober into pause or not,
 * based on the configuration value of
 * stop_prober_on_smartest_pause. If it does, it must also change the
 * equipment specific state information. The behaviour of this
 * function and the function phPFuncSTUnpaused(3) should be symmetric.
 *
 * This function will also be called, if the SmarTest pause was
 * initiated by a previous prober stop. In this case the prober plugin
 * knows, that the prober is already paused.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncSTPaused()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncSTPaused_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncSTPaused_t phPFuncSTPaused;
#endif
phPFuncSTPaused_t phPlugPFuncSTPaused;


/*****************************************************************************
 *
 * Notify SmarTest un-pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is ment to inform the plugin about SmarTest leaving
 * the pause state (pause flag is set). It's the responsibility of the
 * plugin to decide whether to put the prober out of pause or not,
 * based on the configuration value of
 * stop_prober_on_smartest_pause. If it does, it must also change the
 * equipment specific state information. The behaviour of this
 * function and the function phPFuncSTPaused(3) should be symmetric.
 *
 * This function will also be called, if the SmarTest un-pause was
 * initiated by a previous prober unpause. In this case the prober plugin
 * knows, that the prober is already unpaused.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncUnpaused()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncSTUnpaused_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncSTUnpaused_t phPFuncSTUnpaused;
#endif
phPFuncSTUnpaused_t phPlugPFuncSTUnpaused;


/*****************************************************************************
 *
 * send a command string to the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is only available for message based probers. It
 * allows to send an arbitrary command string to the prober, using
 * the current communication session. It is the responsibility of the
 * calling function to ensure that the command string is of some
 * meaning to the prober. Also, if this function is called, the
 * impact to other parts of the framework must be considered
 * (e.g. changing of driver state, active configuration, etc.)
 *
 * Timeout:
 * The usual timeout for sending commands applies to this function.
 *
 * Note: To use this function, call phPlugPFuncTestCommand()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncTestCommand_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char *command            /* command string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncTestCommand_t phPFuncTestCommand;
#endif
phPFuncTestCommand_t phPlugPFuncTestCommand;


/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * In case of problems, this function may be used to read a string of
 * diagnostic information and possible error messages. Both the
 * diagnostic of the driver and the diagnostic of the prober will be
 * returned. Prober diagnostic (and or error messages) is only
 * available, if a message based prober is used.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncDiag()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncDiag_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **diag              /* pointer to probers diagnostic output */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncDiag_t phPFuncDiag;
#endif
phPFuncDiag_t phPlugPFuncDiag;


/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Calling phPFuncLoadCass(3), phPFuncUnloadCass(3),
 * phPFuncLoadWafer(3), phPFuncUnloadWafer(3), phPFuncGetDie(3), or
 * phPFuncGetSubDie(3) may result in a PHPFUNC_ERR_WAITING return
 * code, indicating that the requested operation has not been
 * completed yet. The Framework usually repeats the call until the
 * return code is different from PHPFUNC_ERR_WAITING. 
 * 
 * However, based on global timeout definitions, the operator may
 * abort the operation, if there is no chance to fullfill it (e.g. no
 * more wafers while waiting for wafers). In this case the framework
 * will call this function phPFuncStatus(3) using the PHPFUNC_SR_ABORT
 * action to indicate the plugin, that the last operation should be
 * finally aborted. The plugin may use this information to clear or
 * reset any internal state. It will than be called with the original
 * request and must return with error code PHPFUNC_ERR_ABORTED.
 *
 * In case some external framework component was able to solve the
 * current problem (waiting , etc) it should notify the plugin about
 * this fact to, using the PHPFUNC_SR_HANDLED action.
 *
 * If some other external framework component (e.g. the event
 * handling) needs to know the last operation requested to the plugin
 * caused a waiting state, it may use the PHPFUNC_SR_QUERY action.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncStatus()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncStatus_t)(
    phPFuncId_t proberID                /* driver plugin ID */,
    phPFuncStatRequest_t action         /* the current status action
                                           to perform */,
    phPFuncAvailability_t *lastCall     /* the last call to the
                                           plugin, not counting calls
                                           to this function */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncStatus_t phPFuncStatus;
#endif
phPFuncStatus_t phPlugPFuncStatus;


/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function may be called by the framework, to ask the plugin to
 * look out for any unsolicited events, e.g. the prober was put into
 * pause mode by the operator or was put back into running mode. The
 * plugin checks for any events stored in the communication layer and
 * changes the equipment specific state accordingly.
 *
 * Note: To use this function, call phPlugPUpdateState()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncUpdateState_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncUpdateState_t phPFuncUpdateState;
#endif
phPFuncUpdateState_t phPlugPFuncUpdateState;


/*****************************************************************************
 *
 * return the current cassette ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function retrieves the identification string from the
 * current cassette.
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the prober supports it. 
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncCassID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncCassID_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **cassIdString      /* resulting pointer to cassette ID string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncCassID_t phPFuncCassID;
#endif
phPFuncCassID_t phPlugPFuncCassID;


/*****************************************************************************
 *
 * return the current wafer ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function retrieves the identification string from the
 * current wafer.
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the prober supports it. 
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncWafID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncWafID_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **wafIdString      /* resulting pointer to wafer ID string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncWafID_t phPFuncWafID;
#endif
phPFuncWafID_t phPlugPFuncWafID;


/*****************************************************************************
 *
 * return the current probe card ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function retrieves the identification string from the
 * current probe card
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the prober supports it. 
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncProbeID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncProbeID_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **probeIdString     /* resulting pointer to probe card ID string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncProbeID_t phPFuncProbeID;
#endif
phPFuncProbeID_t phPlugPFuncProbeID;


/*****************************************************************************
 *
 * return the current lot ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function retrieves the identification string from the
 * current lot
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the prober supports it. 
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based probers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugPFuncProbeID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncLotID_t)(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **lotIdString       /* resulting pointer to lot ID string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncLotID_t phPFuncLotID;
#endif
phPFuncLotID_t phPlugPFuncLotID;


/*****************************************************************************
 *
 * Start a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The prober driver framework uses a unified prober model that
 * defines handling of cassettes, wafers, dice and sub-dice. Please
 * refer to the analysis document to find out more.
 *
 * This function tells the prober to start a new lot or waits until
 * the prober has started a new lot. It is ment for synchronization
 * during multiple lot testing and may also be used to wait for the
 * lot start signal at the very beginning of communication.
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and the lot
 * has not been loaded yet.  It is the responsibility of the calling
 * function (the framework) to decide, whether the call to this
 * function should be repeated (and how often it should be repeated)
 * until the global waiting_for_lot_timeout time is reached
 * (usually set to a value of 120 seconds or above). The
 * flag_check_interval may be used outside of this function to check
 * whether some user interaction took place at some other place of
 * SmarTest or to call a special 'waiting' hook function.
 *
 * Note: This function will only be called, if the test cell client
 * contains a lot level (i.e. calls to ph_lot_start and
 * ph_lot_done)
 *
 * Conditions:
 *
 * The conditions describe the valid state of the abstract equipment
 * specific state model before and after this call has been
 * executed. The abstract equipment specific state does not need to
 * exactly reflect the real equipment state. It is the freedom of
 * plugin to delay visibility of real state changes in the abstract
 * prober model. The plugin function does not need to check the
 * preconditions, it may instead assume that they are valid. The
 * surrounding driver framework will ensure valid preconditions by
 * executing other plugin functions first and by controlling the
 * correct calling sequence. The postconditions must be assured by
 * this function call in case of successfull execution. In case of
 * unsuccessfull operation, the state must not be changed by the
 * plugin. The following terms use 'L' for 'lot is started', 'C' for
 * 'cassette is loaded', 'W' for 'wafer is loaded', 'P' for 'return
 * with PAT_DONE'
 *
 * Preconditions:
 * !L && !C && !W
 *
 * postconditions:
 * !P &&  L && !C && !W           ||
 * !P &&  L &&  C && (W || !W)    ||
 *  P && !W && !C && !L
 *
 * Note: To use this function, call phPlugPFuncLoadCass()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncStartLot_t)(
    phPFuncId_t proberID       /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncStartLot_t phPFuncStartLot;
#endif
phPFuncStartLot_t phPlugPFuncStartLot;


/*****************************************************************************
 *
 * End a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The prober driver framework uses a unified prober model that
 * defines handling of cassettes, wafers, dice and sub-dice. Please
 * refer to the analysis document to find out more.
 *
 *
 * This function is the counterpart for the StartLot function. After
 * the function has been executed, the prober has ended the current
 * lot and is ready to start a new lot now.
 *
 * Timeout:
 *
 * The configuration holds an entry for the flag_check_interval This
 * time interval is usually set to a value of 1 to 5 seconds. This
 * function returns with a PHPFUNC_ERR_WAITING error as soon as the
 * defined time in flag_check_interval has elapsed and the lot
 * has not been unloaded yet.  It is the responsibility of the calling
 * function (the framework) to decide, whether the call to this
 * function should be repeated (and how often it should be repeated)
 * until the global waiting_for_lot_timeout time is reached
 * (usually set to a value of 120 seconds or above). The
 * flag_check_interval may be used outside of this function to check
 * whether some user interaction took place at some other place of
 * SmarTest or to call a special 'waiting' hook function.
 *
 * Note: This function will only be called, if the test cell client
 * contains a lot level (i.e. calls to ph_lot_start and
 * ph_lot_done)
 *
 * Conditions:
 *
 * The conditions describe the valid state of the abstract equipment
 * specific state model before and after this call has been
 * executed. The abstract equipment specific state does not need to
 * exactly reflect the real equipment state. It is the freedom of
 * plugin to delay visibility of real state changes in the abstract
 * prober model. The plugin function does not need to check the
 * preconditions, it may instead assume that they are valid. The
 * surrounding driver framework will ensure valid preconditions by
 * executing other plugin functions first and by controlling the
 * correct calling sequence. The postconditions must be assured by
 * this function call in case of successfull execution. In case of
 * unsuccessfull operation, the state must not be changed by the
 * plugin. The following terms use 'L' for 'lot is started', 'C' for
 * 'cassette is loaded', 'W' for 'wafer is loaded', 'P' for 'return
 * with PAT_DONE'
 *
 * Preconditions:
 *  L && !C && !W
 *
 * postconditions:
 * !W && !C && !L && (P || !P)
 *
 * Note: To use this function, call phPlugPFuncUnloadCass()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncEndLot_t)(
    phPFuncId_t proberID       /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncEndLot_t phPFuncEndLot;
#endif
phPFuncEndLot_t phPlugPFuncEndLot;

/*****************************************************************************
 *
 * Get status ( parameter, information,etc.) from Prober by key name
 *
 * Authors: Jiawei Lin
 *
 * Description:
 * 
 * Implements the following information request by CR27092&CR25172:
 *    (1)STDF WCR, 
 *    (2)Wafer Number, Lot Number, Chuck Temperatue, Name of 
 *       Prober setup, Cassette Status. These are only applicable for
 *       TSK Prober
 *    (3)z_contact_height, planarity_window
 *  
 * Implements the following information request by CR28427:
 *      wafer status, error code, multisite location number
 *
 * Note: This function is only available if the HW interface and GPIB 
 *       command supports it.
 *  
 *       Never free the memory of retuned pointer, or overwrite the 
 *       content of the data area.
 *
 * 
 *
 * Note: To use this function, call phPlugPFuncGetStatus()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncGetStatus_t)(
    phPFuncId_t proberID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
);


/*****************************************************************************
 *
 * Set status ( parameter ) to Prober by key name
 *
 * Authors: Garry Xie
 *
 * Description:
 * 
 * Implements the following setting to prober, request by CR28427:
 *                  alarm buzzer
 *
 *
 * Note: This function is only available if the HW interface and GPIB 
 *       command supports it.
 *  
 *       Never free the memory of retuned pointer, or overwrite the 
 *       content of the data area.
 *
 * 
 *
 * Note: To use this function, call phPlugPFuncSetStatus()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncSetStatus_t)(
    phPFuncId_t proberID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncGetStatus_t phPFuncGetStatus;
#endif
phPFuncGetStatus_t phPlugPFuncGetStatus;
#ifdef _PH_PFUNC_INTERNAL_
phPFuncSetStatus_t phPFuncSetStatus;
#endif
phPFuncSetStatus_t phPlugPFuncSetStatus;


/*****************************************************************************
 *
 * Contact Test
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao  Shangai
 * Description:
 *
 * This function perform contact test to find z contact height
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncContacttest_t)(
    phPFuncId_t proberID                          /* driver plugin ID */,
    phContacttestSetup_t  contacttestSetupID      /* contact test ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncContacttest_t phPFuncContacttest;
#endif
phPFuncContacttest_t phPlugPFuncContacttest;

/*****************************************************************************
 *
 * Send setup and action command to prober by GPIB
 *
 * Authors: Danglin Li
 *
 * Description:
 *
 * This fucntion is used to send prober setup and action command to
 * setup prober initial parameters and control prober.
 *
 * Note: This function is only available if the HW interface and GPIB
 *       command supports it.
 *
 *       Never free the memory of retuned pointer, or overwrite the
 *       content of the data area.
 *
 *
 *
 * Note: To use this function, call phPlugPFuncExecGpibCmd()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncExecGpibCmd_t)(
    phPFuncId_t proberID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncExecGpibCmd_t phPFuncExecGpibCmd;
#endif
phPFuncExecGpibCmd_t phPlugPFuncExecGpibCmd;

/*****************************************************************************
 *
 * Send query command to prober by GPIB
 *
 * Authors: Danglin Li
 *
 * Description:
 *
 * This fucntion is used to send prober query command to get prober parameters
 * during prober driver runtime.
 *
 * Note: This function is only available if the HW interface and GPIB
 *       command supports it.
 *
 *       Never free the memory of retuned pointer, or overwrite the
 *       content of the data area.
 *
 *
 *
 * Note: To use this function, call phPlugPFuncExecGpibQuery()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncExecGpibQuery_t)(
    phPFuncId_t proberID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncExecGpibQuery_t phPFuncExecGpibQuery;
#endif
phPFuncExecGpibQuery_t phPlugPFuncExecGpibQuery;

/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is the counterpart to the phPFuncInit(3) function. It
 * closes down the current driver session, clears and frees all
 * internal memory. The proberID becomes invalid after this call.
 *
 * Conditions:
 *
 * The conditions describe the valid state of the abstract equipment
 * specific state model before and after this call has been
 * executed. The abstract equipment specific state does not need to
 * exactly reflect the real equipment state. It is the freedom of
 * plugin to delay visibility of real state changes in the abstract
 * prober model. The plugin function does not need to check the
 * preconditions, it may instead assume that they are valid. The
 * surrounding driver framework will ensure valid preconditions by
 * executing other plugin functions first and by controlling the
 * correct calling sequence. The postconditions must be assured by
 * this function call in case of successfull execution. In case of
 * unsuccessfull operation, the state must not be changed by the
 * plugin. The following terms use 'L' for 'lot is started', 'C' for
 * 'cassette is loaded', 'W' for 'wafer is loaded', 'P' for 'return
 * with PAT_DONE'
 *
 * Preconditions:
 * !L && !C && !W
 *
 * postconditions:
 * !W && !C && !L
 *
 * Note: To use this function, call phPlugPFuncDestroy()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phPFuncError_t (phPFuncDestroy_t)(
    phPFuncId_t proberID     /* driver plugin ID */
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncDestroy_t phPFuncDestroy;
#endif
phPFuncDestroy_t phPlugPFuncDestroy;




typedef void (phPFuncSimResults_t)(
    phPFuncError_t *dummyReturnValues,
    int dummyReturnCount
);

#ifdef _PH_PFUNC_INTERNAL_
phPFuncSimResults_t phPFuncSimResults;
#endif
phPFuncSimResults_t phPlugPFuncSimResults;

#ifdef __cplusplus
}
#endif

#endif /* ! _PH_PFUNC_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
