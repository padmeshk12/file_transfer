/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_plugin.h
 * CREATED  : 14 Jul 1999
 *
 * CONTENTS : Plugin library management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
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

#ifndef _PH_PLUGIN_H_
#define _PH_PLUGIN_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_hfunc.h"
#include "ph_hhook.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

typedef enum {
    PHPLUG_ERR_OK = 0                   /* no error */,
    PHPLUG_ERR_LOAD                     /* the library could not be
					   loaded, an error message was 
					   printed to the logger */,
    PHPLUG_ERR_DIFFER                   /* not all expected symbols or
					   more of them were found
					   (the availability flags
					   differ from real found
					   symbols). The library still
					   works and the returned
					   availability flags have
					   been corrected */,
    PHPLUG_ERR_NEXIST                   /* the requested library does
					   not exist */
} phPlugError_t;

/* jump table for driver plugin, using the function types as defined
   in ph_hfunc.h */
struct driverPlugin {
    phFuncAvailability_t found;
    phFuncAvailability_t reported;
    phFuncAvailability_t used;

    phFuncAvailable_t    *available;
    phFuncInit_t         *init;
    phFuncReconfigure_t  *reconfigure;
    phFuncReset_t        *reset;
    phFuncDriverID_t     *driverID;
    phFuncEquipID_t      *equipID;
    phFuncGetStart_t     *getStart;
    phFuncBinDevice_t    *binDevice;
    phFuncReprobe_t      *reprobe;
    phFuncSendCommand_t  *sendCommand;
    phFuncSendQuery_t    *sendQuery;
    phFuncDiag_t         *diag;
    phFuncBinReprobe_t   *binReprobe;
    phFuncSTPaused_t     *STPaused;
    phFuncSTUnpaused_t   *STUnpaused;
    phFuncStatus_t       *status;
    phFuncUpdateState_t  *update;
    phFuncCommTest_t     *commTest;
    phFuncDestroy_t      *destroy;
    phFuncStripIndexID_t      *stripID;
    phFuncStripMaterialID_t   *materialID;
    phFuncGetStatus_t    *getStatus;
    phFuncSetStatus_t    *setStatus;
    phFuncLotStart_t     *lotStart;
    phFuncLotDone_t      *lotDone;
    phFuncStripStart_t     *stripStart; /*Oct 23 2014 magco li*/
    phFuncStripDone_t      *stripDone;  /*Oct 23 2014 magco li*/
    phFuncExecGpibCmd_t *execGpibCmd;
    phFuncExecGpibQuery_t *execGpibQuery;
    phFuncGetSrqStatusByte_t *getSrqStatusByte;
};

/* jump table for hook plugin, using the function types as defined
   in ph_hhook.h */
struct hookPlugin {
    phHookAvailability_t found;
    phHookAvailability_t reported;
    phHookAvailability_t used;

    phHookAvailable_t    *available;
    phHookInit_t         *init;
    phHookActionStart_t  *actionStart;
    phHookActionDone_t   *actionDone;
    phHookProblem_t      *problem;
    phHookWaiting_t      *waiting;
    phHookPopup_t        *popup;
};

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


/* plugin functions, served by this module. Refer to ph_hfunc.h for
   detailed description */

phFuncAvailable_t   phPlugFuncAvailable;
phFuncInit_t        phPlugFuncInit;
phFuncReconfigure_t phPlugFuncReconfigure;
phFuncReset_t       phPlugFuncReset;
phFuncDriverID_t    phPlugFuncDriverID;
phFuncEquipID_t     phPlugFuncEquipID;
phFuncGetStart_t    phPlugFuncGetStart;
phFuncBinDevice_t   phPlugFuncBinDevice;
phFuncReprobe_t     phPlugFuncReprobe;
phFuncSendCommand_t phPlugFuncSendCommand;
phFuncSendQuery_t   phPlugFuncSendQuery;
phFuncDiag_t        phPlugFuncDiag;
phFuncCommTest_t    phPlugFuncCommTest;
phFuncDestroy_t     phPlugFuncDestroy;
phFuncStripIndexID_t     phPlugFuncStripIndexID;
phFuncStripMaterialID_t     phPlugFuncStripMaterialID;
phFuncGetStatus_t    phPlugFuncGetStatus;
phFuncSetStatus_t    phPlugFuncSetStatus;
phFuncLotStart_t     phPlugFuncLotStart;
phFuncLotDone_t      phPlugFuncLotDone;
phFuncStripStart_t     phPlugFuncStripStart;/*Oct 23 2014 magco li*/
phFuncStripDone_t      phPlugFuncStripDone;/*Oct 23 2014 magco li*/
phFuncExecGpibCmd_t phPlugFuncExecGpibCmd;
phFuncExecGpibQuery_t phPlugFuncExecGpibQuery;
phFuncGetSrqStatusByte_t phPlugFuncGetSrqStatusByte;

/* plugin functions, served by this module. Refer to ph_hhook.h for
   detailed description */

phHookAvailable_t   phPlugHookAvailable;
phHookInit_t        phPlugHookInit;
phHookActionStart_t phPlugHookActionStart;
phHookActionDone_t  phPlugHookActionDone;
phHookProblem_t     phPlugHookProblem;
phHookWaiting_t     phPlugHookWaiting;
phHookPopup_t       phPlugHookPopup;


/*****************************************************************************
 *
 * Load a driver plugin library
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * A shared driver plugin library is loaded and checked for
 * consistency. The name of the library most not be given, only the
 * path were to look for the library. This function knowns the correct
 * naming scheme of the library. If the library can not be loaded, an
 * error message is logged and an error code is returned. If the
 * library does not provide all symbols as named by its function
 * phFuncAvailable(3), the correct availability is determined and
 * returned. The library can still be used in this case.
 *
 * Notes: <delete this, if not used>
 *
 * Returns: error code
 *
 ***************************************************************************/

phPlugError_t phPlugLoadDriver(
    phLogId_t            logger         /* report errors here */,
    char                 *path          /* directory path were to look
					   for driver library plugin */,
    phFuncAvailability_t *available     /* returns the set of
					   available driver plugin functions */
);

/*****************************************************************************
 *
 * Load a driver plugin library
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Similar as phPlugLoadDriver() but working on hook function plugins.
 *
 * Notes: not yet implemented
 *
 * Returns: error code
 *
 ***************************************************************************/
phPlugError_t phPlugLoadHook(
    phLogId_t            logger         /* report errors here */,
    char                 *path          /* directory path were to look
					   for hook library plugin */,
    phHookAvailability_t *available     /* returns the set of
					   available hook plugin functions */
);

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_PLUGIN_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
