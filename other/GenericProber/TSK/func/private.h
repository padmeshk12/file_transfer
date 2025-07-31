/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.h
 * CREATED  : 26 Jan 2000
 *
 * CONTENTS : Private prober specific implementation
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR27092 & CR25172
 *            Garry Xie, R&D Shanghai, CR28427
 *            Fabarca & Kun Xiao, R&D Shanghai, CR21589
 *            Danglin Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            August 2005, Jiawei Lin, CR27092 & CR25172
 *              declare the "privateGetStatus". "getStatusXXX" series functions
 *              are used to retrieve the information or parameter from Prober.
 *              Such information is like WCR, Lot Number, Wafer Number, Cassette
 *              Status, Chuck temperature and so on.
 *            February 2006, Garry Xie , CR28427
 *              declare the "privateSetStatus" ."setSatusXXX" series functions
 *              are used to set parameters to the prober, like "Alarm_buzzer xxx".
 *            July 2006, Fabarca & Kun Xiao, CR21589
 *               declare the "privateContactTest". This function performs contact test 
 *               to get z contact height automatically.
 *           Nov 2008, CR41221 & CR42599
 *               declare the "privateExecGpibCmd". this function performs to send
 *               prober setup and action command by gpib.
 *               declare the "privateExecGpibQuery". this function performs to send
 *               prober query command by gpib.
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
/* #define LDCASS_IMPLEMENTED */
/* #define UNLCASS_IMPLEMENTED */
#define LDWAFER_IMPLEMENTED
#define UNLWAFER_IMPLEMENTED
#define GETDIE_IMPLEMENTED
#define BINDIE_IMPLEMENTED
/* #define GETSUBDIE_IMPLEMENTED */
/* #define BINSUBDIE_IMPLEMENTED */
/* #define DIELIST_IMPLEMENTED */
/* #define SUBDIELIST_IMPLEMENTED */
#define REPROBE_IMPLEMENTED
#define CLEAN_IMPLEMENTED
/* #define PMI_IMPLEMENTED */
#define PAUSE_IMPLEMENTED
#define UNPAUSE_IMPLEMENTED
/* #define TEST_IMPLEMENTED */
#define DIAG_IMPLEMENTED
#define STATUS_IMPLEMENTED
/* #define UPDATE_IMPLEMENTED */
/* #define CASSID_IMPLEMENTED */
#define WAFID_IMPLEMENTED
/* #define PROBEID_IMPLEMENTED */
#define LOTID_IMPLEMENTED
#define STARTLOT_IMPLEMENTED
#define ENDLOT_IMPLEMENTED
#define COMMTEST_IMPLEMENTED
#define GETSTATUS_IMPLEMENTED
#define SETSTATUS_IMPLEMENTED
#define SETSTATUSFORAUTOZ_IMPLEMENTED
#define DESTROY_IMPLEMENTED

/*Begin CR21589, Fabarca & Kun Xiao, July 2006*/
#define CONTACTTEST_IMPLEMENTED 

/*--- typedefs --------------------------------------------------------------*/

/* make your plugin private changes here */

struct pluginPrivate {
    int stIsMaster;                     /* true, if driver controls stepping */
    long sendTimeout;                   /* in usec to send any GPIB command */
    long receiveTimeout;                /* in usec to receive any answer */
    int chuckUnderFirstDie;            
    int b_is_waferid;                   /* set, if the b command
                                           retrieves the wafer ID and
                                           the W command starts needle
                                           cleaning, 0 if reverse (default) */
    int waferid_rm_first_char;          /* true if user wants to remove the 
                                           first character from the returned
                                           wafer ID string on TSK probers that
                                           have a firmware defect which inserts 
                                           the character at the front of the
                                           wafer ID string.  Default = 0. */
    int max_bins;                       /* either 64, 256,10000 or 1000000,Default = 64 */
    int unexpectedSRQResp;              /* how to do when received unexpected SRQ
                                           during waiting for lot start.
                                           0: show confirmation dialog
                                           2: abort
                                           3: continue                    */
    int waferCountReceived;             /* set to 1 if SRQ 0x50 is received. this
                                           SRQ is always output when wafer is unloaded.
                                           "M" --> 0x50 --> 0x46 (manual unlaod/next wafer)
                                           is different from "M" --> 0x46 (sequence back/re-alignment).*/
    int waferLoadedImmediatelyAfterBinning;         /* a flag indicating if the wafer
                                                     is immediately loaded are pressed after
                                                     the current touchdown is binned out (after
                                                     the "M" command)
                                                     0: not loaded immediately after binning
                                                     1: loaded immediately after binning */
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
 * Initialize prober specific plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateInit(
    phPFuncId_t driverID      /* the prepared driver plugin ID */
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReconfigure(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif


#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReset(
    phPFuncId_t proberID     /* driver plugin ID */
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDriverID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
);
#endif



#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the prober identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateEquipID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
);
#endif



#ifdef LDCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Load an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLoadCass(
    phPFuncId_t proberID       /* driver plugin ID */
);
#endif



#ifdef UNLCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Unload an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUnloadCass(
    phPFuncId_t proberID       /* driver plugin ID */
);
#endif



#ifdef LDWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Load a wafer to the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLoadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t source        /* where to load the wafer
                                           from. PHESTATE_WAFL_OUTCASS
                                           is not a valid option! */
);
#endif



#ifdef UNLWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Unload a wafer from the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUnloadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer
                                           to. PHESTATE_WAFL_INCASS is
                                           not valid option! */
);
#endif



#ifdef GETDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateGetDie(
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
#endif



#ifdef BINDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateBinDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL, if
                                sub-die probing is active */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis or
                                NULL, if sub-die probing is active */
);
#endif



#ifdef GETSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next sub-die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateGetSubDie(
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
#endif



#ifdef BINSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested sub-dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateBinSubDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL if working
                                with sub die level */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis */
);
#endif



#ifdef DIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDieList(
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
#endif



#ifdef SUBDIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send sub-die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSubDieList(
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
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReprobe(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif



#ifdef CLEAN_IMPLEMENTED
/*****************************************************************************
 *
 * Clean the probe needles
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCleanProbe(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif



#ifdef PMI_IMPLEMENTED
/*****************************************************************************
 *
 * Perform a probe mark inspection
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privatePMI(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif



#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSTPaused(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSTUnpaused(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif



#ifdef TEST_IMPLEMENTED
/*****************************************************************************
 *
 * send a command string to the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateTestCommand(
    phPFuncId_t proberID     /* driver plugin ID */,
    char *command            /* command string */
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDiag(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **diag              /* pointer to probers diagnostic output */
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateStatus(
    phPFuncId_t proberID                /* driver plugin ID */,
    phPFuncStatRequest_t action         /* the current status action
                                           to perform */,
    phPFuncAvailability_t *lastCall     /* the last call to the
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUpdateState(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif



#ifdef CASSID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current cassette ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCassID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **cassIdString      /* resulting pointer to cassette ID string */
);
#endif



#ifdef WAFID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current wafer ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateWafID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **wafIdString      /* resulting pointer to wafer ID string */
);
#endif



#ifdef PROBEID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current probe card ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateProbeID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **probeIdString     /* resulting pointer to probe card ID string */
);
#endif


#ifdef LOTID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current lot ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLotID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **lotIdString       /* resulting pointer to lot ID string */
);
#endif 


#ifdef STARTLOT_IMPLEMENTED
/*****************************************************************************
 *
 * Start a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateStartLot(
    phPFuncId_t proberID       /* driver plugin ID */
);
#endif



#ifdef ENDLOT_IMPLEMENTED
/*****************************************************************************
 *
 * End a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateEndLot(
    phPFuncId_t proberID       /* driver plugin ID */
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
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCommTest(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
);
#endif


#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Get Information/Parameter/Status from Prober
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *   Implements the following information request by CR27092&CR25172:
 *    (1)STDF WCR, 
 *    (2)Wafer Number, Lot Number, Chuck Temperatue, Name of 
 *       Prober setup, Cassette Status.
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateGetStatus(
    phPFuncId_t proberID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
    
);
#endif

#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Set parameter to the prober
 *
 * Authors: Garry Xie
 *
 * Description:
 *   set the following information to Prober request by CR28427
 *   Alarm_Buzzer,Double_Z_UP Enable,Double_Z_Up Disable
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSetStatus(
    phPFuncId_t proberID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */
    
);
#endif

#ifdef SETSTATUSFORAUTOZ_IMPLEMENTED
/*****************************************************************************
 *
 * Set parameter to the prober
 *
 * Authors: Jax Wu
 *
 * Description:
 *   set some commands for autoz 
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSetStatusForAutoZ(
    phPFuncId_t proberID        /* driver plugin ID */,
    const char *commandString   /* the string of command */,
    const char *paramString     /* the parameter for command string*/,
    char **responseString       /* output of response string */
    
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDestroy(
    phPFuncId_t proberID     /* driver plugin ID */
);
#endif

#ifdef CONTACTTEST_IMPLEMENTED
/*****************************************************************************
 *
 * contact test
 *
 * Authors: Fabrizio Arca - EDO,Kun Xiao Shangai
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateContacttest(
    phPFuncId_t proberID                       /* driver plugin ID */,
    phContacttestSetup_t contactestSetupId     /* contact test ID */
);
#endif

#endif /* ! _PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/

