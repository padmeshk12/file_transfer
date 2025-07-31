/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_hfunc.h
 * CREATED  : 21 May 1999
 *
 * CONTENTS : General Interface to Handler Specific Plugins
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 21 May 1999, Michael Vogt, created
 *             8 Feb 2001, Chris Joyce, added new ping function to
 *                check GPIB communication
 *             24 Oct 2014, Magco Li, added stripStart function and 
 *                and stripDone function
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

#ifndef _PH_HFUNC_H_
#define _PH_HFUNC_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/
#include "compile_compatible.h"
#include "ph_tcom.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"

/*--- defines ---------------------------------------------------------------*/
#define MAX_STATUS_MSG 4096

/*--- typedefs --------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/* Handle for a handler driver plugin */
typedef struct phFuncStruct *phFuncId_t;

/* Enumeration (flag) type, to define the existent functionality in
   the driver plugin: All functions should be implemented as stub
   functions. Dummy stubs must return a PHFUNC_ERR_NA error code, if
   called.

   Insert more entries below the PHFUNC_AV_DIAG line. Never change
   existing entries */
typedef enum {
    PHFUNC_AV_INIT            = 0x00000001  /* used for phFuncInit(3)           */,
    PHFUNC_AV_RECONFIGURE     = 0x00000002  /* used for phFuncReconfigure(3)    */,
    PHFUNC_AV_RESET           = 0x00000004  /* used for phFuncReset(3)          */,
    PHFUNC_AV_DRIVERID        = 0x00000008  /* used for phFuncDriverID(3)       */,
    PHFUNC_AV_EQUIPID         = 0x00000010  /* used for phFuncEquipID(3)        */,
    PHFUNC_AV_START           = 0x00000020  /* used for phFuncGetStart(3)       */,
    PHFUNC_AV_BIN             = 0x00000040  /* used for phFuncBinDevice(3)      */,
    PHFUNC_AV_REPROBE         = 0x00000080  /* used for phFuncReprobe(3)        */,
    PHFUNC_AV_COMMAND         = 0x00000100  /* used for phFuncSendCommand(3)    */,
    PHFUNC_AV_QUERY           = 0x00000200  /* used for phFuncSendQuery(3)      */,
    PHFUNC_AV_DIAG            = 0x00000400  /* used for phFuncDiag(3)           */,

    PHFUNC_AV_BINREPR         = 0x00000800  /* used for phFuncBinReprobe(3)     */,
    PHFUNC_AV_PAUSE           = 0x00001000  /* used for phFuncSTPaused(3)       */,
    PHFUNC_AV_UNPAUSE         = 0x00002000  /* used for phFuncSTUnpaused(3)     */,
    PHFUNC_AV_STATUS          = 0x00004000  /* used for phFuncStatus(3)         */,
    PHFUNC_AV_UPDATE          = 0x00008000  /* used for phFuncUpdateState(3)    */,
    PHFUNC_AV_COMMTEST        = 0x00010000  /* used for phFuncCommTest(3)       */,
    PHFUNC_AV_STRIP_INDEXID   = 0x00020000  /* used for phFuncStripIndexID(3)  Oct 14 2004 kaw    */,
    PHFUNC_AV_STRIPID         = 0x00040000  /* used for phFuncStripMaterialID(3)  Oct 14 2004 kaw */,
    PHFUNC_AV_GET_STATUS      = 0x00080000  /* used for phFuncSetStatus(3)    Feb 07 2005 kaw     */,
    PHFUNC_AV_SET_STATUS      = 0x00100000  /* used for phFuncGetStatus(3)    Feb 07 2005 kaw     */,
    PHFUNC_AV_LOT_START       = 0x00200000  /* used for phFuncLotStart(3)     Feb 23 2005 kaw     */,
    PHFUNC_AV_LOT_DONE        = 0x00400000  /* used for phFuncLotDone(3)      Feb 23 2005 kaw     */,
    PHFUNC_AV_EXECGPIBCMD     = 0x00800000  /* used for phFuncExecGpibCmd(3)  */,
    PHFUNC_AV_EXECGPIBQUERY   = 0x01000000  /* used for phFuncExecGpibQuery(3)*/,
    PHFUNC_AV_GETSRQSTATUSBYTE= 0x02000000  /* used for phFuncGetSrqStatusByte(3)*/,
    PHFUNC_AV_STRIP_START     = 0X04000000  /* used for phFuncGetStripStart(3)*/,
    PHFUNC_AV_STRIP_DONE      = 0X08000000  /* used for phFuncGetStripDone(3)*/,

    PHFUNC_AV_DESTROY         = 0x10000000  /* used for phFuncDestroy(3)        */,
    PHFUNC_AV_MINIMUM         = 0x00000061  /* minimum functional set:
					   init, start, and bin */
} phFuncAvailability_t;

typedef enum {
    PHFUNC_SR_QUERY                      /* the last call to the
                                            driver plugin is returned,
                                            no further actions are
                                            done */,
    PHFUNC_SR_RESET                      /* the plugin is informed,
                                            that the last plugin
                                            action (that has caused an
                                            error return value) will
                                            be re-tried by the framework
                                            and the plugin must perform
                                            transactions for this call from
                                            the start and any intermediate 
                                            transaction values being 
                                            discarded. */,
    PHFUNC_SR_HANDLED                    /* the plugin is informed,
                                            that the last plugin
                                            action (that has caused an
                                            error return value) has
                                            been completed outside by
                                            some other component of
                                            the framework */,
    PHFUNC_SR_ABORT                      /* the plugin is informed,
                                            that the last plugin call
                                            (that has caused an error
                                            return value) must finally
                                            be aborted by plugin, as
                                            soon as it is called again */
} phFuncStatRequest_t;

typedef enum {
    PHFUNC_ERR_OK = 0      /* no error */,
    PHFUNC_ERR_NOT_INIT    /* not initialized */,
    PHFUNC_ERR_INVALID_HDL /* the passed driver ID is not valid */,
    PHFUNC_ERR_NA          /* plugin function is not available for this
			      handler type, see phFuncAvailable(3) */,
    PHFUNC_ERR_MEMORY      /* not enough memory available on heap */,
    PHFUNC_ERR_CONFIG      /* invalid configuration */,
    PHFUNC_ERR_TIMEOUT     /* timeout during waiting for parts or other
			      kind of communication (lower level) */,
    PHFUNC_ERR_BINNING     /* invalid bin request */,

    PHFUNC_ERR_GPIB        /* error during communication with GPIB intfc. */,
    PHFUNC_ERR_LAN         /* error during communication with LAN intfc. */,
    PHFUNC_ERR_RS232       /* error during communication with RS232 intfc*/,
    PHFUNC_ERR_MODEL       /* illegal prober model for this plugin */,
    PHFUNC_ERR_WAITING     /* timeout during waiting for an action to complete */,
    PHFUNC_ERR_ABORTED     /* the plugin's action was previously told
                              to be aborted by the event handler,
                              using the phFuncStatus()
                              function. E.g. After waiting for a
                              device timed out the event handler
                              called phFuncStatus(ABORT). Than the
                              next call to wait for devices must
                              return PHFUNC_ERR_ABORTED */,
    PHFUNC_ERR_ANSWER      /* a received answer from the handler does
                              not match the expected format or is out
                              of the range of expected SRQs */,
    PHFUNC_ERR_FATAL       /* the handler is in a fatal state, the
                              test must be restarted from the
                              beginning */,
    PHFUNC_ERR_JAM         /* the handler is in a jammed state */,
    PHFUNC_ERR_LOT_START   /* not really an error, the handler has signeled start of lot */,
    PHFUNC_ERR_LOT_DONE    /* not really an error, the handler has signeled end of lot */,
    PHFUNC_ERR_DEVICE_START /* not really an error, the handler has signeled device start */,
    PHFUNC_ERR_FIRST_RETEST /* Not really an error, the handler (TechWing) is in first auto retest */,
    PHFUNC_ERR_SECOND_RETEST /* Not really an error, the handler (TechWing) is in second auto retest
                                for TechWing, the 'second" means last times of retest */,
    PHFUNC_ERR_RETEST_DONE /* Not really an error, the handler (TechWing) is in retest done */ ,
    PHFUNC_ERR_HAVE_DEAL  /* have sent msg or received msg */
    
} phFuncError_t;

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
 * created with phFuncInit(3)). It is specific for the implemented
 * driver plugin library. It does not require a plugin ID parameter as
 * all other functions. The task of this function is to return a flag
 * mask which summarizes the availability of plugin functions. The
 * result is a binary ORed mask of elements of the
 * phFuncAvailability_t enumeration. Future plugins may increase the
 * number of provided functions but always must keep the existing
 * enumerator entries unchanged.
 *
 * Before calling a plugin function (e.g. phFuncReprobe(3)), the
 * calling framework must check whether the plugin function is
 * implemented or not. E.g. 
 *
 * if (phFuncAvailable() & PHFUNC_AV_REPROBE) phFuncReprobe(...);
 *
 * Note: To use this function, call phPlugFuncAvailable()
 *
 * Returns: The availability mask
 *
 ***************************************************************************/
typedef phFuncAvailability_t (phFuncAvailable_t)(void);

#ifdef _PH_HFUNC_INTERNAL_
phFuncAvailable_t phFuncAvailable;
#endif
phFuncAvailable_t phPlugFuncAvailable;

/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Calling this function creates a new handler specific plugin, which
 * will be use for the direct handler interaction. The plugin will use
 * the passed <communication> (which is already active and which was
 * opened by the surrounding framework) for equipment
 * communication. The <logger> will be used for debug and error log of 
 * plugin calls. The <estate> is used to log and change the current
 * handler state in a general manner. In principle only the handler
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
 * communication test function phPlugFuncCommTest() to check whether
 * it can talk to the prober and the reconfigure function
 * phPlugFuncReconfigure() to perform all initial (or updating)
 * prober initialization steps.
 *
 * Note: To use this function, call phPlugPFuncInit()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncInit_t)(
    phFuncId_t *driverID     /* the resulting driver plugin ID */,
    phTcomId_t tester        /* the tester/tcom session to be used */,
    phComId_t communication  /* the valid communication link to the HW
			        interface used by the handler */,
    phLogId_t logger         /* the data logger to be used */,
    phConfId_t configuration /* the configuration manager */,
    phEstateId_t estate      /* the driver state */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncInit_t phFuncInit;
#endif
phFuncInit_t phPlugFuncInit;

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
 * Note: To use this function, call phPlugFuncReconfigure()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncReconfigure_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncReconfigure_t phFuncReconfigure;
#endif
phFuncReconfigure_t phPlugFuncReconfigure;

/*****************************************************************************
 *
 * reset the handler
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function may be used to reset the handler. The reset mechanism
 * and the reset actions are handler specific. The function could be
 * used during some error handling procedures or after recovery of a
 * lost connection.
 *
 * Note: This function is only available, if the HW interface of the
 * handler supports it. 
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncReset()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncReset_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncReset_t phFuncReset;
#endif
phFuncReset_t phPlugFuncReset;

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
 * name or family of handlers it is used for and a revision number of
 * revision date. The ID string will be set in an internal storage
 * area. The parameter <*driverIdString> will point to this area after
 * completion of the call.
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Note: To use this function, call phPlugFuncDriverID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncDriverID_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncDriverID_t phFuncDriverID;
#endif
phFuncDriverID_t phPlugFuncDriverID;

/*****************************************************************************
 *
 * return the handler identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function retrieves the identification string from the
 * connected handler. A typical identification string should include
 * the name and correct type of the handler and its internal firmware
 * revision.
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the handler supports it. 
 * If it is not available, the driver
 * should guess a reasonable ID string, since it knows, to which kind
 * of handler it is talking (taken from the configuration).
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncEquipID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncEquipID_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncEquipID_t phFuncEquipID;
#endif
phFuncEquipID_t phPlugFuncEquipID;

/*kaw start Oct 14 2004 */
/* Add Delta Orion commands - kaw */
/*****************************************************************************
 *
 * return the handler strip index identification string
 *
 * Authors: Michael Vogt, Ken Ward
 *
 * Description:
 * 
 * This function retrieves the strip index identification string from the
 * connected handler. A typical identification string should include
 * ??
 * 
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the handler supports it. 
 * If it is not available, the driver
 * should guess a reasonable ID string, since it knows, to which kind
 * of handler it is talking (taken from the configuration).
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncStripIndexID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncStripIndexID_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncStripIndexID_t phFuncStripIndexID;
#endif
phFuncStripIndexID_t phPlugFuncStripIndexID;

/*****************************************************************************
 *
 * return the strip material identification string
 *
 * Authors: Michael Vogt, Ken Ward
 *
 * Description:
 * 
 * This function retrieves the strip material identification string from the
 * connected handler. A typical identification string should include
 * ??
 * 
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the handler supports it. 
 * If it is not available, the driver
 * should guess a reasonable ID string, since it knows, to which kind
 * of handler it is talking (taken from the configuration).
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncStripMaterialID()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncStripMaterialID_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncStripMaterialID_t phFuncStripMaterialID;
#endif
phFuncStripMaterialID_t phPlugFuncStripMaterialID;

/*kaw end Oct 14 2004 */

/*kaw start Feb 07 2005 */
/* Add Mirae commands - kaw */
/*****************************************************************************
 *
 * Get status information from handler
 *
 * Authors: Michael Vogt, Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          07 Feb 2005, Ken Ward - added commands for Mirae
 *
 * Description: 
 *
 * Implements the following Mirae commands:
 *   VERSION?
 *   NAME?
 *   TESTSET?
 *   SETTEMP?
 *   MEASTEMP [1 - 9] ?
 *   STATUS?
 *   JAM?
 *   JAMCODE?
 *   JAMQUE?
 *   SETLAMP?
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the handler supports it. 
 * If it is not available, the driver
 * should guess a reasonable ID string, since it knows, to which kind
 * of handler it is talking (taken from the configuration).
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncGetStatus()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncGetStatus_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *commandString      /* pointer to string containing status to Get */
                             /* and parameters                             */,
    char **responseString    /* pointer to the response string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncGetStatus_t phFuncGetStatus;
#endif
phFuncGetStatus_t phPlugFuncGetStatus;

/*****************************************************************************
 *
 * Set status information in the handler
 *
 * Authors: Michael Vogt, Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          07 Feb 2005, Ken Ward - added commands for Mirae
 *
 * Description: 
 *
 * Implements the following Mirae commands:
 *   LOADER
 *   START
 *   STOP
 *   PLUNGER [0 or 1]
 *   SETNAME [string - 1 to 12 chars]
 *
 * Note: This functionality is only fully available, if the HW
 * interface and the protocol of the handler supports it. 
 * If it is not available, the driver
 * should guess a reasonable ID string, since it knows, to which kind
 * of handler it is talking (taken from the configuration).
 *
 * Warning: Never perform any memory management operations like
 * free(3C) on the returned pointer, nor overwrite the contents of the
 * data area.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncSetStatus()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncSetStatus_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *commandString      /* pointer to string containing status to set */
                             /* and parameters                             */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncSetStatus_t phFuncSetStatus;
#endif
phFuncSetStatus_t phPlugFuncSetStatus;


/*****************************************************************************
 *
 * Wait for lot start signal
 *
 * Authors: Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          23 Feb 2005, Ken Ward added function
 *
 * Description: 
 * Waits for a Lot Start SRQ from the handler.
 *
 * If parm == NOTIMEOUT, will not return until it gets an SRQ, 
 *   i.e. will not return PHFUNC_ERR_WAITING
 *
 * Otherwise: (example SRQs for Mirae MR5800)
 *
 *            if ( srq == 0x46 ) // 70 decimal = lot start
 *            {
 *                retVal = PHFUNC_ERR_LOT_START;
 *            }
 *            else if ( srq == 0x47 ) // 71 decimal = Device ready to test
 *                retVal = PHFUNC_ERR_DEVICE_START;
 *            }
 *            else if ( srq == 0x3d ) // 61 decimal
 *            {
 *                retVal = PHFUNC_ERR_JAM;
 *            }
 *            else if ( srq == 0x48 ) // 72 decimal = Lot End
 *            {
 *                retVal = PHFUNC_ERR_LOT_DONE;
 *            }
 *            else if ( srq == 0x0 ) // No SRQ received, timed out
 *            {
 *                retVal = PHFUNC_ERR_WAITING;
 *            }
 *            else
 *            {
 *                retVal = PHFUNC_ERR_ANSWER;
 *            }
 ***************************************************************************/
typedef phFuncError_t (phFuncLotStart_t)(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                /* if == NOTIMEOUT, then will not return until gets an SRQ */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncLotStart_t phFuncLotStart;
#endif
phFuncLotStart_t phPlugFuncLotStart;



/*****************************************************************************
 *
 * Clean up after lot done
 *
 * Authors: Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          23 Feb 2005, Ken Ward added function
 *
 * Description: 
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncLotDone_t)(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncLotDone_t phFuncLotDone;
#endif
phFuncLotDone_t phPlugFuncLotDone;

/* End of Add Mirae commands - kaw */

/*****************************************************************************
 *
 * Wait for strip start signal
 *
 * Authors: Magco Li
 *
 * History: 24 Oct 2014, magco li added function
 *
 * Description: Waits for a strip Start SRQ from the handler.
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncStripStart_t)(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                /* if == NOTIMEOUT, then will not return until gets an SRQ */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncStripStart_t phFuncStripStart;
#endif
phFuncStripStart_t phPlugFuncStripStart;

/*****************************************************************************
 *
 * Clean up after strip done
 *
 * Authors: magco li
 *
 * History: 24 Oct 2014, magco li added function
 *
 * Description: waits for a strip done
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncStripDone_t)(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncStripDone_t phFuncStripDone;
#endif
phFuncStripDone_t phPlugFuncStripDone;

/*****************************************************************************
 *
 * Wait for test start signal
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This is the main handling function to initiate the connection of a
 * new (group of) device(s). This function must be implemented for
 * every driver plugin. Based on the configuration the driver asks the
 * handler to apply devices to all (or a subset) of possible
 * sites. The handler notifies, which sites are really used and which
 * sites are empty. The site population is then returned via the
 * driver state controller (see phEstateAGetSiteInfo(3) and
 * phEstateASetSiteInfo(3)).
 *
 * Timeout:
 *
 * The configuration holds an entry for the 'waiting for parts
 * heartbeat' timeout. This timeout is usually set to a value of 1 to
 * 5 seconds. This function returns with a timeout error as soon as
 * the defined time has elapsed and no devices were applied. It is the
 * responsibility of the calling function (the framework) to decide,
 * whether the call to this function should be repeated (and how often
 * it should be repeated) until the global 'waiting for parts timeout'
 * time is reached (usually set to a value of 30 to 60 seconds). The
 * heartbeat timeout may be used outside of this function to check
 * whether some user interaction took place at some other place of
 * SmarTest or to call special 'waiting for parts' hook functions.
 *
 * Note: To use this function, call phPlugFuncGetStart()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncGetStart_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncGetStart_t phFuncGetStart;
#endif
phFuncGetStart_t phPlugFuncGetStart;

/*****************************************************************************
 *
 * Bin tested devices
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is the main handling function, that takes devices out
 * of the tester sockets and sorts them into the handlers output
 * bins. This function must be implemented for every driver
 * plugin. All decisions concerning the bins which ae used must be
 * done by the calling framework (i.e. asking SmarTest for the test
 * results, remaping bin tables, taking care of untested devices,
 * etc.). 
 *
 * The driver receives an array of bin numbers, which has a valid
 * bining information for each site that was originally populated
 * during the phFuncGetStart(3) function call. The bin numbers count
 * from 0 to the maximum number of available bins at the handler. The
 * numbers 0 to max-1 refer to the bin names definition from the
 * configuration. For user convenience, error messages and other
 * messages sent to the user should always map the bin index to the
 * defined handler bin name. The bin index will only be used within
 * the driver framework for efficient SmarTest bin to handler bin
 * mapping.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncBinDevice()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncBinDevice_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteBinMap      /* */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncBinDevice_t phFuncBinDevice;
#endif
phFuncBinDevice_t phPlugFuncBinDevice;

/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Calling this function asks the handler to reprobe the currently
 * applied devices. This operation may be neccessary if the continuity
 * test for some devices fail.
 *
 * Note: This function is only available, if the HW interface of the
 * handler supports it. 
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncReprobe()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncReprobe_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncReprobe_t phFuncReprobe;
#endif
phFuncReprobe_t phPlugFuncReprobe;

/*****************************************************************************
 *
 * send a command string to the handler
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is only available for message based handlers. It
 * allows to send an arbitrary command string to the handler, using
 * the current communication session. It is the responsibility of the
 * calling function to ensure that the command string is of some
 * meaning to the handler. Also, if this function is called, the
 * impact to other parts of the framework must be considered
 * (e.g. changing of driver state, active configuration, etc.)
 *
 * Timeout:
 * The usual timeout for sending commands applies to this function.
 *
 * Note: To use this function, call phPlugFuncSendCommand()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncSendCommand_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *command            /* command string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncSendCommand_t phFuncSendCommand;
#endif
phFuncSendCommand_t phPlugFuncSendCommand;

/*****************************************************************************
 *
 * send a query to the handler
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is only available for message based handlers. It
 * allows to send an arbitrary query string to the handler and to
 * receive an answer to this query, using the current communication
 * session. It is the responsibility of the calling function to ensure
 * that the query is of some meaning to the handler. Also, if
 * this function is called, the impact to other parts of the framework
 * must be considered (e.g. changing of driver state, active
 * configuration, etc.).
 *
 * Warning: The answer of the query is placed into some internal
 * communicaiton buffer. The calling function should never perform any
 * memory management operations like free(3C) on the returned pointer,
 * nor overwrite the contents of the message area.
 *
 * Timeout:
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncSendQuery()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncSendQuery_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *query              /* query string */,
    char **answer            /* pointer to answer string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncSendQuery_t phFuncSendQuery;
#endif
phFuncSendQuery_t phPlugFuncSendQuery;

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
 * diagnostic of the driver and the diagnostic of the handler will be
 * returned. Handler diagnostic (and or error messages) is only
 * available, if a message based handler is used.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncDiag()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncDiag_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **diag              /* pointer to handlers diagnostic output */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncDiag_t phFuncDiag;
#endif
phFuncDiag_t phPlugFuncDiag;

/*****************************************************************************
 *
 * reprobe or bin devices
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is a combination of reprobe and binning function (see
 * phPlugFuncBinDevice() and phPlugFuncReprobe(). All current devices
 * either receive a reprobe command or binning data. There is need to
 * have this function because some handlers (e.g. Delta Design
 * handlers) need to receive a result for each current device before a
 * reprobe action can be performed.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncReprobe()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncBinReprobe_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap      /* valid binning data for each site where
                                the above reprobe flag is not set */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncBinReprobe_t phFuncBinReprobe;
#endif
phFuncBinReprobe_t phPlugFuncBinReprobe;

/*****************************************************************************
 *
 * Notify SmarTest pause to handler plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is ment to inform the plugin about SmarTest entering
 * the pause state (pause flag is set).  It's the responsibility of
 * the plugin to decide whether to put the handler into pause or not,
 * based on the configuration value of
 * stop_handler_on_smartest_pause. If it does, it must also change the
 * equipment specific state information. The behaviour of this
 * function and the function phFuncSTUnpaused(3) should be symmetric.
 *
 * This function will also be called, if the SmarTest pause was
 * initiated by a previous handler stop. In this case the handler plugin
 * knows, that the handler is already paused.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncSTPaused()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncSTPaused_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncSTPaused_t phFuncSTPaused;
#endif
phFuncSTPaused_t phPlugFuncSTPaused;


/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is ment to inform the plugin about SmarTest leaving
 * the pause state (pause flag is reset). It's the responsibility of the
 * plugin to decide whether to put the handler out of pause or not,
 * based on the configuration value of
 * stop_handler_on_smartest_pause. If it does, it must also change the
 * equipment specific state information. The behaviour of this
 * function and the function phFuncSTPaused(3) should be symmetric.
 *
 * This function will also be called, if the SmarTest un-pause was
 * initiated by a previous handler unpause. In this case the handler plugin
 * knows, that the handler is already unpaused.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncUnpaused()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncSTUnpaused_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncSTUnpaused_t phFuncSTUnpaused;
#endif
phFuncSTUnpaused_t phPlugFuncSTUnpaused;


/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Calling phFuncStart(3), phFuncBin(3), phFuncReprobe(3), or
 * phFuncBinReprobe(3) may result in a PHFUNC_ERR_WAITING return code,
 * indicating that the requested operation has not been completed
 * yet. The Framework usually repeats the call until the return code
 * is different from PHFUNC_ERR_WAITING.
 * 
 * However, based on global timeout definitions, the operator may
 * abort the operation, if there is no chance to fullfill it (e.g. no
 * more devices to test). In this case the framework will call this
 * function phFuncStatus(3) using the PHFUNC_SR_ABORT action to
 * indicate the plugin, that the last operation should be finally
 * aborted. The plugin may use this information to clear or reset any
 * internal state. It will than be called with the original request
 * and must return with error code PHFUNC_ERR_ABORTED.
 *
 * In the case some external framework component was able to solve the
 * current problem (waiting , etc) it should notify the plugin about
 * this fact to, using the PHFUNC_SR_HANDLED action.
 *
 * If some other external framework component (e.g. the event
 * handling) needs to know the last operation requested to the plugin
 * caused a waiting state, it may use the PHFUNC_SR_QUERY action.
 *
 * Timeout:
 *
 * In case of message based handlers, the usual timeout for sending
 * commands applies to this function. 
 *
 * Note: To use this function, call phPlugFuncStatus()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncStatus_t)(
    phFuncId_t handlerID                /* driver plugin ID */,
    phFuncStatRequest_t action          /* the current status action
                                           to perform */,
    phFuncAvailability_t *lastCall      /* the last call to the
                                           plugin, not counting calls
                                           to this function */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncStatus_t phFuncStatus;
#endif
phFuncStatus_t phPlugFuncStatus;


/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function may be called by the framework, to ask the plugin to
 * look out for any unsolicited events, e.g. the handler was put into
 * pause mode by the operator or was put back into running mode. The
 * plugin checks for any events stored in the communication layer and
 * changes the equipment specific state accordingly.
 *
 * Note: To use this function, call phPlugUpdateState()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncUpdateState_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncUpdateState_t phFuncUpdateState;
#endif
phFuncUpdateState_t phPlugFuncUpdateState;


/*****************************************************************************
 *
 * communication test
 *
 * Authors: Michael Vogt
 *          Chris Joyce
 *
 * Description:
 *
 * This function may be used to test the communication between the handler
 * and the tester for message based handlers.
 *
 * The plugin must first clear the event and message queues so no
 * events or messages are pending.  The plugin must try to send an ID
 * request to the handler the actual format of the request being plugin
 * specific.  If the sending of the ID fails the error code
 * PHFUNC_ERR_TIMEOUT should be returned and *TestPassed set to
 * FALSE.  If the sending passes the plugin should wait the usual
 * heartbeat timeout.  If no answer is returned a PHFUNC_ERR_WAITING
 * waiting should be returned and *TestPassed set to FALSE.  If any
 * kind of answer is returned then *TestPassed should be set to TRUE
 * and the error code PHFUNC_ERR_OK returned.
 *
 * The expected model type should NOT be check for against known
 * configuration values.
 *
 * Note: To use this function, call phPlugFuncCommTest()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncCommTest_t)(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncCommTest_t phFuncCommTest;
#endif
phFuncCommTest_t phPlugFuncCommTest;


/*****************************************************************************
 *
 * Send setup and action command to handler by GPIB
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * This fucntion is used to send handler setup and action command to
 * setup handler initial parameters and control handler.
 *
 * Note: This function is only available if the HW interface and GPIB
 *       command supports it.
 *
 *       Never free the memory of retuned pointer, or overwrite the
 *       content of the data area.
 *
 *
 *
 * Note: To use this function, call phPlugFuncExecGpibCmd()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncExecGpibCmd_t)(
    phFuncId_t handlerID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncExecGpibCmd_t phFuncExecGpibCmd;
#endif
phFuncExecGpibCmd_t phPlugFuncExecGpibCmd;


/*****************************************************************************
 *
 * Send query command to handler by GPIB
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * This fucntion is used to send handler query command to get handler parameters
 * during handler driver runtime.
 *
 * Note: This function is only available if the HW interface and GPIB
 *       command supports it.
 *
 *       Never free the memory of retuned pointer, or overwrite the
 *       content of the data area.
 *
 *
 *
 * Note: To use this function, call phPlugFuncExecGpibQuery()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncExecGpibQuery_t)(
    phFuncId_t handlerID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncExecGpibQuery_t phFuncExecGpibQuery;
#endif
phFuncExecGpibQuery_t phPlugFuncExecGpibQuery;


/*****************************************************************************
 *
 * get a SRQ status byte
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * This fucntion is used to get a SRQ status byte
 *
 * Note: This function is only available if the HW interface and GPIB
 *       command supports it.
 *
 *       Never free the memory of retuned pointer, or overwrite the
 *       content of the data area.
 *
 *
 *
 * Note: To use this function, call phPlugFuncGetSrqStatusByte()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncGetSrqStatusByte_t)(
    phFuncId_t handlerID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncGetSrqStatusByte_t phFuncGetSrqStatusByte;
#endif
phFuncGetSrqStatusByte_t phPlugFuncGetSrqStatusByte;


/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is the counterpart to the phFuncInit(3) function. It
 * closes down the current driver session, clears and frees all
 * internal memory. The handlerID becomes invalid after this call.
 *
 * Note: To use this function, call phPlugFuncDestroy()
 *
 * Returns: error code
 *
 ***************************************************************************/
typedef phFuncError_t (phFuncDestroy_t)(
    phFuncId_t handlerID     /* driver plugin ID */
);

#ifdef _PH_HFUNC_INTERNAL_
phFuncDestroy_t phFuncDestroy;
#endif
phFuncDestroy_t phPlugFuncDestroy;

#ifdef __cplusplus
}
#endif

#endif /* ! _PH_HFUNC_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
