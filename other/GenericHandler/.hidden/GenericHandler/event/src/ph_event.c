/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_event.c
 * CREATED  : 19 Jul 1999
 *
 * CONTENTS : Event handler for handler driver framework
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 19 Jul 1999, Michael Vogt, created
 *            27 Jul 1999, Chris Joyce, implemented functions
 *            16 Feb 2001, Chris Joyce, improved error event handling messages
 *                                      and introduced diagnostics functions
 *                                      for GPIB
 *            Dec 2005, Jiawei Lin, CR28409
 *              enhance "phEventError" to disable the popup window if the
 *              user dislike it; add one new parameter "displayWindowForOperator"
 *              for "phEventError" function
 *            7 May 2015, Magco Li, CR93846
 *              add rs232 interface type to phEventInit()
 *
 * Instructions:
 *
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_keys.h"
#include "ph_event.h"
#include "ph_hhook.h"
#include "ph_plugin.h"

#include "ph_GuiServer.h"
#include "ph_diag.h"
#include "ph_event_private.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define COMMENT_PART_LENGTH 256

#define PH_SESSION_CHECK

#ifdef DEBUG
#define PH_SESSION_CHECK
#endif

#ifdef PH_SESSION_CHECK
#define CheckSession(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHEVENT_ERR_INVALID_HDL
#else
#define CheckSession(x)
#endif

#define RELEASE_BUFFER(x) {\
   if ( x != NULL ) \
   {\
     free(x);\
     x = NULL;\
   }\
}


/*****************************************************************************
 *
 *         GUI descriptions for phGuiCreateUserDefDialog calls
 *
 *****************************************************************************/

/*
 *  Event Error gui description
 */
static const char *eventErrorGuiDesc = \
    "S:`eventError`:`%s` {@v \n" \
            "\tL::`%s`"    \
            "a:`diag`:`Diagnostics:`:`%s` " \
            "\tS::{@h\n"   \
                "\t\tp:`p_quit`:`Quit...`:e[`Quit the testprogram`] \n"                      \
                "\t\t*p:`p_cont`:`KeepWaiting...`:e[`carry on waiting`] \n"                 \
            "\t}\n"         \
    "} \n";


static const char *eventErrorGuiOnlyQuitDesc = \
    "S:`eventError`:`%s` {@v \n" \
            "\tL::`%s`"    \
            "a:`diag`:`Diagnostics:`:`%s` " \
            "\tS::{@h\n"   \
                "\t\t*\n" \
                "\t\tp:`p_quit`:`Quit...`:e[`Quit the testprogram`] \n"                      \
            "\t}\n"         \
    "} \n";

/*
 *
 *  EventHandling:     Set devices to test 
 *  Handtestmode:
 *
 */
static const char *setDevicesToTestDesc =      \
    "S:`setDevicesToTest`:`%s` {@v \n"         \
            "\tL::`%s`"                        \
            "\t\tT::`Device population`{\n"    \
                             "%s"              \
                         "\t\t}:%d \n"         \
            "\tL::`\n`"                        \
            "\tS::{@h\n"                       \
                     "%s"                      \
                     "%s"                      \
                     "%s"                      \
            "\t}\n"                            \
                 "}\n";

static const char *setDevicesToTestToggleButtonDesc = "`%s`:`%s`\n";

static const char *setDevicesToTestTimeoutButtonDesc =  \
           "\t\t*p:`p_timeout`:`Timeout`:e[`perform timeout`]\n";

static const char *setDevicesToTestCancelButtonDesc =  \
           "\t\t*p:`p_close`:`Cancel`:e[`return to previous window`]\n";

static const char *setDevicesToTestQuitButtonDesc =  \
           "\t\tp:`p_quit`:`Quit`:e[`quit the testprogram`]\n";

static const char *setDevicesToTestContinueButtonDesc =  \
           "\t\t*p:`p_continue`:`Continue`:e[`continue with test`]\n";

static const char *setDevicesToTestContinueFirstButtonDesc =  \
           "\t\tp:`p_continue`:`Continue`:e[`continue with test`]\n";

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/


/*--- global objects    -----------------------------------------------------*/


static systemFlagType flagType[] =      { { PHTCOM_SF_CI_ABORT,        GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_PAUSE,        GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_QUIT,         GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_RESET,        GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_RETEST,       GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_CHECK_DEV,    GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_LEVEL_END,    GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_SKIP,         GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_CI_BIN_CODE,     SITE_STRING },
                                          { PHTCOM_SF_CI_BIN_NUMBER,   SITE_BOOLEAN },
                                          { PHTCOM_SF_CI_GOOD_PART,    SITE_BOOLEAN },
                                          { PHTCOM_SF_CI_BAD_PART,     SITE_INT },
                                          { PHTCOM_SF_CI_SITE_SETUP,   SITE_INT },
                                          { PHTCOM_SF_LOT_AVAILABLE,    GLOBAL_BOOLEAN },
                                          { PHTCOM_SF_DEVICE_AVAILABLE,    GLOBAL_BOOLEAN },
                                          { PHTCOM_SF__END,            (systemFlagTypeEnum)0              }
                                        };


/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Remove white spaces from beginning and end of string
 *
 *****************************************************************************/

static const char *getEventCauseStr(phEventCause_t reason)
{
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    const char* p=NULL;
/* End of Huatek Modification */

    switch ( reason )
    {
      case PHEVENT_CAUSE_WP_TIMEOUT:
        p="TIMEOUT";
        break;
      case PHEVENT_CAUSE_IF_PROBLEM:
        p="IF PROBLEM";
        break;
      case PHEVENT_CAUSE_ANSWER_PROBLEM:
        p="ANSWER PROBLEM";
        break;
    }

    return p;
}

static const char *getSetDevicesCauseStr(setDevicesCause_t reason)
{
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    const char* p=NULL;
/* End of Huatek Modification */

    switch ( reason )
    {
      case SET_DEVICE_WP_TIMEOUT:
        p="TIMEOUT";
        break;
      case SET_DEVICE_IF_PROBLEM:
        p="IF PROBLEM";
        break;
      case SET_DEVICE_ANSWER_PROBLEM:
        p="ANSWER PROBLEM";
        break;
      case SET_DEVICE_HANDTEST:
        p="HANDTEST";
        break;
    }

    return p;
}

/*****************************************************************************
 *
 * test cell client call string.
 *
 * Authors: Chris Joyce
 *
 * History:
 *
 * Description: Return phFrameAction_t as a string.
 *
 ***************************************************************************/
static char *getFrameActionString(phFrameAction_t faType)
{
    static char frameActionString[PHEVENT_MAX_LINE_LENGTH];

    switch (faType)
    {
        case PHFRAME_ACT_DRV_START:
            sprintf(frameActionString, "%s", "ph_driver_start");
            break;
        case PHFRAME_ACT_DRV_DONE:
            sprintf(frameActionString, "%s", "ph_driver_done");
            break;
        case PHFRAME_ACT_LOT_START:
            sprintf(frameActionString, "%s", "ph_lot_start");
            break;
        case PHFRAME_ACT_LOT_DONE:
            sprintf(frameActionString, "%s", "ph_lot_done");
            break;
        case PHFRAME_ACT_DEV_START:
            sprintf(frameActionString, "%s", "ph_device_start");
            break;
        case PHFRAME_ACT_DEV_DONE:
            sprintf(frameActionString, "%s", "ph_device_done");
            break;
        case PHFRAME_ACT_PAUSE_START:
            sprintf(frameActionString, "%s", "ph_pause_start");
            break;
        case PHFRAME_ACT_PAUSE_DONE:
            sprintf(frameActionString, "%s", "ph_pause_done");
            break;
        case PHFRAME_ACT_HANDTEST_START:
            sprintf(frameActionString, "%s", "ph_handtest_start");
            break;
        case PHFRAME_ACT_HANDTEST_STOP:
            sprintf(frameActionString, "%s", "ph_handtest_stop");
            break;
        case PHFRAME_ACT_STEPMODE_START:
            sprintf(frameActionString, "%s", "ph_stepmode_start");
            break;
        case PHFRAME_ACT_STEPMODE_STOP:
            sprintf(frameActionString, "%s", "ph_stepmode_stop");
            break;
        case PHFRAME_ACT_REPROBE:
            sprintf(frameActionString, "%s", "ph_reprobe");
            break;
        case PHFRAME_ACT_LEAVE_LEVEL:
            sprintf(frameActionString, "%s", "ph_leave_level");
            break;
        case PHFRAME_ACT_CONFIRM_CONFIG:
            sprintf(frameActionString, "%s", "ph_confirm_configuration");
            break;
        case PHFRAME_ACT_ACT_CONFIG:
            sprintf(frameActionString, "%s", "ph_interactive_configuration");
            break;
        case PHFRAME_ACT_SET_CONFIG:
            sprintf(frameActionString, "%s", "ph_set_configuration");
            break;
        case PHFRAME_ACT_GET_CONFIG:
            sprintf(frameActionString, "%s", "ph_get_configuration");
            break;
        case PHFRAME_ACT_DATALOG_START:
            sprintf(frameActionString, "%s", "ph_datalog_start");
            break;
        case PHFRAME_ACT_DATALOG_STOP:
            sprintf(frameActionString, "%s", "ph_datalog_stop");
            break;
        case PHFRAME_ACT_GET_ID:
            sprintf(frameActionString, "%s", "ph_get_id");
            break;
        case PHFRAME_ACT_GET_INS_COUNT:
            sprintf(frameActionString, "%s", "ph_get_insertion_count");
            break;
        case PHFRAME_ACT_TIMER_START:
            sprintf(frameActionString, "%s", "ph_timer_start");
            break;
        case PHFRAME_ACT_TIMER_STOP:
            sprintf(frameActionString, "%s", "ph_timer_stop");
            break;
        case PHFRAME_ACT_GET_STATUS:
            sprintf(frameActionString, "%s", "ph_get_status");
            break;
        case PHFRAME_ACT_SET_STATUS:
            sprintf(frameActionString, "%s", "ph_set_status");
            break;
        case PHFRAME_ACT_EXEC_GPIB_CMD:
            sprintf(frameActionString, "%s", "ph_exec_gpib_cmd");
            break;
        case PHFRAME_ACT_EXEC_GPIB_QUERY:
            sprintf(frameActionString, "%s", "ph_exec_gpib_query");
            break;
        case PHFRAME_ACT_GET_SRQ_STATUS_BYTE:
            sprintf(frameActionString, "%s", "ph_get_srq_status_byte");
            break;
        case PHFRAME_ACT_READ_STATUS_BYTE:
            sprintf(frameActionString, "%s", "ph_read_status_byte");
            break;
        case PHFRAME_ACT_SEND_MESSAGE:
            sprintf(frameActionString, "%s", "ph_send");
            break;
        case PHFRAME_ACT_RECEIVE_MESSAGE:
            sprintf(frameActionString, "%s", "ph_receive");
            break;
        default:
            sprintf(frameActionString, "%s", "undefined frame action");
            break;
    }

    return frameActionString;
}


/*****************************************************************************
 *
 * Get current action string based current plugin call
 *
 * Authors: Chris Joyce
 *
 * History: 5 Dec 2000, Chris Joyce, created
 *
 * Description: Return phFuncAvailability_t as a string.
 *
 ***************************************************************************/
static const char *getCurrentActionString(phFuncAvailability_t pfType)
{
    const char *pluginString;

    switch (pfType)
    {
      case PHFUNC_AV_INIT:
        pluginString="initialization";
        break;
      case PHFUNC_AV_RECONFIGURE:
        pluginString="configure";
        break;
      case PHFUNC_AV_RESET:
        pluginString="reset";
        break;
      case PHFUNC_AV_DRIVERID:
        pluginString="get driver ID";
        break;
      case PHFUNC_AV_EQUIPID:
        pluginString="get equipment ID";
        break;
      case PHFUNC_AV_START:
        pluginString="device start";
        break;
      case PHFUNC_AV_BIN:
        pluginString="bin devices";
        break;
      case PHFUNC_AV_REPROBE:
        pluginString="reprobe";
        break;
      case PHFUNC_AV_COMMAND:
        pluginString="command";
        break;
      case PHFUNC_AV_QUERY:
        pluginString="query";
        break;
      case PHFUNC_AV_DIAG:
        pluginString="get diagnostics";
        break;
      case PHFUNC_AV_BINREPR:
        pluginString="bin and reprobe";
        break;
      case PHFUNC_AV_PAUSE:
        pluginString="pause start";
        break;
      case PHFUNC_AV_UNPAUSE:
        pluginString="pause done";
        break;
      case PHFUNC_AV_STATUS:
        pluginString="status request";
        break;
      case PHFUNC_AV_UPDATE:
        pluginString="update";
        break;
      case PHFUNC_AV_COMMTEST:
        pluginString="communication test";
        break;
      case PHFUNC_AV_EXECGPIBCMD:
        pluginString="send setup and action command";
        break;
      case PHFUNC_AV_EXECGPIBQUERY:
        pluginString="send handler query command";
        break;
      case PHFUNC_AV_GETSRQSTATUSBYTE:
        pluginString="get handler gpib srq status byte";
        break;
      case PHFUNC_AV_DESTROY:
        pluginString="destroy";
        break;
      case PHFUNC_AV_LOT_START:
        pluginString="lot start";
        break;
      default:
        pluginString="undefined plugin function";
        break;
    }

    return pluginString;
}

/*****************************************************************************
 * getSystemFlagString
 *****************************************************************************/

static char *getSystemFlagString(phTcomSystemFlag_t sflag)
{
    static char systemFlagString[PHEVENT_MAX_LINE_LENGTH];

    switch (sflag)
    {
      case PHTCOM_SF_CI_ABORT:
        sprintf(systemFlagString, "abort");
        break;
      case PHTCOM_SF_CI_BAD_PART:
        sprintf(systemFlagString, "bad_part");
        break;
      case PHTCOM_SF_CI_BIN_CODE:
        sprintf(systemFlagString, "bin_code");
        break;
      case PHTCOM_SF_CI_BIN_NUMBER:
        sprintf(systemFlagString, "bin_number");
        break;
      case PHTCOM_SF_CI_CHECK_DEV:
        sprintf(systemFlagString, "check_dev");
        break;
      case PHTCOM_SF_CI_GOOD_PART:
        sprintf(systemFlagString, "good_part");
        break;
      case PHTCOM_SF_CI_LEVEL_END:
        sprintf(systemFlagString, "level_end");
        break;
      case PHTCOM_SF_CI_PAUSE:
        sprintf(systemFlagString, "pause");
        break;
      case PHTCOM_SF_CI_QUIT:
        sprintf(systemFlagString, "quit");
        break;
      case PHTCOM_SF_CI_RESET:
        sprintf(systemFlagString, "reset");
        break;
      case PHTCOM_SF_CI_RETEST:
        sprintf(systemFlagString, "retest");
        break;
      case PHTCOM_SF_CI_SITE_SETUP:
        sprintf(systemFlagString, "site_setup");
        break;
      case PHTCOM_SF_CI_SKIP:
        sprintf(systemFlagString, "skip");
        break;
      case PHTCOM_SF_LOT_AVAILABLE:
        sprintf(systemFlagString, "lot_available");
        break;
      case PHTCOM_SF_DEVICE_AVAILABLE:
        sprintf(systemFlagString, "device_available");
        break;
      default:
        sprintf(systemFlagString, "Unknown PHTCOM System flag!");
        break;
    }
    return &systemFlagString[0];
}


/*****************************************************************************
 * getNumberOfSites
 *****************************************************************************/
static int getNumberOfSites(
    phEventId_t eventID           /* event handler ID */
)
{
    int numSites = 0;
    phEstateSiteUsage_t *sitePopulation = NULL;


    phEstateAGetSiteInfo(eventID->estateId, &sitePopulation, &numSites);

    phLogEventMessage(eventID->loggerId, 
                      LOG_VERBOSE,
                      "number of sites %d ",
                      numSites);

    return numSites;
}

/*****************************************************************************
 * selectActiveSites: selection function of type *pSiteSelectfn
 *****************************************************************************/
static BOOLEAN selectActiveSites(phEstateSiteUsage_t siteUsage)
{
    return (siteUsage != PHESTATE_SITE_DEACTIVATED &&
	siteUsage !=  PHESTATE_SITE_POPDEACT) ? TRUE : FALSE;
}

/*****************************************************************************
 * selectPopulatedSites: selection function of type *pSiteSelectfn
 *****************************************************************************/
static BOOLEAN selectPopulatedSites(phEstateSiteUsage_t siteUsage)
{
    return (siteUsage == PHESTATE_SITE_POPULATED || 
	siteUsage == PHESTATE_SITE_POPDEACT) ? TRUE : FALSE;
}

/*****************************************************************************
 * selectPopdeactSites: selection function of type *pSiteSelectfn
 *****************************************************************************/
static BOOLEAN selectPopdeactSites(phEstateSiteUsage_t siteUsage)
{
    return (siteUsage == PHESTATE_SITE_POPDEACT) ? TRUE : FALSE;
}

/*****************************************************************************
 * getAllSelectedSiteInfo
 *****************************************************************************/
static void getAllSelectedSiteInfo(
    phEventId_t eventID           /* event handler ID */,
    pSiteSelectfn selectfn        /* pointer to selection function */
)
{
    phConfError_t confRtnValue=PHCONF_ERR_OK;
    phEstateSiteUsage_t *sitePopulation = NULL;
    int siteCounter;
    int selectedSiteCounter;
    const char *siteString;

    /* get site information */
    eventID->numOfSites = 0;
    phEstateAGetSiteInfo(eventID->estateId, &sitePopulation, &(eventID->numOfSites));

    phLogEventMessage(eventID->loggerId, 
                      LOG_VERBOSE,
                      "number of sites %d ",
                      eventID->numOfSites);

    selectedSiteCounter=0;
    for (siteCounter=0; siteCounter<eventID->numOfSites; siteCounter++)
    {
        eventID->siteUsage[siteCounter]=sitePopulation[siteCounter];

        if ( (*selectfn)(sitePopulation[siteCounter]) )
        {
            confRtnValue = phConfConfString(eventID->configurationId,
                                            PHKEY_SI_HSIDS, 
                                            1, 
                                            &siteCounter, 
                                            &siteString);

            if ( confRtnValue == PHCONF_ERR_OK )
            {
                strncpy(eventID->selectedSites[selectedSiteCounter].siteName,siteString,PHEVENT_MAX_BUTTON_LENGTH);

                /* ensure siteName string is null-terminated */
                eventID->selectedSites[selectedSiteCounter].siteName[PHEVENT_MAX_BUTTON_LENGTH-1]='\0';

                eventID->selectedSites[selectedSiteCounter].siteIndex=siteCounter;
                eventID->selectedSites[selectedSiteCounter].siteUsage=sitePopulation[siteCounter];

                phLogEventMessage(eventID->loggerId,
                                  LOG_VERBOSE,
                                  "add %s to selected sites ",
                                  eventID->selectedSites[selectedSiteCounter].siteName);

                selectedSiteCounter++;
            }
        }
    }
 
    eventID->numOfSelectedSites=selectedSiteCounter;

    return;
}

/*****************************************************************************
 * getSelectedSiteSiteIndex
 *****************************************************************************/
static int getSelectedSiteSiteIndex(
    phEventId_t eventID           /* event handler ID */,
    int index                     /* index to selectedSites array in event struct */
)
{
    return eventID->selectedSites[index].siteIndex;
}

/*****************************************************************************
 * updateSiteInfo
 *****************************************************************************/
static void updateSiteInfo(
    phEventId_t eventID           /* event handler ID */
)
{
    phEstateError_t stateRtnValue=PHESTATE_ERR_OK;
    int selectedSiteCounter;
    int siteIndex;

    /* update copy of site info (eventID->siteUsage) with new site usage */
    for ( selectedSiteCounter=0 ; selectedSiteCounter < eventID->numOfSelectedSites ; selectedSiteCounter++ )
    {
        siteIndex = getSelectedSiteSiteIndex(eventID,selectedSiteCounter);
        eventID->siteUsage[siteIndex]=eventID->selectedSites[selectedSiteCounter].siteUsage;
    }

    /* set new site information */
    stateRtnValue=phEstateASetSiteInfo(eventID->estateId, eventID->siteUsage, eventID->numOfSites);
}

/*****************************************************************************
 * updateSiteInfoSetPopulatedEmpty
 *
 * 12/07/2000 add perSiteReprobe information to call. Do not set site
 * to empty if this is a reprobe for this site since device
 * must be retested.
 *
 *****************************************************************************/
static void updateSiteInfoSetPopulatedEmpty(
    phEventId_t eventID           /* event handler ID */,
    long *perSiteReprobe          /* TRUE, if a device needs reprobe */
)
{
    phEstateError_t stateRtnValue=PHESTATE_ERR_OK;
    int selectedSiteCounter;
    int siteIndex;

    /* update copy of site info (eventID->siteUsage) so site all populated sites are set empty */
    for ( selectedSiteCounter=0 ; selectedSiteCounter < eventID->numOfSelectedSites ; selectedSiteCounter++ )
    {
        siteIndex = getSelectedSiteSiteIndex(eventID,selectedSiteCounter);

        if ( !perSiteReprobe || !perSiteReprobe[siteIndex] )
        { 
            eventID->siteUsage[siteIndex]=PHESTATE_SITE_EMPTY;
        }
    }

    /* set new site information */
    stateRtnValue=phEstateASetSiteInfo(eventID->estateId, eventID->siteUsage, eventID->numOfSites);
}


/*****************************************************************************
 * updateSiteInfoSetPopdeactDeactivated
 *
 * 12/07/2000 add perSiteReprobe information to call. Do not set site
 * to deactivated if this is a reprobe for this site since device
 * must be retested.
 *
 *****************************************************************************/
static void updateSiteInfoSetPopdeactDeactivated(
    phEventId_t eventID           /* event handler ID */,
    long *perSiteReprobe          /* TRUE, if a device needs reprobe */
)
{
    phEstateError_t stateRtnValue=PHESTATE_ERR_OK;
    int selectedSiteCounter;
    int siteIndex;

    /* update copy of site info (eventID->siteUsage) so site all populated sites are set empty */
    for ( selectedSiteCounter=0 ; selectedSiteCounter < eventID->numOfSelectedSites ; selectedSiteCounter++ )
    {
        siteIndex = getSelectedSiteSiteIndex(eventID,selectedSiteCounter);

        if ( !perSiteReprobe || !perSiteReprobe[siteIndex] )
        { 
            eventID->siteUsage[siteIndex]=PHESTATE_SITE_DEACTIVATED;
        }
    }

    /* set new site information */
    stateRtnValue=phEstateASetSiteInfo(eventID->estateId, eventID->siteUsage, eventID->numOfSites);
}

/*****************************************************************************
 *
 *    initializeMessageString
 *    initialize message string buffer and set-up internal variables
 *
 *****************************************************************************/
static void initializeMessageString(
    phEventId_t eventID           /* event handler ID */,
    int max_length                /* maximum line length to be used */,
    char* buffer                  /* message string buffer to be used */
)
{
    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "initializeMessageString(P%p, %d, P%p)",
                      eventID,
                      max_length,
                      buffer);

    eventID->max_message_line_length=max_length;
    eventID->message_string=buffer;

    /* re-set message string */
    eventID->message_position=0;
    eventID->message_line_position=0;
    strcpy(eventID->message_string,"");
}

/*****************************************************************************
 *
 * addNewLineToMessageString
 * add new line to message string if not already started on a new line.
 *
 *****************************************************************************/
static void addNewLineToMessageString(
    phEventId_t eventID           /* event handler ID */
)
{
    int last_char = strlen(eventID->message_string) - 1;

    if ( eventID->message_position + 1 < PHEVENT_MAX_MESSAGE_LENGTH &&
         last_char >= 0 &&
         eventID->message_string[last_char] != '\n' )
    {
        strcat(eventID->message_string,"\n");
        eventID->message_position++;
        eventID->message_line_position=0;
    }

}

/*****************************************************************************
 *
 *    initializeInternalString
 *    initialize internal string and set-up internal variables
 *
 *****************************************************************************/
static void initializeInternalString(
    phEventId_t eventID           /* event handler ID */
)
{
    initializeMessageString(eventID,
                            PHEVENT_MAX_LINE_LENGTH,
                            eventID->msg_str_buffer);
}

/*****************************************************************************
 * getString
 *****************************************************************************/
static char* getString(
    phEventId_t eventID           /* event handler ID */
)
{
    return eventID->message_string;
}

/*****************************************************************************
 * addSimpleStrToMsgStr
 *****************************************************************************/
static void addSimpleStrToMsgStr(
    phEventId_t eventID           /* event handler ID */,
    const char* source            /* string to be added to message string */
)
{
    int length;
    const char *ptrNewline;

    length = strlen(source);

    if ( eventID->message_position + length + 1 < PHEVENT_MAX_MESSAGE_LENGTH )
    {
        /*
         * if string is not too big to be added then add it.
         * if newlines are included in string then use this format for string
         * otherwise check the new length of the string and added a
         * new line to the string if this is greater than eventID->max_message_line_length
         */

        ptrNewline = strrchr(source,'\n');

        if ( ptrNewline )
        {
            eventID->message_line_position = ( source + length -1 ) - ptrNewline;
        }
        else
        {
            /* check to see if you should write on a new line */
            if ( eventID->message_line_position &&
                 (eventID->message_line_position + length > eventID->max_message_line_length) )
            {
                strcat(eventID->message_string,"\n");
                eventID->message_position++;
                eventID->message_line_position=0;
            }
            eventID->message_line_position += length;
        }
        strcat(eventID->message_string,source);
        eventID->message_position += length;
    }
}

/*****************************************************************************
 * addStrToMsgStrWordByWord
 *
 * break string up given in pBuffer into words and send each word to
 * addSimpleStrToMsgStr
 *
 *****************************************************************************/
static void addStrToMsgStrWordByWord(
    phEventId_t eventID           /* event handler ID */,
    char* pBuffer                 /* string to be added to message string */
)
{
    char *ptrToWS;
    char c;

    ptrToWS = strchr(pBuffer, ' ');

    while ( ptrToWS )
    {
        ++ptrToWS;

        c=*ptrToWS;

        *ptrToWS='\0';

        addSimpleStrToMsgStr(eventID, pBuffer);

        *ptrToWS=c;

        pBuffer=ptrToWS;

        ptrToWS = strchr(ptrToWS, ' ');
    }

    addSimpleStrToMsgStr(eventID, pBuffer);
}


/*****************************************************************************
 *
 * addFormatStrToMsgStr
 *
 *****************************************************************************/
static void addStringToMsgStr(
    phEventId_t eventID           /* event handler ID */,
    const char* format            /* format string to be added to message
                                     string as in printf(3) */,
    ...                           /* variable argument list */
)
{
    char buffer[PHEVENT_MAX_MESSAGE_LENGTH +1];
    va_list argp;

    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    vsprintf(buffer, format, argp);
    addStrToMsgStrWordByWord(eventID, buffer);
    va_end(argp);

    return;
}

/*****************************************************************************
 * addSystemFlagsToMessageString
 *****************************************************************************/
static void addSystemFlagsToMessageString(
    phEventId_t eventID           /* event handler ID */
)
{
    phTcomSystemFlag_t systemflag;
    systemFlagTypeEnum systemFlagType;
    long value;
    phTcomError_t tcomRtnValue=PHTCOM_ERR_OK;
    int numOfSites;
    int site;
    char flagString[PHEVENT_MAX_LINE_LENGTH];
    char lineString[PHEVENT_MAX_LINE_LENGTH];
    char *systemFlagName;
    int flagTypeCounter;
    int addNewLine=0;     /* add new line between global flags and site specific flags */


    numOfSites=getNumberOfSites(eventID);


    for ( flagTypeCounter=0 ; flagType[flagTypeCounter].flag != PHTCOM_SF__END ; flagTypeCounter++ )
    {
        systemFlagType=flagType[flagTypeCounter].flagtype;
        systemflag=flagType[flagTypeCounter].flag;
        systemFlagName=getSystemFlagString(systemflag);

        if ( systemFlagType == GLOBAL_BOOLEAN || systemFlagType == GLOBAL_INT )
        {
            tcomRtnValue = phTcomGetSystemFlag(eventID->testerId,systemflag,&value);

            if ( systemFlagType == GLOBAL_BOOLEAN )
            {
                if  ( value == 1 )
                {
                    addStringToMsgStr(eventID,"%s ",systemFlagName);
                }
            }
            else
            {       /* GLOBAL_INT */
                    addStringToMsgStr(eventID,"%s: %ld ",systemFlagName,value);
            }
        }
        else  /* SITE_BOOLEAN || SITE_STRING || SITE_INT || SITE_FLAG */
        {
            if ( !addNewLine )
            {
                addNewLineToMessageString(eventID);
                addNewLine=1;
            }

            sprintf(flagString,"%s[",systemFlagName);
            strcpy(lineString,flagString);

            for ( site =1 ; site <= numOfSites && tcomRtnValue==PHTCOM_ERR_OK ; site++ )
            {
                if (flagType[flagTypeCounter].flag == PHTCOM_SF_CI_BIN_CODE)
                {
                    tcomRtnValue = phTcomGetBinOfSite(eventID->testerId,site,flagString,&value);
                }
                else if (flagType[flagTypeCounter].flag == PHTCOM_SF_CI_BIN_NUMBER)
                {
                    tcomRtnValue = phTcomGetBinOfSite(eventID->testerId,site,flagString,&value);
                    sprintf(flagString,"%ld",value);
                }
                else
                {
                    tcomRtnValue = phTcomGetSystemFlagOfSite(eventID->testerId,systemflag,site,&value);
                    if ( systemFlagType == SITE_STRING )
                    {
                        strncpy(flagString,(char*)(&value),4);
                        flagString[4]='\0';
                    }
                    else
                    {
                        sprintf(flagString,"%ld",value);
                    }
                }

                strcat( lineString, flagString );
                if ( site != numOfSites )
                {
                    strcat(lineString, " ");
                }
            }
            strcat( lineString, "] " );
            addStringToMsgStr( eventID, lineString );
        }
    }
}


/*****************************************************************************
 * addBinAndReprobeInfoToMessageString
 *****************************************************************************/
static void addBinAndReprobeInfoToMessageString(
    phEventId_t eventID           /* event handler ID */,
    int index                     /* index to selectedSites array in event struct */,
    int reprobe                   /* TRUE, if device needs reprobe */,
    int binNum                    /* bin number index to handler_bin_ids */
)
{
    phConfError_t confRtnValue=PHCONF_ERR_OK;
    const char *binString;
    char binInfoString[PHEVENT_MAX_MESSAGE_LENGTH];
    int listLength = 0;
    phConfType_t defType;

    phLogEventMessage(eventID->loggerId, 
                      PHLOG_TYPE_TRACE,
                      "addBinAndReprobeInfoToMessageString(P%p, %d, %d, %d)",
                      eventID,
                      index,
                      reprobe,
                      binNum);

    if ( reprobe )
    {
        sprintf(binInfoString,"reprobe device at site %s ",eventID->selectedSites[index].siteName);
        addStringToMsgStr( eventID, "%s", binInfoString );
    }
    else
    {
        sprintf(binInfoString,"bin device at site %s to bin ",eventID->selectedSites[index].siteName);

        addStringToMsgStr( eventID, "%s", binInfoString );

        /* get length of PHKEY_BI_HBIDS list */ 
        confRtnValue = phConfConfIfDef(eventID->configurationId, PHKEY_BI_HBIDS);
        if (confRtnValue == PHCONF_ERR_OK)
        {
            listLength = 0;
            confRtnValue = phConfConfType(eventID->configurationId, PHKEY_BI_HBIDS,
                0, NULL, &defType, &listLength);
            if (confRtnValue != PHCONF_ERR_OK )
            {
                phLogEventMessage(eventID->loggerId, PHLOG_TYPE_ERROR,
                                  "unable to determine '%s' list length \n"
                                  PHKEY_BI_HBIDS);
            }
        }

        if ( confRtnValue == PHCONF_ERR_OK && 
             binNum >= 0 &&
             binNum < listLength )
        {
            confRtnValue = phConfConfString(eventID->configurationId,
                                            PHKEY_BI_HBIDS,
                                            1,
                                            &binNum,
                                            &binString);

            addStringToMsgStr( eventID, "%s", binString );
        }
        else
        {
            sprintf(binInfoString,"[%d] ",binNum);
            addStringToMsgStr( eventID, "%s", binInfoString );
        }
    }

    addNewLineToMessageString(eventID);
    return;
}

/*****************************************************************************
 *
 * Check GUI error and convert into a event error.
 *
 *****************************************************************************/
static  phEventError_t checkGuiError(
    phEventId_t eventID           /* event handler ID */,
    phGuiError_t guiError         /* status from gui call */
)
{
    phEventError_t rtnValue;

    switch (guiError)
    {
      case PH_GUI_ERR_OK:
        /* no error */
        rtnValue=PHEVENT_ERR_OK;
        break;
      default:
        phLogEventMessage(eventID->loggerId, PHLOG_TYPE_ERROR,
            "received error code %d from gui server",
            (int) guiError);
        rtnValue=PHEVENT_ERR_GUI;
        break;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Get GUI BOOL data
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce
 *
 * Description:
 * Gets GUI data string for componentName and converts to integer according
 * to format.
 *
 *****************************************************************************/
static phEventError_t getGuiDataBool(
    phEventId_t eventID           /* event handler ID */,
    phGuiProcessId_t guiId        /* guiID of process */,
    const char *componentName     /* name of the component */,
    int *value                    /* int value to set */
)
{
    phEventError_t rtnValue;
    phGuiError_t guiError;

    int timeout_getGuiData=5;
    char response[PHEVENT_MAX_LINE_LENGTH];
    char cmptName[PHEVENT_MAX_LINE_LENGTH];

    strcpy(cmptName,componentName);

    guiError = phGuiGetData(guiId, cmptName, response, timeout_getGuiData);
    rtnValue=checkGuiError(eventID,guiError);

    if ( rtnValue == PHEVENT_ERR_OK )
    {
        if ( strcasecmp(response, "true") == 0)
        {
            *value=TRUE;
#if DEBUG
            phLogEventMessage(eventID->loggerId, LOG_VERBOSE,
                "interpretted component %s returned value \"%s\" as %d \n",
                componentName,response,*value);
#endif
        }
        else if( strcasecmp(response, "false") == 0)
        {
            *value=FALSE;
#if DEBUG
            phLogEventMessage(eventID->loggerId, LOG_VERBOSE,
                "interpretted component %s returned value \"%s\" as %d \n",
                componentName,response,*value);
#endif
        }
        else
        {
            phLogEventMessage(eventID->loggerId, PHLOG_TYPE_ERROR,
                "unable to interpret gui server return value \"%s\" \n"
                "format expected true | false ",
                response);
            rtnValue=PHEVENT_ERR_GUI;
        }
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Set GUI BOOL data
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce
 *
 * Description:
 * Gets GUI data string for componentName and converts to integer according
 * to format.
 *
 *****************************************************************************/
static phEventError_t setGuiDataBool(
    phEventId_t eventID           /* event handler ID */,
    phGuiProcessId_t guiId        /* guiID of process */,
    const char *componentName     /* name of the component */,
    int value                     /* int value to set */
)
{
    phEventError_t rtnValue;
    phGuiError_t guiError;

/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int timeout_getGuiData=5;*/
/*    char response[PHEVENT_MAX_LINE_LENGTH];*/
/* End of Huatek Modification */
    char cmptName[PHEVENT_MAX_LINE_LENGTH];

    strcpy(cmptName,componentName);

    if ( value )
    {
        guiError = phGuiChangeValue(guiId, cmptName, "True");
    }
    else
    {
        guiError = phGuiChangeValue(guiId, cmptName, "False");
    }
    rtnValue=checkGuiError(eventID,guiError);

    return rtnValue;
}


/*****************************************************************************
 * confirmationDialog
 *  Quit                             Cont
 *  Please confirm:
 *  Do you really want to...
 *
 *  QUIT the testprogram or
 *  CONTINUE where you were.
 *
 *****************************************************************************/
static int confirmationDialog(
    phEventId_t eventID           /* event handler ID */
)
{
    phEventResult_t result;
    phTcomUserReturn_t stReturn;
    long response = 0;

    phLogEventMessage(eventID->loggerId, 
                      PHLOG_TYPE_TRACE,
                      "confirmationDialog(P%p)",
                      eventID);

    phEventPopup(eventID, &result, &stReturn, 1, "menu",
	"Please confirm: \n"
	"Do you really want to... \n\n"
        "QUIT - Quit the testprogram or\n"
        "CONTINUE - Go back to the dialog.",
	"", "", "", "", "", "",
	&response, NULL);

    if (result == PHEVENT_RES_ABORT || response == 1)
    {
        phTcomSetSystemFlag(eventID->testerId, PHTCOM_SF_CI_QUIT, 1L);
        phTcomSetSystemFlag(eventID->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
    }

    return (response == 1);
}

/*****************************************************************************
 *
 * Handtest binAndReprobe
 *
 * History: 3 Jul 2000, Chris Joyce, implemented
 *
 * Description:
 *
 *****************************************************************************/
static phEventError_t handtestBinAndReprobe(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    long *perSiteReprobe          /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap           /* valid binning data for each site where
                                     the above reprobe flag is not set */,
    phFuncError_t *funcError      /* the simulated get start result */
)
{
    int selectedSiteCounter;
    int siteCounter;
    char dialogString[PHEVENT_MAX_MESSAGE_LENGTH +1];
    int firstSite=0;            /* site associated with the first line 
                                   of bin information on dialog box */
    int lastSite;               /* site associated with the last line 
                                   on bin information on dialog box */
    int maxNumBinsDisplayed=10; /* maxium number of bins dialog box
                                   is able to display on one screen */
    long response;
    const char *MoreButton;
    const char *MoreMessage;


    /* get all populated site information */
    getAllSelectedSiteInfo(eventID,&selectPopulatedSites);

    if ( eventID->numOfSelectedSites > maxNumBinsDisplayed )
    {
        MoreButton="More";
        MoreMessage="or see\nMORE bin information ";
    }
    else
    {
        MoreButton="";
        MoreMessage="";
    }

    firstSite=0;
    do
    {
        lastSite=firstSite+maxNumBinsDisplayed-1 ;

        if ( lastSite >= eventID->numOfSelectedSites )
        {
            lastSite=eventID->numOfSelectedSites-1;
        }

        /* initialize internal message string for site info details */
        initializeInternalString(eventID);

        for ( selectedSiteCounter=firstSite ; selectedSiteCounter <= lastSite ; selectedSiteCounter++ )
        {
            siteCounter=getSelectedSiteSiteIndex(eventID,selectedSiteCounter);
            addBinAndReprobeInfoToMessageString(eventID,
                                                selectedSiteCounter,
                                                perSiteReprobe && perSiteReprobe[siteCounter],
                                                perSiteBinMap && perSiteBinMap[siteCounter]);
        }

        if ( perSiteReprobe && perSiteBinMap )
        {
            strcpy(dialogString,"Handler driver hand test mode: bin and reprobe devices");
        }
        else if ( perSiteReprobe )
        {
            strcpy(dialogString,"Handler driver hand test mode: reprobe devices");
        }
        else
        {
            strcpy(dialogString,"Handler driver hand test mode: bin devices");
        }

        strcat(dialogString,"\nDo you want to...");
        strcat(dialogString,"\nQUIT the testprogram ");
        strcat(dialogString,MoreMessage);
        strcat(dialogString,"or \nCONTINUE with the testprogram. \n\n");
        strcat(dialogString,"Populated site bin information: \n");
        strcat(dialogString,getString(eventID));
    
        phEventPopup(eventID, result, stReturn, 1, "menu",
	    dialogString,
	    "",
	    "",
	    "",
	    "",
	    "",
	    MoreButton,
	    &response, NULL);
	if (*result == PHEVENT_RES_ABORT)
	    return PHEVENT_ERR_OK;

        switch (response)
        {
            case 1: /* QUIT */
                response=confirmationDialog(eventID);
                if ( response == 1 )
                {
                    *result=PHEVENT_RES_ABORT;

		    /* accept the quit flag, but return with PASS to
		       avoid an error message from the application
		       model. */

                    *stReturn=PHTCOM_CI_CALL_PASS;
                }
                break;
            case 7: /* MORE */
                firstSite=lastSite+1;
                if ( firstSite >= eventID->numOfSelectedSites )
                {
                    firstSite=0;
                }
                break;
            case 8: /* CONT */

	      getAllSelectedSiteInfo(eventID,&selectPopdeactSites);
	      updateSiteInfoSetPopdeactDeactivated(eventID,perSiteReprobe);

	      getAllSelectedSiteInfo(eventID,&selectPopulatedSites);
	      updateSiteInfoSetPopulatedEmpty(eventID,perSiteReprobe);

	      *result=PHEVENT_RES_HANDLED;
              *funcError=PHFUNC_ERR_OK;
              break;
        }
    } while (response != 1 && response != 7 && response != 8 );

    return PHEVENT_ERR_OK;    
}


/*****************************************************************************
 *
 * Initialize driver event handler
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, dummy stub created
 *           4 Aug 1999, Chris Joyce, implemented
 *
 * Description:
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventInit(
    phEventId_t *eventID         /* result event handler ID */,
    phFuncId_t driver            /* the driver plugin ID */,
    phComId_t communication      /* the valid communication link to the HW
			            interface used by the handler */,
    phLogId_t logger             /* the data logger to be used */,
    phConfId_t configuration     /* the configuration manager */,
    phStateId_t state            /* the overall driver state */,
    phEstateId_t estate          /* the equipment specific state */,
    phTcomId_t tester            /* the tester communication */
)
{
    phEventError_t rtnValue=PHEVENT_ERR_OK;
    phDiagError_t diagError;
    phConfError_t confError;
    struct phEventStruct *tmpId;
    const char *confIntType = NULL;

    /* set eventID to NULL in case anything goes wrong */
    *eventID = NULL;

    phLogEventMessage(logger,
                      PHLOG_TYPE_TRACE,
                      "phEventInit(P%p, P%p, P%p, P%p, P%p, P%p, P%p, P%p)",
                      eventID,
                      driver,
                      communication,
                      logger,
                      configuration,
                      state,
                      estate,
                      tester);

    /* allocate new event data type */
    tmpId = PhNew(struct phEventStruct);
    if (tmpId == 0)
    {
        phLogEventMessage(logger,
                          PHLOG_TYPE_FATAL,
                          "not enough memory during call to phEventInit()");
        rtnValue=PHEVENT_ERR_MEMORY;
    }

    /* set values for structure */
    if ( rtnValue == PHEVENT_ERR_OK )
    {
        /* if have not returned yet everything is okay */
        tmpId -> myself = tmpId;
        tmpId -> driverId = driver;
        tmpId -> communicationId = communication;
        tmpId -> loggerId = logger;
        tmpId -> configurationId = configuration;
        tmpId -> stateId = state;
        tmpId -> estateId = estate;
        tmpId -> testerId = tester;
    }

    /* create diagnostic handle */
    if ( rtnValue == PHEVENT_ERR_OK )
    {
        diagError=phDiagnosticsInit( &(tmpId->gpibD), communication, logger);
        if ( diagError != DIAG_ERR_OK )
        {
            phLogEventMessage(logger,
                              PHLOG_TYPE_FATAL,
                              "error returned from diagnostics initialization (%d)",
                              diagError);
            rtnValue=PHEVENT_ERR_DIAG;
        }
    }

    /* get interface type */
    if ( rtnValue == PHEVENT_ERR_OK )
    {
        confError = phConfConfString(tmpId->configurationId,
            PHKEY_IF_TYPE, 0, NULL, &confIntType);
        if (confError != PHCONF_ERR_OK)
        {
            phLogFrameMessage(tmpId->loggerId, PHLOG_TYPE_FATAL,
                "could not get interface type from configuration, giving up");
            rtnValue=PHEVENT_ERR_CONF;
        }
        else
        {
            if (strcasecmp(confIntType, "gpib") == 0 )
            {
                tmpId->interfaceType=GPIB;
            }
            else if (strcasecmp(confIntType, "lan") == 0 )
            {
                tmpId->interfaceType=LAN;
            }
             else if (strcasecmp(confIntType, "lan-server") == 0 )
            {
                tmpId->interfaceType=LAN_SERVER;
            }
            else if (strcasecmp(confIntType, "rs232") == 0 )
            {
                tmpId->interfaceType=RS232;
            }
            else
            {
                phLogFrameMessage(tmpId->loggerId, PHLOG_TYPE_FATAL,
                    "unknown interface type \"%s\", giving up", confIntType);
                rtnValue=PHEVENT_ERR_CONF;
            }
        }
    }

    if (rtnValue == PHEVENT_ERR_OK )
    {
        *eventID  = tmpId;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Driver action event
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, dummy stub created
 *           4 Aug 1999, Chris Joyce, implemented
 *          25 Aug 1999, Michael Vogt, call to hook layer added
 *
 * Description:
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventAction(
    phEventId_t eventID           /* event handler ID */,    
    phFrameAction_t call          /* the identification of an incoming
				     call from the test cell client */,
    phEventResult_t *result       /* the result of this event call */,
    char *parm_list_input         /* the parameters passed over from
				     the test cell client */,
    int parmcount                 /* the parameter count */,
    char *comment_out             /* the comment to be send back to the
				     test cell client */,
    int *comlen                   /* the length of the resulting
				     comment */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this event
				     function */
)
{
    phStateDrvMode_t driverMode;           /* current driver mode */
    char dialogString[PHEVENT_MAX_MESSAGE_LENGTH +1];
    char appModelString[PHEVENT_MAX_MESSAGE_LENGTH +1];
    long response;
    const char *DumpButton="Dump";
    const char *DumpMessage="DUMP the driver's current trace for analysis or\n";
    phHookError_t hookError;

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventAction(P%p, %d, P%p, \"%s\", %d, P%p, P%p, P%p)", 
                      eventID,
                      call,
                      result,
                      parm_list_input ? parm_list_input : "(NULL)",
                      parmcount,
                      comment_out,
                      comlen,
                      stReturn);

    /* first give the hook layer a chance to handle this call, but
       only if existent */

    if (phPlugHookAvailable() & PHHOOK_AV_ACTION_START)
    {
	hookError = phPlugHookActionStart(call, result, parm_list_input, 
	    parmcount, comment_out, comlen, stReturn);

	if (hookError == PHHOOK_ERR_OK && 
	    (*result == PHEVENT_RES_ABORT || *result == PHEVENT_RES_HANDLED))
	{
	    /* hook function took over control, log this fact and
               return immediately */

	    phLogEventMessage(eventID->loggerId, LOG_NORMAL,
		"action start hook enforces immediate event return\n"
		"result: %ld, comment_out: \"%s\", comlen: %d, return: %s",
		(long) *result,
		comment_out, comlen, 
		*stReturn == PHTCOM_CI_CALL_PASS ?
		"PASS" : (*stReturn == PHTCOM_CI_CALL_ERROR ?
		"ERROR" : "BREAKD"));

	    return PHEVENT_ERR_OK;
	}
    }

    /* check to see if single step mode has been set */

    phStateGetDrvMode(eventID->stateId,&driverMode);

    if ( driverMode == PHSTATE_DRV_SST || driverMode == PHSTATE_DRV_SST_HAND )
    {
        /* in single step mode: Create dialog box for user to see wishes to continue */
            
        /* initialize internal message string for app model call and system flags */
        initializeInternalString(eventID);
        /* print app-model part */
        sprintf(appModelString,"%s(\"%s\") ",getFrameActionString(call),parm_list_input);
        addStringToMsgStr(eventID,"%s",appModelString);
        addNewLineToMessageString(eventID);
        /* print system flags part */
        addSystemFlagsToMessageString(eventID);

        /* create dialog  */
        do
        {
            strcpy(dialogString,"Handler driver single step mode:\n");
            strcat(dialogString,"Do you want to...\n");
            strcat(dialogString,"QUIT the testprogram or\n");
            strcat(dialogString,"STEP perform the next driver action\n");
            strcat(dialogString,DumpMessage);
            strcat(dialogString,"CONTINUE with normal driver operation breaking out of single step mode.\n\n");
            strcat(dialogString,"Handler driver call and current settings for selected system flags:\n");
            strcat(dialogString,getString(eventID));

            phEventPopup(eventID, result, stReturn, 1, "menu",
		dialogString,
		"Step",
		DumpButton,
		"",
		"",
		"",
		"",
		&response, NULL);
	    if (*result == PHEVENT_RES_ABORT)
		return PHEVENT_ERR_OK;

            switch (response)
            {
                case 1: /* QUIT */
                    response=confirmationDialog(eventID);
                    if ( response == 1 )
                    {
                        *result=PHEVENT_RES_ABORT;

			/* accept the quit flag, but return with
			   PASS to avoid an error message from the
			   test cell client. */

                        *stReturn=PHTCOM_CI_CALL_PASS;
                    }
                    break;
                case 2: /* STEP */
                    *result=PHEVENT_RES_CONTINUE;
                    break;
                case 3: /* DUMP */
                    phLogDump(eventID->loggerId, PHLOG_MODE_ALL);
                    DumpButton="";
                    DumpMessage="";
                    break;
                case 8: /* CONT */
                    if ( driverMode == PHSTATE_DRV_SST )
                    {
                        phStateSetDrvMode(eventID->stateId, PHSTATE_DRV_NORMAL);
                    }
                    else  /* PHSTATE_DRV_SST_HAND */
                    {
                        phStateSetDrvMode(eventID->stateId, PHSTATE_DRV_HAND);
                    }
                    *result=PHEVENT_RES_CONTINUE;
                    break;
            }
        } while (response != 1 && response != 2 && response != 8 );
    }

    return PHEVENT_ERR_OK;
}

/*****************************************************************************
 *
 * Driver action done event
 *
 * Authors: Michael Vogt
 *
 * History: 25 Aug 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventDoneAction(
    phEventId_t eventID           /* event handler ID */,    
    phFrameAction_t call          /* the identification of an incoming
				     call from the test cell client */,
    char *parm_list_input         /* the parameters passed over from
				     the test cell client */,
    int parmcount                 /* the parameter count */,
    char *comment_out             /* the comment to be send back to
				     the test cell client, as already
				     set by the driver */,
    int *comlen                   /* the length of the resulting
				     comment */,
    phTcomUserReturn_t *stReturn  /* SmarTest return value as already
				     set by the driver */
)
{
    phHookError_t hookError;
    phTcomUserReturn_t orgStReturn;
    int orgComlen;
    char orgComment_out[COMMENT_PART_LENGTH+1];

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId, PHLOG_TYPE_TRACE,
        "phEventDoneAction(P%p, %d, \"%s\", %d, P%p, P%p, P%p)",
        eventID, call,
        parm_list_input ? parm_list_input : "(NULL)", parmcount,
        comment_out, comlen, stReturn);

    /* first give the hook layer a chance to handle this call, but
       only if existent */

    if (phPlugHookAvailable() & PHHOOK_AV_ACTION_DONE)
    {
	/* save original result values */
	orgStReturn = *stReturn;
	orgComlen = *comlen;
	strncpy(orgComment_out, comment_out, COMMENT_PART_LENGTH);
	orgComment_out[COMMENT_PART_LENGTH] = '\0';

	hookError = phPlugHookActionDone(call, parm_list_input, 
	    parmcount, comment_out, comlen, stReturn);

	if (hookError == PHHOOK_ERR_OK && 
	    (*stReturn != orgStReturn ||
	     *comlen != orgComlen ||
	     strncmp(orgComment_out, comment_out, COMMENT_PART_LENGTH) != 0))
	{
	    /* hook function took over control, log this fact */

	    phLogEventMessage(eventID->loggerId, LOG_NORMAL,
		"action done hook has overruled driver return values\n"
		"comment_out: \"%s\", comlen: %d, return: %s",
		comment_out, *comlen, 
		*stReturn == PHTCOM_CI_CALL_PASS ?
		"PASS" : (*stReturn == PHTCOM_CI_CALL_ERROR ?
		"ERROR" : "BREAKD"));
	}
    }

    return PHEVENT_ERR_OK;    
}


/*****************************************************************************
 *
 * Handtest mode | phEventError : Set devices to test
 *
 * Authors: Chris Joyce
 *
 * History: 27 Feb 2001, Chris Joyce, created
 *
 * Description:
 * mixture of following:
 *      phEventHandtestGetStart(eventID,result,stReturn,&funcError);
 *      phEventError_t phEventHandtestGetStart()
 * 
 *****************************************************************************/
static phEventError_t phEventSetDevicesToTest(
    phEventId_t eventID           /* event handler ID */,
    setDevicesCause_t reason      /* the reason, why this function is called */,
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    phFuncError_t *funcError      /* the simulated get start result */
)
{
    phEventError_t rtnValue=PHEVENT_ERR_OK;
    phGuiError_t guiError;

    const char *setDevicesTitle;
    char setDevicesText[PHEVENT_MAX_MESSAGE_LENGTH +1];

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    char *descr=NULL;
/* End of Huatek Modification */
    phGuiProcessId_t gui;
    int displayDialog;                  /* keep display this dialog while true */
    char response[PHEVENT_MAX_REPONSE_LENGTH];   /* gui response string */
    int dialog_length;
    int siteInfoIndex;
    char siteToggleKey[PHEVENT_MAX_LINE_LENGTH];
    char siteToggleString[PHEVENT_MAX_LINE_LENGTH];
    char *toggleButtonsDesc;
    const char *firstDesc;
    const char *timeoutDesc;
    const char *lastDesc;
    int value;

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventSetDevicesToTest(P%p, \"%s\", P%p, P%p, P%p)",
                      eventID,
                      getSetDevicesCauseStr(reason),
                      result,
                      stReturn,
                      funcError);

    /* Set-up set devices title */
    if ( reason == SET_DEVICE_HANDTEST )
    {
        setDevicesTitle="Handtest Mode: Set Devices to Test";
    }
    else
    {
        setDevicesTitle="Prober/Handler: Set Devices to Test";
    }

    /* Set-up set devices text */
    dialog_length = 85;
    initializeMessageString(eventID, dialog_length, setDevicesText);
    if ( reason == SET_DEVICE_HANDTEST )
    {
        addStringToMsgStr(eventID,"\nSet the devices to test and choose one of the following options... \n\n");
        addStringToMsgStr(eventID,"CONTINUE to continue with the testprogram using the device settings below \n");
        addStringToMsgStr(eventID,"TIMEOUT to send a timeout signal or \n");
        addStringToMsgStr(eventID,"QUIT... to quit the test program\n");
    }
    else
    {
        addStringToMsgStr(eventID,"\nYou have decided to manually set the devices to test ");
        addStringToMsgStr(eventID,"because ");
        switch ( reason )
        {
          case SET_DEVICE_WP_TIMEOUT:
            addStringToMsgStr(eventID,"the device start operation has timed out. ");
            break;
          case SET_DEVICE_IF_PROBLEM:
            addStringToMsgStr(eventID,"an interface problem has occured during the device start operation. ");
            break;
          case SET_DEVICE_ANSWER_PROBLEM:
            addStringToMsgStr(eventID,"the answer received from the handler during a device start operation ");
            addStringToMsgStr(eventID,"was not understood. ");
            break;
/* Begin of Huatek Modification, Donnie Tu, 12/11/2001 */
/* Issue Number: 326 */
          default: break;
/* End of Huatek Modification */
        }
        addStringToMsgStr(eventID,"Before starting any of these tests please check the ");
        addStringToMsgStr(eventID,"communication cable between the tester and handler. ");
        addStringToMsgStr(eventID,"Then set the devices to test below and choose one of the following options... \n\n");
        addStringToMsgStr(eventID,"CANCEL to go back to the previous dialog without starting device test\n");
        addStringToMsgStr(eventID,"CONTINUE with the testprogram using the device settings below ");
    }

    /* 
     * Set-up set devices toggle buttons 
     *     get all active site information 
     *     create toggle part of description
     */
    getAllSelectedSiteInfo(eventID,&selectActiveSites);
    toggleButtonsDesc = (char *) malloc( ( strlen(setDevicesToTestToggleButtonDesc) + 
                                           16 +
                                           PHEVENT_MAX_BUTTON_LENGTH ) * eventID->numOfSelectedSites );

    if (toggleButtonsDesc == 0)
    {
        rtnValue=PHEVENT_ERR_MEMORY;
    }
    else
    {
        strcpy(toggleButtonsDesc,"");
        for ( siteInfoIndex=0 ; siteInfoIndex < eventID->numOfSelectedSites ; ++siteInfoIndex )
        {
            sprintf(siteToggleKey,"toggle_%d",siteInfoIndex);

            sprintf(siteToggleString,setDevicesToTestToggleButtonDesc,
                                     siteToggleKey,
                                     eventID->selectedSites[siteInfoIndex].siteName);
            strcat(toggleButtonsDesc,siteToggleString);
        }
    }

    /* 
     * Set-up set devices GUI description
     */
    if ( rtnValue == PHEVENT_ERR_OK )
    {
        if ( reason == SET_DEVICE_HANDTEST )
        {
            firstDesc   = setDevicesToTestQuitButtonDesc;
            timeoutDesc = setDevicesToTestTimeoutButtonDesc;
            lastDesc    = setDevicesToTestContinueButtonDesc;
        }
        else
        {
            firstDesc   = setDevicesToTestContinueFirstButtonDesc;
            timeoutDesc = "";
            lastDesc    = setDevicesToTestCancelButtonDesc;
        }

        descr = (char *) malloc(strlen(setDevicesToTestDesc) +
                                strlen(setDevicesTitle)  +
                                strlen(setDevicesText)   +
                                strlen(toggleButtonsDesc) + 
                                3 +
                                strlen(firstDesc) + 
                                strlen(timeoutDesc) + 
                                strlen(lastDesc) + 1);

        if (descr == 0)
        {
            rtnValue=PHEVENT_ERR_MEMORY;
        }
        else
        {
            sprintf(descr, setDevicesToTestDesc, setDevicesTitle,
                                                 setDevicesText,
                                                 toggleButtonsDesc,
                                                 eventID->numOfSelectedSites,
                                                 firstDesc,
                                                 timeoutDesc,
                                                 lastDesc);

            guiError=phGuiCreateUserDefDialog(&gui, eventID->loggerId, PH_GUI_COMM_SYNCHRON,
                                              descr);
            rtnValue=checkGuiError(eventID,guiError);

            phLogEventMessage(eventID->loggerId, LOG_NORMAL,
                              "Set Devices To Test Dialog: %s\n"
                              "%s\n"
                              "Device population \n"
                              "%s\n"
                              "%s\n",
                              setDevicesTitle,
                              setDevicesText,
                              toggleButtonsDesc,
                              (reason==SET_DEVICE_HANDTEST)  ? "QUIT TIMEOUT CONTINUE" : "CONTINUE CANCEL");
        }
    }

    /* 
     * Set-up set devices toggle buttons
     */
    for ( siteInfoIndex = 0 ;
          rtnValue==PHEVENT_ERR_OK && siteInfoIndex<eventID->numOfSelectedSites ;
          ++siteInfoIndex )
    {
        sprintf(siteToggleKey,"toggle_%d",siteInfoIndex);

        /* 
         * following GUI review suggestion all sites which can be set populated
         * are set populated.
         *
         * previous setting:
         * value=(eventID->selectedSites[siteInfoIndex].siteUsage==PHESTATE_SITE_POPULATED);
         *
         */
        value=1;

        rtnValue = setGuiDataBool(eventID, gui, siteToggleKey, value);
    }

    displayDialog=TRUE;
    while ( rtnValue==PHEVENT_ERR_OK && displayDialog==TRUE )
    {
        if ( rtnValue == PHEVENT_ERR_OK )
        {
            guiError=phGuiShowDialogGetResponse(gui, 0, response);
            rtnValue=checkGuiError(eventID,guiError);
        }

        if ( rtnValue == PHEVENT_ERR_OK )
        {
            guiError=phGuiDisableComponent(gui,"setDevicesToTest");
            rtnValue = checkGuiError( eventID, guiError );
        }

        if ( strcmp(response,"p_continue")==0 )   /* Continue */
        {
            phLogEventMessage(eventID->loggerId, LOG_NORMAL,"User pressed 'CONTINUE'");

            /* 
             * Retreive toggle buttons site populations
             */
            for ( siteInfoIndex = 0 ;
                  rtnValue==PHEVENT_ERR_OK && siteInfoIndex<eventID->numOfSelectedSites ;
                  ++siteInfoIndex )
            {
                sprintf(siteToggleKey,"toggle_%d",siteInfoIndex);
                rtnValue = getGuiDataBool(eventID, gui, siteToggleKey, &value);

                if ( value )
                {
                    eventID->selectedSites[siteInfoIndex].siteUsage=PHESTATE_SITE_POPULATED;
                    phLogEventMessage(eventID->loggerId, LOG_NORMAL,
                                      "site \"%s\" is set POPULATED",
                                      eventID->selectedSites[siteInfoIndex].siteName);
                }
                else
                {
                    eventID->selectedSites[siteInfoIndex].siteUsage=PHESTATE_SITE_EMPTY;
                    phLogEventMessage(eventID->loggerId, LOG_NORMAL,
                                      "site \"%s\" is set EMPTY",
                                      eventID->selectedSites[siteInfoIndex].siteName);
                }
            }
            if ( rtnValue == PHEVENT_ERR_OK )
            {
                updateSiteInfo(eventID);

                *result=PHEVENT_RES_HANDLED;
                *funcError=PHFUNC_ERR_OK;
            }
            displayDialog=FALSE;
        }
        else if ( strcmp(response,"p_timeout")==0 )   /* Timeout */
        {
            phLogEventMessage(eventID->loggerId, LOG_NORMAL,"User pressed 'TIMEOUT'");

            *result=PHEVENT_RES_CONTINUE;
            *funcError=PHFUNC_ERR_TIMEOUT;
            displayDialog=FALSE;
        }
        else if ( strcmp(response,"p_close")==0 )   /* Close */
        {
            phLogEventMessage(eventID->loggerId, LOG_NORMAL,"User pressed 'CLOSE'");
            displayDialog=FALSE;
        }
        else  /* "p_quit" ( Quit ) */
        {
            phLogEventMessage(eventID->loggerId, LOG_NORMAL,"User pressed 'QUIT'");

            if ( confirmationDialog(eventID) == 1 )
            {
                *result=PHEVENT_RES_ABORT;
                /* accept the quit flag, but return with PASS to
                   avoid an error message from the application
                   model. */
                *stReturn=PHTCOM_CI_CALL_PASS;
                displayDialog=FALSE;
            }
        }

        if ( rtnValue==PHEVENT_ERR_OK && displayDialog==TRUE )
        {
            guiError=phGuiEnableComponent(gui,"setDevicesToTest");
            rtnValue = checkGuiError( eventID, guiError );
        }
    }
    phGuiDestroyDialog(gui);
    free(toggleButtonsDesc);
    free(descr);

    return PHEVENT_ERR_OK;
}


/*****************************************************************************
 *
 * Error event
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, dummy stub created
 *           4 Aug 1999, Chris Joyce, implemented
 *
 * Description:
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventError(
    phEventId_t eventID           /* event handler ID */,    
    phEventCause_t reason         /* the reason, why this event is called */,
    phEventDataUnion_t *data      /* pointer to additional data,
				     associated with the given
				     <reason>, or NULL, if no
				     additional data is available */,
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this event
				     function */,
    int displayWindowForOperator  /* YES or NO, introduced for CR28409 */
)
{
    phEventError_t rtnValue=PHEVENT_ERR_OK;
    phFuncError_t funcError;
    phGuiError_t guiError;
    phTcomError_t tcomRtnValue=PHTCOM_ERR_OK;
    phConfError_t confRtnValue=PHCONF_ERR_OK;

    BOOLEAN onlyQuitButton = TRUE;
    BOOLEAN readDiagMsg = FALSE;
    const char *onlyQuitButtonConfig = NULL;
    const char *eventErrorGuiDescTemp = NULL;

    long value;
    BOOLEAN promptOperator=TRUE;  /* default action */
    int displayDialog;
    char *diagnosticsString = NULL;
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    phHookError_t hookError=PHHOOK_ERR_OK;
/* End of Huatek Modification */

    char* descr = NULL;
    const char *secondButtonDesc;
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    const char *eventErrorTitle=NULL;
/* End of Huatek Modification */
    const char *handledDesc;
    const char *startDesc;

    phGuiProcessId_t gui = NULL, pleaseWaitGui = NULL;
    char response[PHEVENT_MAX_REPONSE_LENGTH];
    char eventErrorText[PHEVENT_MAX_MESSAGE_LENGTH +1];
    char emptyStr[] = "";

    long hookResponse;

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventError(P%p, %s, P%p, P%p, P%p)", 
                      eventID,
                      getEventCauseStr(reason),
                      data,
                      result,
                      stReturn);

    /* first give the hook layer a chance to handle this call, but
       only if existent */

    if (phPlugHookAvailable() & PHHOOK_AV_PROBLEM)
    {
	hookError = phPlugHookProblem(reason, data, result, stReturn);

	if (hookError == PHHOOK_ERR_OK && 
	    (*result == PHEVENT_RES_ABORT || *result == PHEVENT_RES_HANDLED))
	{
	    /* hook function took over control, log this fact and
               return immediately */

	    phLogEventMessage(eventID->loggerId, LOG_NORMAL,
		"problem hook enforces immediate event return\n"
		"result: %ld, return: %s",
		(long) *result,
		*stReturn == PHTCOM_CI_CALL_PASS ?
		"PASS" : (*stReturn == PHTCOM_CI_CALL_ERROR ?
		"ERROR" : "BREAKD"));

	    return PHEVENT_ERR_OK;
	}
    }

    /* first see if PAUSE flag has been set */
    tcomRtnValue = phTcomGetSystemFlag(eventID->testerId,PHTCOM_SF_CI_PAUSE,&value);
    if ( tcomRtnValue == PHTCOM_ERR_OK && value == 1 )
    {
        phLogEventMessage(eventID->loggerId,
                          LOG_DEBUG,
                          "pause flag set so prompt operator about timeout");
        promptOperator=TRUE;
    }
    else if ( reason==PHEVENT_CAUSE_WP_TIMEOUT )
    {
        switch (data->to.opAction)
        {
            case PHEVENT_OPACTION_CONTINUE :
                promptOperator=FALSE;
                break;
            case PHEVENT_OPACTION_SKIP :
                //return with handled
                *result = PHEVENT_RES_HANDLED;
                promptOperator=FALSE;
                break;
            case PHEVENT_OPACTION_ASK:
                promptOperator=TRUE;
                break;
        }
    }
    else  /*    PHEVENT_CAUSE_IF_PROBLEM           */
    {     /* or PHEVENT_CAUSE_ANSWER_PROBLEM       */
        promptOperator=TRUE;
    }

    if ( (promptOperator==TRUE) && (displayWindowForOperator==YES) )
    {
        /* Set-up internally used variables for diagnostics */
        switch ( reason )
        {
          case PHEVENT_CAUSE_WP_TIMEOUT:
            eventID->faType=data->to.am_call;
            eventID->pfType=data->to.p_call;
            onlyQuitButton = FALSE;
            readDiagMsg = FALSE;
            break;
          case PHEVENT_CAUSE_IF_PROBLEM:
            eventID->faType=data->intf.am_call;
            eventID->pfType=data->intf.p_call;
            onlyQuitButton = FALSE;
            readDiagMsg = TRUE;
            break;
          case PHEVENT_CAUSE_ANSWER_PROBLEM:
            eventID->faType=data->intf.am_call;
            eventID->pfType=data->intf.p_call;
            onlyQuitButton = TRUE;
            readDiagMsg = TRUE;
            break;
        }

#if 0
        eventID->allowHandledOperation =   reason == PHEVENT_CAUSE_WP_TIMEOUT && 
                                           eventID->pfType == PHFUNC_AV_EQUIPID;
#endif
        /* never show the ignore button which is dangerous */
        eventID->allowHandledOperation = FALSE;

        eventID->retryPluginCall=FALSE;

        /* Set-up Event Error title */
        switch ( reason )
        {
          case PHEVENT_CAUSE_WP_TIMEOUT:
            eventErrorTitle="Prober/Handler: Timeout";
            break;
          case PHEVENT_CAUSE_IF_PROBLEM:
            eventErrorTitle="Prober/Handler: Interface Error";
            break;
          case PHEVENT_CAUSE_ANSWER_PROBLEM:
            eventErrorTitle="Prober/Handler: Handler not understood";
            break;
        }

        /* Set-up Event Error text */
        initializeMessageString(eventID, 100, eventErrorText);
        addStringToMsgStr(eventID,"The \"%s\" operation has not yet been completed.\n",
                getCurrentActionString(eventID->pfType));
        if(onlyQuitButton != TRUE)
        {
          addStringToMsgStr(eventID,"\nPlease choose one of the following options...\n\n");
          addStringToMsgStr(eventID,"KEEP WAITING... to carry on waiting for the operation to finish\n");
        }

        addStringToMsgStr(eventID,"QUIT... to quit the test program \n");

        /*
         * Set-up diagnostic string: use internal message string.
         */
        initializeMessageString(eventID, 100, eventID->diagnosticsMsg);
        if ( phPlugFuncAvailable() & PHFUNC_AV_DIAG && readDiagMsg)
        {
            funcError=phPlugFuncDiag(eventID->driverId,
                                      &diagnosticsString);
        }
        else
        {
            diagnosticsString=emptyStr;
        }
        addStringToMsgStr(eventID,"%s",diagnosticsString);
        if ( !(*getString(eventID)) )
        {
            addStringToMsgStr(eventID,"<not available>");
        }

        /* Set-up troubleshoot GUI description */

        eventErrorGuiDescTemp = onlyQuitButton?eventErrorGuiOnlyQuitDesc:eventErrorGuiDesc;

        /*
         * Create GUI description string
         */
        descr = (char *) malloc(strlen(eventErrorGuiDescTemp) +
                                strlen(eventErrorTitle)   +
                                strlen(eventErrorText)    +
                                strlen(eventID->diagnosticsMsg) + 1);
        if (descr == 0)
        {
            rtnValue=PHEVENT_ERR_MEMORY;
        }
        if ( rtnValue==PHEVENT_ERR_OK )
        {
            sprintf(descr, eventErrorGuiDescTemp, 
                           eventErrorTitle, 
                           eventErrorText, 
                           getString(eventID));

            guiError=phGuiCreateUserDefDialog(&gui, eventID->loggerId, PH_GUI_COMM_SYNCHRON,
                                              descr);
            rtnValue=checkGuiError(eventID,guiError);
        }

        if ( descr != NULL )
        {
            free(descr);
            descr = NULL;
        }

        /* Now give hook popup function call chance to take over */
        if (phPlugHookAvailable() & PHHOOK_AV_POPUP)
        {
            hookError = phPlugHookPopup(result, stReturn, 1, eventErrorText,
                "Troubleshoot",
                "",
                const_cast<char*>((eventID->pfType==PHFUNC_AV_START) ? "Start" : ""),
                "", "", "", &hookResponse, NULL);

            if (hookError == PHHOOK_ERR_OK &&
                (*result == PHEVENT_RES_ABORT || *result == PHEVENT_RES_HANDLED))
            {
                /* hook function took over control, log this fact and
                   return immediately */

                phLogEventMessage(eventID->loggerId, LOG_NORMAL,
                    "popup hook enforces immediate event return\n"
                    "result: %ld, return: %s",
                    (long) *result,
                    *stReturn == PHTCOM_CI_CALL_PASS ?
                    "PASS" : (*stReturn == PHTCOM_CI_CALL_ERROR ?
                    "ERROR" : "BREAKD"));

                return PHEVENT_ERR_OK;
            }
        }

        if(onlyQuitButton)
        {
            phLogEventMessage(eventID->loggerId, LOG_NORMAL,
                    "Event Handling Dialog: %s\n"
                    "The \"%s\" operation has not yet been completed.\n"
                    "Diagnostics: "
                    "%s\n"
                    "QUIT...",
                    eventErrorTitle,
                    getCurrentActionString(eventID->pfType),
                    eventID->diagnosticsMsg);
        }
        else
        {
            phLogEventMessage(eventID->loggerId, LOG_NORMAL,
                    "Event Handling Dialog: %s\n"
                    "The \"%s\" operation has not yet been completed.\n"
                    "Diagnostics: "
                    "%s\n"
                    "QUIT...    KEEP WAITING... ",
                    eventErrorTitle,
                    getCurrentActionString(eventID->pfType),
                    eventID->diagnosticsMsg);
         }

        displayDialog=TRUE;
        *result=PHEVENT_RES_VOID;
        while ( rtnValue==PHEVENT_ERR_OK && displayDialog==TRUE )
        {
            guiError=phGuiShowDialogGetResponse(gui, 0, response);
            rtnValue=checkGuiError(eventID,guiError);

            if ( rtnValue == PHEVENT_ERR_OK )
            {
                guiError=phGuiDisableComponent(gui,"eventError");
                rtnValue = checkGuiError( eventID, guiError );
            }

            if ( strcmp(response,"p_quit")==0 ) /* QUIT */
            {
                if ( confirmationDialog(eventID) == 1 )
                {
                    *result=PHEVENT_RES_ABORT;

                    /* accept the quit flag, but return with
                       PASS to avoid an error message from the
                       test cell client. */

                    *stReturn=PHTCOM_CI_CALL_PASS;
                    displayDialog=FALSE;
                    phLogEventMessage(eventID->loggerId, LOG_NORMAL,"User pressed 'QUIT'");
                }
            }
            else /* "p_cont" RETRY */
            {
                *result=PHEVENT_RES_CONTINUE;
                displayDialog=FALSE;
                phLogEventMessage(eventID->loggerId, LOG_NORMAL,"User pressed 'KEEP WAITING'");
            }
            if ( rtnValue==PHEVENT_ERR_OK && displayDialog==TRUE )
            {
                guiError=phGuiEnableComponent(gui,"eventError");
                rtnValue = checkGuiError( eventID, guiError );
            }
        }

        phGuiDestroyDialog(gui);        
    }
    else
    {
        phLogEventMessage(eventID->loggerId,
            PHLOG_TYPE_WARNING,
            "timeout occured while waiting for action to complete\n"
            "Pause flag may be set to interrupt waiting for completion.");
    }

    return PHEVENT_ERR_OK;
}


/*****************************************************************************
 *
 * Waiting for parts event
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, dummy stub created
 *
 * Description:
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventWaiting(
    phEventId_t eventID           /* event handler ID */,    
    long elapsedTimeSec           /* elapsed time while waiting for
				     this part */,
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */
)
{
    phHookError_t hookError;

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventWaiting(P%p, %ld, P%p, P%p)", 
                      eventID,
                      elapsedTimeSec,
                      result,
                      stReturn);

    /* first give the hook layer a chance to handle this call, but
       only if existent */

    if (phPlugHookAvailable() & PHHOOK_AV_WAITING)
    {
	hookError = phPlugHookWaiting(elapsedTimeSec, result, stReturn);

	if (hookError == PHHOOK_ERR_OK && 
	    (*result == PHEVENT_RES_ABORT || *result == PHEVENT_RES_HANDLED))
	{
	    /* hook function took over control, log this fact and
               return immediately */

	    phLogEventMessage(eventID->loggerId, LOG_NORMAL,
		"waiting hook enforces immediate event return\n"
		"result: %ld, return: %s",
		(long) *result,
		*stReturn == PHTCOM_CI_CALL_PASS ?
		"PASS" : (*stReturn == PHTCOM_CI_CALL_ERROR ?
		"ERROR" : "BREAKD"));

	    return PHEVENT_ERR_OK;
	}
    }

    *result = PHEVENT_RES_VOID;
    return PHEVENT_ERR_OK;
}


/*****************************************************************************
 *
 * Message popup event
 *
 * Authors: Michael Vogt
 *
 * History: 25 Aug 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventPopup(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHHOOK_RES_ABORT by underlying hook
				     function */,
    int  needQuitAndCont          /* we really need the default QUIT
				     and CONTINUE buttons to be
				     displayed */,
    const char *title             /* title of dialog box */,
    const char *msg               /* the message to be displayed */,
    const char *b2s               /* additional button with text */,
    const char *b3s               /* additional button with text */,
    const char *b4s               /* additional button with text */,
    const char *b5s               /* additional button with text */,
    const char *b6s               /* additional button with text */,
    const char *b7s               /* additional button with text */,
    long *response                /* response (button pressed), ranging
				     from 1 ("quit") over 2 to 7 (b2s
				     to b7s) up to 8 ("continue") */,
    char *input                   /* pointer to area to fill in reply
				     string from operator or NULL, if no 
				     reply is expected */
)
{
    phHookError_t hookError;
    phGuiError_t testerError;

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId, PHLOG_TYPE_TRACE, 
	"phEventPopup(P%p, P%p, P%p, %d, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", P%p, P%p)",
	eventID, result, stReturn, needQuitAndCont,
        title ? title : "(NULL)",
	msg ? msg : "(NULL)",
	b2s ? b2s : "(NULL)",
	b3s ? b3s : "(NULL)",
	b4s ? b4s : "(NULL)",
	b5s ? b5s : "(NULL)",
	b6s ? b6s : "(NULL)",
	b7s ? b7s : "(NULL)",
	response, input);

    /* first give the hook layer a chance to handle this call, but
       only if existent */

    if (phPlugHookAvailable() & PHHOOK_AV_POPUP)
    {
	hookError = phPlugHookPopup(result, stReturn, needQuitAndCont, msg, 
	    b2s, b3s, b4s, b5s, b6s, b7s, response, input);

	if (hookError == PHHOOK_ERR_OK && 
	    (*result == PHEVENT_RES_ABORT || *result == PHEVENT_RES_HANDLED))
	{
	    /* hook function took over control, log this fact and
               return immediately */

	    phLogEventMessage(eventID->loggerId, LOG_NORMAL,
		"popup hook enforces immediate event return\n"
		"result: %ld, return: %s",
		(long) *result,
		*stReturn == PHTCOM_CI_CALL_PASS ?
		"PASS" : (*stReturn == PHTCOM_CI_CALL_ERROR ?
		"ERROR" : "BREAKD"));

	    return PHEVENT_ERR_OK;
	}
    }

    /* perform popup */

    if (input)
	testerError = phGuiMsgDisplayMsgGetStringResponse(eventID->loggerId, 
	    needQuitAndCont, title, msg, b2s, b3s, b4s, b5s, b6s, b7s, response, input);
    else
	testerError = phGuiMsgDisplayMsgGetResponse(eventID->loggerId, 
	    needQuitAndCont, title, msg, b2s, b3s, b4s, b5s, b6s, b7s, response);

    *result = PHEVENT_RES_HANDLED;

    return PHEVENT_ERR_OK;    
}


/*****************************************************************************
 *
 * Get start signal (handtest)
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, dummy stub created
 *           4 Aug 1999, Chris Joyce, implemented
 *
 * Description:
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventHandtestGetStart(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    phFuncError_t *funcError      /* the simulated get start result */
)
{
    return phEventSetDevicesToTest(eventID, SET_DEVICE_HANDTEST, result, stReturn, funcError);
}


/*****************************************************************************
 *
 * Bin tested devices (handtest)
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, dummy stub created
 *           6 Aug 1999, Chris Joyce, implemented
 *
 * Description:
 *
 * please refer to ph_event.h
 *
 *****************************************************************************/

phEventError_t phEventHandtestBinDevice(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    long *perSiteBinMap           /* bin map */
)
{
    phFuncError_t funcError;

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventHandtestBinDevice(P%p, P%p, P%p, P%p)", 
                      eventID,
                      result,
                      stReturn,
                      perSiteBinMap);

    return handtestBinAndReprobe(eventID, result, stReturn, NULL, perSiteBinMap, &funcError);
}


/*****************************************************************************
 *
 * Bin tested devices (handtest)
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, dummy stub created
 *          10 Aug 1999, Chris Joyce, implemented
 *
 * Description:
 *
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventHandtestReprobe(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    phFuncError_t *funcError      /* the simulated get start result */
)
{
    long perSiteReprobe[PHESTATE_MAX_SITES];
    int numOfSites;
    int i;

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventHandtestReprobe(P%p, P%p, P%p, P%p)", 
                      eventID,
                      result,
                      stReturn,
                      funcError);

    numOfSites=getNumberOfSites(eventID);

    /* prepare to reprobe everything */
    for (i=0 ; i<numOfSites ; i++)
    {
        perSiteReprobe[i] = 1L;
    }

    return handtestBinAndReprobe(eventID, result, stReturn, perSiteReprobe, NULL, funcError);
}

/*****************************************************************************
 *
 * Bin or reprobe tested devices (handtest)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jun 2000, Michael Vogt, dummy stub created
 *
 * Description:
 *
 * please refer to ph_event.h
 *
 *****************************************************************************/
phEventError_t phEventHandtestBinReprobe(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    long *perSiteReprobe          /* reprobe flags */,
    long *perSiteBinMap           /* bin map */,
    phFuncError_t *funcError      /* the simulated reprobe result */
)
{

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventHandtestBinReprobe(P%p, P%p, P%p, P%p, P%p, P%p)", 
                      eventID,
                      result,
                      stReturn,
                      perSiteReprobe,
                      perSiteBinMap,
                      funcError);

    return handtestBinAndReprobe(eventID, result, stReturn, perSiteReprobe, perSiteBinMap, funcError);
}

/*Begin of Huatek Modifications, Donnie Tu, 04/23/2002*/
/*Issue Number: 334*/
void phEventFree(phEventId_t *eventID)
{

    if(*eventID)
    {
       phDiagnosticsFree(&((*eventID)->gpibD));
       free(*eventID);
       *eventID=NULL;
    }
}
/*End of Huatek Modifications*/



/*****************************************************************************
 * End of file
 *****************************************************************************/
