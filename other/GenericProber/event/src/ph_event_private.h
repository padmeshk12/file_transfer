/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_event_private.h
 * CREATED  : 27 Jul 1999
 *
 * CONTENTS : Private definitions for event module
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 27 Jul 1999, Chris Joyce, created
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

#ifndef _PH_EVENT_PRIVATE_H_
#define _PH_EVENT_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/
/*--- module includes -------------------------------------------------------*/

#include "ph_diag.h"

/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_2


#define PHEVENT_MAX_MESSAGE_LENGTH 2048
#define PHEVENT_MAX_REPONSE_LENGTH   80
#define PHEVENT_MAX_LINE_LENGTH      80
#define PHEVENT_MAX_BUTTON_LENGTH     9  
#define PHEVENT_MAX_CONF_LENGTH     256 


/*--- typedefs --------------------------------------------------------------*/

typedef BOOLEAN (*pSiteSelectfn)(phEstateSiteUsage_t siteUsage);


/* Enumeration type for the different kinds of system flags */
typedef enum {
    GLOBAL_BOOLEAN       /* global system flag may take boolean values */,
    GLOBAL_INT           /* global system flag may take int values     */,
    SITE_BOOLEAN         /* site specific system flag may take boolean values */,
    SITE_INT             /* site specific system flag may take int values */,
    SITE_STRING          /* site specific system flag that should 
                            be converted from a long to an string */
} systemFlagTypeEnum;

/* struct to build flag-flagtype array */
typedef struct {
  phTcomSystemFlag_t flag;
  systemFlagTypeEnum flagtype;
} systemFlagType;


/* struct for details of site for handtest get start and handtest binning */
typedef struct {
  int siteIndex;
  phEstateSiteUsage_t siteUsage;
  char siteName[PHEVENT_MAX_BUTTON_LENGTH];
} siteDetails;


struct phEventStruct
{
    struct phEventStruct *myself;               /* self reference for validity
                                                   check */
    phPFuncId_t driverId;                        /* the driver plugin ID */
    phComId_t communicationId;                  /* the valid communication link to
                                                   the HW interface used by the
                                                   handler */
    phLogId_t loggerId;                         /* the data logger to be used */
    phConfId_t configurationId;                 /* the configuration manager */
    phStateId_t stateId;                        /* the overall driver state */
    phEstateId_t estateId;                      /* the equipment state */
    phTcomId_t testerId;                        /* the tester communication */

    char msg_str_buffer[PHEVENT_MAX_MESSAGE_LENGTH +1]; 
                                                /* string message for output 
                                                   dialog box */
    int numOfSites;                             /* number of sites found in last
                                                   call to getAllSelectedSiteInfo() */
    phEstateSiteUsage_t siteUsage[PHESTATE_MAX_SITES]; 
                                                /* internal store of site 
                                                        usage details */
    int numOfSelectedSites;                     /* number of selected sites
                                                   counted when getAllSelectedSiteInfo()
                                                   was last called  */
    siteDetails selectedSites[PHESTATE_MAX_SITES]; 
                                                /* selected site details */

    phDiagId_t gpibD;                           /* gpib diagnostics send message */


    /* internally used variables for string manipulation */
    int message_line_position;                  /* for creating messages current 
                                                   position of cursor on line */
    int message_position;                       /* for creating messages current
                                                   position of cursor in message */
    char* message_string;                       /* message string to be used */
    int max_message_line_length;                /* maximum size of message on line */


    /* internally used variables for event error diagnostics */

    char drvGpibPortStr[PHEVENT_MAX_CONF_LENGTH];
    char interfaceNameStr[PHEVENT_MAX_LINE_LENGTH];
    phFrameAction_t faType;
    phPFuncAvailability_t pfType;
    char diagnosticsMsg[PHEVENT_MAX_MESSAGE_LENGTH +1];
};


/*--- external variables ----------------------------------------------------*/

/*--- internal function -----------------------------------------------------*/

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



#endif /* ! _PH_EVENT_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
