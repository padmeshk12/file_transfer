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
 *            Danglin Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            26 Jan 2000, Chris Joyce added Electoglas specific software.
 *            August 2005, Jiawei Lin, CR27092 & CR25172
 *              declare the "privateGetStatus". "getStatusXXX" series functions
 *              are used to retrieve the information or parameter from Prober.
 *              The information is WCR.
 *            Nov 2008, CR41221 & CR42599
 *              declare the "privateExecGpibCmd". this function performs to send
 *              prober setup and action command by gpib.
 *              declare the "privateExecGpibQuery". this function performs to send
 *              prober query command by gpib.
 *
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
#define LDCASS_IMPLEMENTED
#define UNLCASS_IMPLEMENTED
#define LDWAFER_IMPLEMENTED
#define UNLWAFER_IMPLEMENTED
#define GETDIE_IMPLEMENTED
#define BINDIE_IMPLEMENTED
#define GETSUBDIE_IMPLEMENTED
#define BINSUBDIE_IMPLEMENTED
#define DIELIST_IMPLEMENTED
#define SUBDIELIST_IMPLEMENTED
#define REPROBE_IMPLEMENTED
#define CLEAN_IMPLEMENTED
/* #define PMI_IMPLEMENTED */
#define PAUSE_IMPLEMENTED
#define UNPAUSE_IMPLEMENTED
#define TEST_IMPLEMENTED
#define DIAG_IMPLEMENTED 
#define STATUS_IMPLEMENTED
/* #define UPDATE_IMPLEMENTED */
/* #define CASSID_IMPLEMENTED */
#define WAFID_IMPLEMENTED
/* #define PROBEID_IMPLEMENTED */
#define COMMTEST_IMPLEMENTED
#define GETSTATUS_IMPLEMENTED
#define DESTROY_IMPLEMENTED



#define EG_MAX_MESSAGE_SIZE    1024       /*  maximum message size for an
                                              Electroglas prober */

#define EG_MAX_DIAGNOSTIC_MSG   1024      /* maximum diagnostic message
                                             length */

#define PHKEY_EG_RECIPEFILE        "electroglas_recipe_file"

/*--- typedefs --------------------------------------------------------------*/


typedef enum
{
    STEPMODE_EXPLICIT = 0       /* stepping controlled by smartest  */,
    STEPMODE_AUTO               /* stepping controlled by prober    */,
    STEPMODE_LEARNLIST          /* stepping controlled by learnlist */

} stepMode_t;


typedef enum
{
    EG_IDLE = 0                 /* waiting to be told to either send message 
                                   or get a message */,
    EG_SEND_MSG_WAITING_TO_SEND /* an attempt to send a message has timedout
                                   still waiting to send message */,
    EG_GET_MSG_WAITING_SRQ      /* no message has been sent but waiting
                                   to receive a message for example
                                   waiting for a test start signal */,
    EG_GET_MSG_WAITING_MSG      /* no message has been sent but waiting
                                   to receive a message for example
                                   waiting for a test start signal */,
//    EG_SENT_MSG_WAITING_REPLY   /* a message has been sent waiting for 
//                                   reply */,
    EG_TRANSACTIONS_ERROR       /* error occured in last transaction */,
    EG_TRANSACTIONS_HANDLED     /* received a call from phPlugPFuncStatus()
                                   that the current transactions have some
                                   how been handled externally */,
    EG_TRANSACTIONS_ABORTED     /* received a call from phPlugPFuncStatus()
                                   to abort current set of transactions */,
    EG_TRANSACTIONS_RESET       /* received a call from phPlugPFuncReset()
                                   to reset current set of transactions */
} egTransactionState_t;

typedef enum
{
    EG_COMMANDER_SERIES = 0             /* EG4060, EG4080, EG4090, EG4200, EG5300 */,
    EG_PROBER_VISION_SERIES             /* EG2001, EG2010, EG3001, EG4085 */,
    UNKNOWN_FAMILY
} egProberFamilyType_t;


typedef enum
{                         /* Prober run code returned from ?R request  */
    EG_STATE_IDLE         /* = 0 system is idle     */,
    EG_STATE_RUNNING      /* > 0 system is running  */,
    EG_STATE_PAUSED       /* < 0 system is paused   */,
    EG_STATE_UNKNOWN      /* state not yet determined */
} egProberState_t;


typedef enum
{
    NORMAL_STATE                /* plugin is in normal running state */,
    INTO_PAUSE                  /* received message from prober during
                                   normal state going into paused state */,
    PAUSED_STATE                /* prober has been paused */,
    OUT_OF_PAUSE                /* received CO - continue after pause from
                                   prober going into normal state */

} egPluginState_t;


/* make your plugin private changes here */

struct pluginPrivate {
    stepMode_t               stepMode;

    egPluginState_t          pluginState;             /* plugin has gone into paused
                                                         state during transactions */

    egTransactionState_t     egTransactionState;      /* current state of plugin */ 

    phPFuncAvailability_t    currentTransactionCall;  /* current call which is
                                                         performing transactions */
    int                      performedTransactionCount; /* number of transactions
                                                         performed for all
                                                         currentTransactionCall */

    egTransactionState_t     egNormalTransactionState;      /* current state of plugin */ 

    phPFuncAvailability_t    currentNormalTransactionCall;  /* current call which is
                                                         performing transactions */
    int                      performedNormalTransactionCount; /* number of transactions
                                                         performed for all
                                                         currentTransactionCall */


    int                      currentTransactionCount; /* number of transactions
                                                         performed for this
                                                         currentTransactionCall */
    char                     egTransactionLog[EG_MAX_DIAGNOSTIC_MSG];    
                                                        /* string of state for log */
    char                     egDiagnosticString[EG_MAX_DIAGNOSTIC_MSG];    
                                                        /* string of state for log */
    char                     equipmentIDStr[EG_MAX_MESSAGE_SIZE];
    char                     recipeFileName[EG_MAX_MESSAGE_SIZE];

    enum proberModel         proberModel;  /* actual model as determined
                                              by sending an "ID" command */
    egProberFamilyType_t     proberFamily; /* actual family type as 
                                              determined from proberModel */

    egProberFamilyType_t     configProberFamily;  /* family type as determined
                                                     from model in the 
                                                     configuration file */
    /* 
       Note: the prober model as determined from the configuration file is kept 
       in the phPFuncStruct(enum proberModel model) in ph_pfunc_private.h
     */

    /* 
       Probe state variables returned from ?R query
     */
    int                      run_code;
    int                      run_subcode;
    int                      first_die_set;
    int                      reference_stored;
    int                      wafer_on_chuck;
    int                      wafer_profiled;
    int                      wafer_edge_profiled;
    int                      wafer_autoaligned;

    egProberState_t          proberState; /* state of prober as determined by
                                             examining Run Code returned from
                                             a ?R query */
    /* die list parameters */
    int dieListCount;
    long *dieListX;
    long *dieListY;

    /* subdie list parameters */
    int subdieListCount;
    long *subdieListX;
    long *subdieListY;

    BOOLEAN                  firstSubDie;  /* first subdie on die */
    BOOLEAN                  firstDie;     /* first die on wafer */
    BOOLEAN                  firstWafer;   /* first wafer in cassette */


    int                      currentMicroDieSiteNumber;
                                               /* current micro die site 
                                                  counter used to keep track 
                                                  of which micro die site is 
                                                  currently being tested */

    int                      firstMicroDieSiteNumber; /* site number of first
                                                  micro die on a die to be 
                                                  tested. Used to detect a
                                                  pattern complete at the 
                                                  subdie level */

    long                     currentDieX;      /* current die X position */
    long                     currentDieY;      /* current die Y position */

                  /* take value from PHKEY_OP_PAUPROB in configuration file */
    BOOLEAN                  stopProberOnSmartestPause;  

                  /* taken value from PHKEY_PR_SUBDIE in configuration file */
    BOOLEAN                  performSubdieProbing;      

                  /* taken value from PHKEY_BI_INKBAD in configuration file */
    BOOLEAN                  inkBadDies;      

                  /* taken value from PHKEY_PL_EGSENDSETUPCMD in configuration file */
    BOOLEAN                  sendSetupCommand;

                  /* taken value from PHKEY_PL_EGQRYDIEPOS in configuration file */
    BOOLEAN                  queryDiePosition;


    unsigned long            sitesResponse[(PHESTATE_MAX_SITES-1)/32+1];    
                                               /* returned sites response from
                                                  the TS signal when multisite
                                                  probing is enabled */

    BOOLEAN                  sitesPopulated[PHESTATE_MAX_SITES]; 
                                               /* converted value of sitesResponse
                                                  into boolean array for whether
                                                  site is populated or not  */


    int                      sitesResponseArraySize;
                                               /* actual size of sitesResponse array
                                                  calculated from noOfSites */

    BOOLEAN                  probingComplete;

    BOOLEAN                  waferDone;        /* used in subdie level so die level
                                                  also knows to send PAT_DONE */


    BOOLEAN                  proberSimulation;   /* prober is simulated : need to 
                                                    create delays between receiving
                                                    an SRQ and reading the message  */

    /* 
     * 13 Mar 2001 following offsite tests take coordinates from the TS and TF
     * messages and store in following variables 
     */
     long store_dieX;
     long store_dieY;
     BOOLEAN store_die_loc_valid;
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
 *    (1)STDF WCR 
 *
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


#endif /* ! _PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
