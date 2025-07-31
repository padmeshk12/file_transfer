/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.h
 * CREATED  : 26 Jan 2000
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            09 Jun 2000, Michael Vogt, adapted from prober driver
 *            09 Jun 2000, Michael Vogt, adapted from Delta plug-in
 *            Dec 2006, Kun Xiao, CR33707
 *              support query commands for Multitest handler;
 *              introduce the stub function "privateGetStatusStub" as the
 *              universal interface to query all the kinds of information
 *              from Handler.
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

#ifndef _PRIVATE_H_
#define _PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/* to activate or deactivate plugin functions, remove or put comment
   signs around the following defines */

#define INIT_IMPLEMENTED
#define RECONFIGURE_IMPLEMENTED
/* #define RESET_IMPLEMENTED */
#define DRIVERID_IMPLEMENTED
#define EQUIPID_IMPLEMENTED
#define GETSTART_IMPLEMENTED
#define BINDEVICE_IMPLEMENTED
#define REPROBE_IMPLEMENTED
#define BINREPROBE_IMPLEMENTED
#define PAUSE_IMPLEMENTED
#define UNPAUSE_IMPLEMENTED
#define DIAG_IMPLEMENTED
#define STATUS_IMPLEMENTED
#define COMMTEST_IMPLEMENTED
#define GETSTATUS_IMPLEMENTED
#define SETSTATUS_IMPLEMENTED
#define LOTSTART_IMPLEMENTED
#define LOTDONE_IMPLEMENTED

/* #define UPDATE_IMPLEMENTED */
/* #define DESTROY_IMPLEMENTED */

/*--- typedefs --------------------------------------------------------------*/

/* make your plugin private changes here */

struct pluginPrivate {
    int             sites;
    int             siteUsed[PHESTATE_MAX_SITES];

    int             strictPolling;
    long            pollingInterval;
    int             deviceExpected[PHESTATE_MAX_SITES];
    int             devicePending[PHESTATE_MAX_SITES];
    int             oredDevicePending;

    int             stopped;

    int             masterSot;
    int             pollHandler;
    int             handShake;
    int             sitea;
    int             siteb;
    int             sitec;
    int             sited;
    char            bina[PHESTATE_MAX_SITES];
    char            binb[PHESTATE_MAX_SITES];
    char            binc[PHESTATE_MAX_SITES];
    char            bind[PHESTATE_MAX_SITES];
    char            eol[4];


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

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateInit(
    phFuncId_t handlerID      /* the prepared driver plugin ID */
);
#endif


#ifdef RECONFIGURE_IMPLEMENTED
/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReconfigure(
    phFuncId_t handlerID     /* driver plugin ID */
);
#endif


#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the handler
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReset(
    phFuncId_t handlerID     /* driver plugin ID */
);
#endif


#ifdef DRIVERID_IMPLEMENTED
/*****************************************************************************
 *
 * return the driver identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDriverID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
);
#endif



#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the handler identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateEquipID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
);
#endif



#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStart(
    phFuncId_t handlerID      /* driver plugin ID */
);
#endif



#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinDevice(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information */
);
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReprobe(
    phFuncId_t handlerID     /* driver plugin ID */
);
#endif



#ifdef BINREPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinReprobe(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap      /* valid binning data for each site where
                                the above reprobe flag is not set */
);
#endif



#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to handler plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSTPaused(
    phFuncId_t handlerID     /* driver plugin ID */
);
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSTUnpaused(
    phFuncId_t handlerID     /* driver plugin ID */
);
#endif



#ifdef DIAG_IMPLEMENTED
/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDiag(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **diag              /* pointer to handlers diagnostic output */
);
#endif



#ifdef STATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStatus(
    phFuncId_t handlerID                /* driver plugin ID */,
    phFuncStatRequest_t action          /* the current status action
                                           to perform */,
    phFuncAvailability_t *lastCall      /* the last call to the
                                           plugin, not counting calls
                                           to this function */
);
#endif



#ifdef UPDATE_IMPLEMENTED
/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateUpdateState(
    phFuncId_t handlerID     /* driver plugin ID */
);
#endif



#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication test
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateCommTest(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
);
#endif

#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The function for get status
 *
 * Authors: Kun Xiao
 *
 * Description:
 *  use this function to retrieve the information/parameter from handler
 *  the commandString is the key of query type, and paramString is its parmater.
 *  For example:
 *    for the call PROB_HND_CALL(ph_get_status masstemp zone1),
 *    the commandString is "masstemp", the paramString is "zone1"
 *
 ***************************************************************************/
phFuncError_t privateGetStatus(
    phFuncId_t handlerID,       /* driver plugin ID */
    const char *commandString,  /* the string of command, i.e. the key to
                                   get the information from Handler */
    const char *paramString,    /* the parameter for commandString */
    char **responseString       /* output of the response string */
);
#endif



#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Set status to handler
 *
 * Author: Roger Feng
 *
 * Description:
 *  Use this function to set/config the parameter of handler.
 *  The commandString is the name of command; paramString is command parameters.
 *  For example:
 *    for the call PROB_HND_CALL(ph_set_status soaktime 30),
 *    the commandString is "soaktime", the paramString is "30"
 *
 ***************************************************************************/
phFuncError_t privateSetStatus(
    phFuncId_t handlerID,       /* driver plugin ID */
    const char *commandString,  /* the string of command, i.e. the key to
                                   get the information from Handler */
    const char *paramString    /* the parameter for commandString */
);
#endif



#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDestroy(
    phFuncId_t handlerID     /* driver plugin ID */
);
#endif

#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot start from handler
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateLotStart(
    phFuncId_t handlerID,      /* driver plugin ID */
    int timeoutFlag
);
#endif

#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Cleanup after lot done
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateLotDone(
    phFuncId_t handlerID,      /* driver plugin ID */
    int timeoutFlag
);
#endif

#endif /* ! _PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
