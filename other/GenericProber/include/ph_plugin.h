/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
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
 *            Jiawei Lin, R&D Shanghai, CR27092 & CR25172
 *            Garry Xie,R&D Shanghai, CR28427
 *            Fabarca & Kun Xiao, CR21589
 *            Dangln Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
 *            12 Nov 1999, Michael Vogt, adapted for prober framework
 *            August 2005, Jiawei Lin, CR27092 & CR25172
 *              Declare the plugin "phPlugPFuncGetStatus", which retrieve
 *              information/parameter from Prober, like WCR.
 *            February 2006, Garry Xie , CR28427
 *              Declare the plugin "phPlugPFuncSetStatus", which set
 *              information/parameter to Prober, like Alarm_buzzer.
 *            July 2006, fabarca & Kun Xiao, CR21589
 *              Declare the plugin "phPlugFuncContacttest", which performs 
 *              contact test to get z contact height automatically.
 *            Nov 2008, CR41221 & CR42599
 *              Declare the plugin "phPlugPFuncExecGpibCmd", which is
 *              used to send prober setup and action command.
 *              Decleare the plugin "phPlugPFuncExecGpibQuery", which is
 *              used to send prober query command.
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

#include "ph_pfunc.h"
#include "ph_phook.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

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
   in ph_pfunc.h */
struct driverPlugin {
    phPFuncAvailability_t found;
    phPFuncAvailability_t reported;
    phPFuncAvailability_t used;

    phPFuncAvailable_t   *available;
    phPFuncInit_t        *init;
    phPFuncReconfigure_t *reconfigure;
    phPFuncReset_t       *reset;
    phPFuncDriverID_t    *driverID;
    phPFuncEquipID_t     *equipID;
    phPFuncLoadCass_t    *loadCass;
    phPFuncUnloadCass_t  *unloadCass;
    phPFuncLoadWafer_t   *loadWafer;
    phPFuncUnloadWafer_t *unloadWafer;
    phPFuncGetDie_t      *getDie;
    phPFuncBinDie_t      *binDie;
    phPFuncGetSubDie_t   *getSubDie;
    phPFuncBinSubDie_t   *binSubDie;
    phPFuncDieList_t     *dieList;
    phPFuncSubDieList_t  *subDieList;
    phPFuncReprobe_t     *reprobe;
    phPFuncCleanProbe_t  *cleanProbe;
    phPFuncPMI_t         *PMI;
    phPFuncSTPaused_t    *STPaused;
    phPFuncSTUnpaused_t  *STUnpaused;
    phPFuncTestCommand_t *testCommand;
    phPFuncDiag_t        *diag;
    phPFuncStatus_t      *status;
    phPFuncUpdateState_t *update;
    phPFuncCassID_t      *cassID;
    phPFuncWafID_t       *wafID;
    phPFuncProbeID_t     *probeID;
    phPFuncLotID_t       *lotID;
    phPFuncStartLot_t    *startLot;
    phPFuncEndLot_t      *endLot;
    phPFuncCommTest_t    *commTest;
    phPFuncDestroy_t     *destroy;
    phPFuncSimResults_t  *simResults;
    phPFuncGetStatus_t   *getStatus;    /*CR27092&CR25172*/
    phPFuncSetStatus_t   *setStatus;    /*CR28427*/
    phPFuncContacttest_t *contacttest;  /*CR21589*/
    phPFuncExecGpibCmd_t *execGpibCmd;
    phPFuncExecGpibQuery_t *execGpibQuery;
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


/* plugin functions, served by this module. Refer to ph_pfunc.h for
   detailed description */

phPFuncAvailable_t   phPlugPFuncAvailable;
phPFuncInit_t        phPlugPFuncInit;
phPFuncReconfigure_t phPlugPFuncReconfigure;
phPFuncReset_t       phPlugPFuncReset;
phPFuncDriverID_t    phPlugPFuncDriverID;
phPFuncEquipID_t     phPlugPFuncEquipID;
phPFuncLoadCass_t    phPlugPFuncLoadCass;
phPFuncUnloadCass_t  phPlugPFuncUnloadCass;
phPFuncLoadWafer_t   phPlugPFuncLoadWafer;
phPFuncUnloadWafer_t phPlugPFuncUnloadWafer;
phPFuncGetDie_t      phPlugPFuncGetDie;
phPFuncBinDie_t      phPlugPFuncBinDie;
phPFuncGetSubDie_t   phPlugPFuncGetSubDie;
phPFuncBinSubDie_t   phPlugPFuncBinSubDie;
phPFuncDieList_t     phPlugPFuncDieList;
phPFuncSubDieList_t  phPlugPFuncSubDieList;
phPFuncReprobe_t     phPlugPFuncReprobe;
phPFuncCleanProbe_t  phPlugPFuncCleanProbe;
phPFuncPMI_t         phPlugPFuncPMI;
phPFuncSTPaused_t    phPlugPFuncSTPaused;
phPFuncSTUnpaused_t  phPlugPFuncSTUnpaused;
phPFuncTestCommand_t phPlugPFuncTestCommand;
phPFuncDiag_t        phPlugPFuncDiag;
phPFuncStatus_t      phPlugPFuncStatus;
phPFuncUpdateState_t phPlugPFuncUpdateState;
phPFuncCassID_t      phPlugPFuncCassID;
phPFuncWafID_t       phPlugPFuncWafID;
phPFuncProbeID_t     phPlugPFuncProbeID;
phPFuncLotID_t       phPlugPFuncLotID;
phPFuncStartLot_t    phPlugPFuncStartLot;
phPFuncEndLot_t      phPlugPFuncEndLot;
phPFuncCommTest_t    phPlugPFuncCommTest;
phPFuncDestroy_t     phPlugPFuncDestroy;
phPFuncSimResults_t  phPlugPFuncSimResults;
phPFuncGetStatus_t   phPlugPFuncGetStatus;
phPFuncSetStatus_t   phPlugPFuncSetStatus;
phPFuncContacttest_t phPlugPFuncContacttest;
phPFuncExecGpibCmd_t phPlugPFuncExecGpibCmd;
phPFuncExecGpibQuery_t phPlugPFuncExecGpibQuery;
/* plugin functions, served by this module. Refer to ph_hhook.h for
   detailed description */

phHookAvailable_t    phPlugHookAvailable;
phHookInit_t         phPlugHookInit;
phHookActionStart_t  phPlugHookActionStart;
phHookActionDone_t   phPlugHookActionDone;
phHookProblem_t      phPlugHookProblem;
phHookWaiting_t      phPlugHookWaiting;
phHookPopup_t        phPlugHookPopup;


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
    phPFuncAvailability_t *available    /* returns the set of
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

#endif /* ! _PH_PLUGIN_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
