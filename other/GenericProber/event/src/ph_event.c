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
 *            Dangln Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 19 Jul 1999, Michael Vogt, created
 *            27 Jul 1999, Chris Joyce, implemented functions
 *            14 Jan 2000, Ulrich Frank, changed phEventPopup
 *             7 Nov 2000, Chris Joyce, improved error event handling messages
 *                                      and introduced diagnostics functions
 *            Dec 2005, Jiawei Lin, CR28409
 *              enhance "phEventError" to disable the popup window if the
 *              user dislike it; add one new parameter "displayWindowForOperator"
 *              for "phEventError" function
 *            Nov 2008, CR41221 & CR42599
 *              add entries for execute gpib command and query for
 *              getFrameActionString and getCurrentActionString handling
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
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
#include <ctype.h>
/* End of Huatek Modification */

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_keys.h"
#include "ph_event.h"
#include "ph_phook.h"
#include "ph_GuiServer.h"
#include "ph_diag.h"
#include "ph_event_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

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

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/


/*--- global objects    -----------------------------------------------------*/

     /*  info about system flags taken from /opt/93000/src/prod_com/CI_CPI/libcicpi.h   */
     /*                                                                                   */
     /*  CI_ABORT .....................: R/W                                    BOOLEAN   */
     /*  CI_BAD_PART ..................: R              site specific           BOOLEAN   */
     /*  CI_BIN_CODE ..................: R              site specific           INT       */
     /*  CI_BIN_NUMBER ................: R              site specific           INT       */
     /*  CI_CHECK_DEV .................: R/W                                    BOOLEAN   */
     /*  CI_GOOD_PART .................: R              site specific           BOOLEAN   */
     /*  CI_LEVEL_END .................: R/W                                    BOOLEAN   */
     /*  CI_PAUSE .....................: R/W                                    BOOLEAN   */
     /*  CI_QUIT ......................: R/W                                    BOOLEAN   */
     /*  CI_RESET .....................: R/W                                    BOOLEAN   */
     /*  CI_RETEST ....................: R                                      BOOLEAN   */
     /*  CI_SITE_SETUP ................: R/W            site specific           INT       */
     /*  CI_SKIP ......................: R                                      BOOLEAN   */

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
                                          { PHTCOM_SF__END,            (systemFlagTypeEnum)0              }
                                        };


/*--- functions -------------------------------------------------------------*/

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
        case PHFRAME_ACT_CASS_START:
            sprintf(frameActionString, "%s", "ph_cassette_start");
            break;
        case PHFRAME_ACT_CASS_DONE:
            sprintf(frameActionString, "%s", "ph_cassette_done");
            break;
        case PHFRAME_ACT_WAF_START:
            sprintf(frameActionString, "%s", "ph_wafer_start");
            break;
        case PHFRAME_ACT_WAF_DONE:
            sprintf(frameActionString, "%s", "ph_wafer_done");
            break;
        case PHFRAME_ACT_DIE_START:
            sprintf(frameActionString, "%s", "ph_die_start");
            break;
        case PHFRAME_ACT_DIE_DONE:
            sprintf(frameActionString, "%s", "ph_die_done");
            break;
        case PHFRAME_ACT_SUBDIE_START:
            sprintf(frameActionString, "%s", "ph_subdie_start");
            break;
        case PHFRAME_ACT_SUBDIE_DONE:
            sprintf(frameActionString, "%s", "ph_subdie_done");
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
        case PHFRAME_ACT_CLEAN:
            sprintf(frameActionString, "%s", "ph_clean_probe");
            break;
        case PHFRAME_ACT_PMI:
            sprintf(frameActionString, "%s", "ph_pmi");
            break;
        case PHFRAME_ACT_INSPECT:
            sprintf(frameActionString, "%s", "ph_inspect_wafer");
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
        case PHFRAME_ACT_GET_STATUS:
            sprintf(frameActionString, "%s", "ph_get_status");
            break;
       case PHFRAME_ACT_EXEC_GPIB_CMD:
            sprintf(frameActionString, "%s", "ph_exec_gpib_cmd");
            break;
       case PHFRAME_ACT_EXEC_GPIB_QUERY:
            sprintf(frameActionString, "%s", "ph_exec_gpib_query");
            break;
/*
        case PHFRAME_ACT_GET_INS_COUNT:
            sprintf(frameActionString, "%s", "ph_get_insertion_count");
            break;
*/
        case PHFRAME_ACT_TIMER_START:
            sprintf(frameActionString, "%s", "ph_timer_start");
            break;
        case PHFRAME_ACT_TIMER_STOP:
            sprintf(frameActionString, "%s", "ph_timer_stop");
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
 * Description: Return phPFuncAvailability_t as a string.
 *
 ***************************************************************************/
static const char *getCurrentActionString(phPFuncAvailability_t pfType)
{
    const char *pluginString;

    switch (pfType)
    {
        case PHPFUNC_AV_INIT:
            pluginString="initialization";
            break;
        case PHPFUNC_AV_RECONFIGURE:
            pluginString="configure";
            break;
        case PHPFUNC_AV_RESET:
            pluginString="reset";
            break;
        case PHPFUNC_AV_DRIVERID:
            pluginString="get driver ID";
            break;
        case PHPFUNC_AV_EQUIPID:
            pluginString="get equipment ID";
            break;
        case PHPFUNC_AV_LDCASS:
            pluginString="cassette start";
            break;
        case PHPFUNC_AV_UNLCASS:
            pluginString="cassette done";
            break;
        case PHPFUNC_AV_LDWAFER:
            pluginString="wafer start";
            break;
        case PHPFUNC_AV_UNLWAFER:
            pluginString="wafer done";
            break;
        case PHPFUNC_AV_GETDIE:
            pluginString="die start";
            break;
        case PHPFUNC_AV_BINDIE:
            pluginString="die done";
            break;
        case PHPFUNC_AV_GETSUBDIE:
            pluginString="subdie start";
            break;
        case PHPFUNC_AV_BINSUBDIE:
            pluginString="subdie done";
            break;
        case PHPFUNC_AV_DIELIST:
            pluginString="get die list";
            break;
        case PHPFUNC_AV_SUBDIELIST:
            pluginString="get subdie list";
            break;
        case PHPFUNC_AV_REPROBE:
            pluginString="reprobe";
            break;
        case PHPFUNC_AV_CLEAN:
            pluginString="clean probe needles";
            break;
        case PHPFUNC_AV_PMI:
            pluginString="PMI";
            break;
        case PHPFUNC_AV_PAUSE:
            pluginString="pause start";
            break;
        case PHPFUNC_AV_UNPAUSE:
            pluginString="pause done";
            break;
        case PHPFUNC_AV_TEST:
            pluginString="send test command";
            break;
        case PHPFUNC_AV_DIAG:
            pluginString="get diagnostics";
            break;
        case PHPFUNC_AV_STATUS:
            pluginString="status request";
            break;
        case PHPFUNC_AV_UPDATE:
            pluginString="update state";
            break;
        case PHPFUNC_AV_CASSID:
            pluginString="get cassette ID";
            break;
        case PHPFUNC_AV_WAFID:
            pluginString="get wafer ID";
            break;
        case PHPFUNC_AV_PROBEID:
            pluginString="get Probe ID";
            break;
        case PHPFUNC_AV_LOTID:
            pluginString="get Lot ID";
            break;
        case PHPFUNC_AV_STARTLOT:
            pluginString="lot start";
            break;
        case PHPFUNC_AV_ENDLOT:
            pluginString="lot done";
            break;
        case PHPFUNC_AV_COMMTEST:
            pluginString="communication test";
            break;
        case PHPFUNC_AV_EXECGPIBCMD:
            pluginString="send setup and action command";
            break;
        case PHPFUNC_AV_EXECGPIBQUERY:
            pluginString="send prober query command";
            break;
        case PHPFUNC_AV_DESTROY:
            pluginString="destroy";
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
 *****************************************************************************/
static void updateSiteInfoSetPopulatedEmpty(
    phEventId_t eventID           /* event handler ID */
)
{
    phEstateError_t stateRtnValue=PHESTATE_ERR_OK;
    int selectedSiteCounter;
    int siteIndex;

    /* update copy of site info (eventID->siteUsage) so site all populated sites are set empty */
    for ( selectedSiteCounter=0 ; selectedSiteCounter < eventID->numOfSelectedSites ; selectedSiteCounter++ )
    {
        siteIndex = getSelectedSiteSiteIndex(eventID,selectedSiteCounter);
        eventID->siteUsage[siteIndex]=PHESTATE_SITE_EMPTY;
    }

    /* set new site information */
    stateRtnValue=phEstateASetSiteInfo(eventID->estateId, eventID->siteUsage, eventID->numOfSites);
}


/*****************************************************************************
 * updateSiteInfoSetPopulatedEmpty
 *****************************************************************************/
static void updateSiteInfoSetPopdeactDeactivated(
    phEventId_t eventID           /* event handler ID */
)
{
    phEstateError_t stateRtnValue=PHESTATE_ERR_OK;
    int selectedSiteCounter;
    int siteIndex;

    /* update copy of site info (eventID->siteUsage) so site all populated sites are set empty */
    for ( selectedSiteCounter=0 ; selectedSiteCounter < eventID->numOfSelectedSites ; selectedSiteCounter++ )
    {
        siteIndex = getSelectedSiteSiteIndex(eventID,selectedSiteCounter);
        eventID->siteUsage[siteIndex]=PHESTATE_SITE_DEACTIVATED;
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
                else if (flagType[flagTypeCounter].flag ==PHTCOM_SF_CI_BIN_NUMBER)
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
            addStringToMsgStr( eventID,"%s",lineString);
        }
    }
}


/*****************************************************************************
 * addSiteInfoToMessageString
 *****************************************************************************/
static void addSiteInfoToMessageString(
    phEventId_t eventID           /* event handler ID */,
    int index                     /* index to selectedSites array in event struct */
)
{
    char siteInfoString[PHEVENT_MAX_LINE_LENGTH];


    sprintf(siteInfoString,"%s[",eventID->selectedSites[index].siteName);

    switch (eventID->selectedSites[index].siteUsage)
    {
      case PHESTATE_SITE_POPULATED:
	strcat(siteInfoString,"P");
	break;
      case PHESTATE_SITE_EMPTY:
	strcat(siteInfoString,"E");
	break;
      case PHESTATE_SITE_POPDEACT:
	strcat(siteInfoString,"p");
	break;
      case PHESTATE_SITE_DEACTIVATED:
	strcat(siteInfoString,"D");
	break;
      case PHESTATE_SITE_POPREPROBED:
	strcat(siteInfoString,"R");
	break;
    }

    strcat(siteInfoString,"] ");
    addStringToMsgStr( eventID,"%s", siteInfoString );
}


/*****************************************************************************
 * addBinInfoToMessageString
 *****************************************************************************/
static void addBinInfoToMessageString(
    phEventId_t eventID           /* event handler ID */,
    int index                     /* index to selectedSites array in event struct */,
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
                      "addBinInfoToMessageString(P%p, %d, %d)",
                      eventID,
                      index,
                      binNum);

    sprintf(binInfoString,"bin device %s to bin ",eventID->selectedSites[index].siteName);
    addStringToMsgStr( eventID,"%s", binInfoString );

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
              "unable to determine '%s' list length \n",
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

        addStringToMsgStr( eventID,"%s", binString );
    }
    else
    {
        sprintf(binInfoString,"[%d] ",binNum);
        addStringToMsgStr( eventID,"%s", binInfoString );
    }
    addNewLineToMessageString(eventID);
}


/*****************************************************************************
 * toggleSiteUsageEmptyFull
 *****************************************************************************/
static void toggleSiteUsageEmptyFull(
    phEventId_t eventID           /* event handler ID */,
    int index                     /* index to selectedSites array in event struct */
)
{
    if ( eventID->selectedSites[index].siteUsage == PHESTATE_SITE_POPULATED )
        eventID->selectedSites[index].siteUsage=PHESTATE_SITE_EMPTY;
    else
        eventID->selectedSites[index].siteUsage=PHESTATE_SITE_POPULATED;

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
    phPFuncId_t driver            /* the driver plugin ID */,
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

    struct phEventStruct *tmpId;

    /* set eventID to NULL in case anything goes wrong */
    *eventID = NULL;

    phLogEventMessage(logger,
                      PHLOG_TYPE_TRACE,
                      "phEventInit(P%p, P%p, P%p, P%p, P%p, P%p, P%p, P%p)",
                      phEventInit,
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

    if ( rtnValue == PHEVENT_ERR_OK )
    {
        tmpId -> myself = tmpId;
        tmpId -> driverId = driver;
        tmpId -> communicationId = communication;
        tmpId -> loggerId = logger;
        tmpId -> configurationId = configuration;
        tmpId -> stateId = state;
        tmpId -> estateId = estate;
        tmpId -> testerId = tester;

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
		comment_out, *comlen,
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
            strcpy(dialogString,"Prober driver single step mode:\n");
            strcat(dialogString,"Do you want to...\n");
            strcat(dialogString,"QUIT the testprogram or\n");
            strcat(dialogString,"STEP perform the next driver action\n");
            strcat(dialogString,DumpMessage);
            strcat(dialogString,"CONTINUE with normal driver operation breaking out of single step mode.\n\n");
            strcat(dialogString,"Prober driver call and current settings for selected system flags:\n");
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
 * Calling phPFuncLoadCass(3), phPFuncUnloadCass(3),
 * phPFuncLoadWafer(3), phPFuncUnloadWafer(3), phPFuncGetDie(3), or
 * phPFuncGetSubDie(3) may result in a PHPFUNC_ERR_WAITING return
 * code, indicating that the requested operation has not been
 * completed yet. The Framework usually repeats the call until the
 * return code is different from PHPFUNC_ERR_WAITING.
 *
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
    phPFuncError_t funcError;
    phGuiError_t guiError;
    phTcomError_t tcomRtnValue=PHTCOM_ERR_OK;
 
    BOOLEAN onlyQuitButton = TRUE;
    BOOLEAN readDiagMsg = FALSE;
    const char *eventErrorGuiDescTemp = NULL;

    long value;
    BOOLEAN promptOperator=TRUE;  /* default action */
    int displayDialog;
    char emptyStr[] = "";
    char *diagnosticsString = NULL;
    phHookError_t hookError;

    char* descr;
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    const char* eventErrorTitle=NULL;
/* End of Huatek Modification */

    phGuiProcessId_t gui;
    char response[PHEVENT_MAX_REPONSE_LENGTH];
    char eventErrorText[PHEVENT_MAX_MESSAGE_LENGTH +1];

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
            eventErrorTitle="Prober/Handler: Prober not understood";
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
        if ( (phPlugPFuncAvailable() & PHPFUNC_AV_DIAG) && readDiagMsg)
        {
            funcError=phPlugPFuncDiag(eventID->driverId,
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
            sprintf(descr, eventErrorGuiDescTemp, eventErrorTitle, eventErrorText, getString(eventID));

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
                "Troubleshoot", "", "", "", "", "", &hookResponse, NULL);

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
                    "QUIT...    KEEP WAITING...",
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
                phLogEventMessage(eventID->loggerId, LOG_NORMAL,"User pressed 'KEEP WAITING...'");
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
	    "timeout occured while waiting for action to complete");
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
 *          14 Jan 2000, Ulrich Frank, changed popup
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

    if (testerError != PH_GUI_ERR_OK) return PHEVENT_ERR_GUI;

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
    phPFuncError_t *funcError      /* the simulated get start result */
)
{
    int selectedSiteCounter;
    char dialogString[PHEVENT_MAX_MESSAGE_LENGTH +1];
    char lineString[PHEVENT_MAX_LINE_LENGTH];
    int firstSite=0;            /* site associated with the first button 
                                   on dialog box for populate/empty toggle */
    int lastSite;               /* site associated with last button 
                                   on dialog box for populate/empty toggle */
    int maxNumSiteButtons=4;    /* maxium number of buttons on the dialog
                                   box for site populate/empty toggle */
    long response;
    const char *MoreButton;
    const char *MoreMessage;

    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventHandtestGetStart(P%p, P%p, P%p, P%p)", 
                      eventID,
                      result,
                      stReturn,
                      funcError);

    /* get all active site information */
    getAllSelectedSiteInfo(eventID,&selectActiveSites);

    if ( eventID->numOfSelectedSites > maxNumSiteButtons )
    {
        MoreButton="More";
        MoreMessage="or see\nMORE site information ";
    }
    else
    {
        MoreButton="";
        MoreMessage="";
    }

    firstSite=0;
    do
    {
        lastSite=firstSite+maxNumSiteButtons-1 ;

        if ( lastSite >= eventID->numOfSelectedSites )
        {
            lastSite=eventID->numOfSelectedSites-1;
        }

        /* initialize internal message string for site info details */
        initializeInternalString(eventID);

        for ( selectedSiteCounter=firstSite ; selectedSiteCounter <= lastSite ; selectedSiteCounter++ )
        {
            addSiteInfoToMessageString(eventID, selectedSiteCounter);
        }

        /* set-up populate/empty site line on dialog */
        if ( eventID->numOfSelectedSites == 0 )      /* no selected sites */
        {
            lineString[0]='\0';
        }
        else if ( firstSite == lastSite ) /* only one selected site */
        {
            sprintf(lineString,"\npopulate/empty site %s ",eventID->selectedSites[firstSite].siteName);
        }
        else                              /* more than one selected site */
        {
            sprintf(lineString,"\npopulate/empty sites %s - %s ",eventID->selectedSites[firstSite].siteName,
                                                                    eventID->selectedSites[lastSite].siteName);
        }

        strcpy(dialogString,"Prober driver hand test mode: get devices");
        strcat(dialogString,"\nDo you want to...");
        strcat(dialogString,"\nQUIT the testprogram or");
        strcat(dialogString,lineString);
        strcat(dialogString,MoreMessage);
        strcat(dialogString,"or send a\nTIMEOUT signal or\n");
        strcat(dialogString,"CONTINUE with the testprogram. \n\n");
        strcat(dialogString,"Active site information: \n");
        strcat(dialogString,"E-site empty, P-site populated \n");
        strcat(dialogString,getString(eventID));
    
        phEventPopup(eventID, result, stReturn, 1, "menu",
	    dialogString,
	    eventID->selectedSites[firstSite].siteName,
	    const_cast<char*>(( firstSite+1 < eventID->numOfSelectedSites ) ? eventID->selectedSites[firstSite+1].siteName : ""),
	    const_cast<char*>(( firstSite+2 < eventID->numOfSelectedSites ) ? eventID->selectedSites[firstSite+2].siteName : ""),
	    const_cast<char*>(( firstSite+3 < eventID->numOfSelectedSites ) ? eventID->selectedSites[firstSite+3].siteName : ""),
	    MoreButton,
	    "Timeout",
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
            case 2: /* firstSite */
                toggleSiteUsageEmptyFull(eventID, firstSite);
                break;
            case 3: /* firstSite+1 */
                toggleSiteUsageEmptyFull(eventID, firstSite+1);
                break;
            case 4: /* firstSite+2 */
                toggleSiteUsageEmptyFull(eventID, firstSite+2);
                break;
            case 5: /* firstSite+3 */
                toggleSiteUsageEmptyFull(eventID, firstSite+3);
                break;
            case 6: /* MORE */
                firstSite=lastSite+1;
                if ( firstSite >= eventID->numOfSelectedSites )
                {
                    firstSite=0;
                }
                break;
            case 7: /* TIMEOUT */
                *result=PHEVENT_RES_CONTINUE;
                *funcError=PHPFUNC_ERR_TIMEOUT;
                break;
            case 8: /* CONT */
                updateSiteInfo(eventID);

                *result=PHEVENT_RES_HANDLED;
                *funcError=PHPFUNC_ERR_OK;
                break;
        }
    } while (response != 1 && response != 7 && response != 8 );

    return PHEVENT_ERR_OK;    
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
    long *perSiteBinMap           /* bin map */,
    long *perSitePassed           /* pass/fail map */
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


    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventHandtestBinDevice(P%p, P%p, P%p, P%p)", 
                      eventID,
                      result,
                      stReturn,
                      perSiteBinMap);

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
            addBinInfoToMessageString(eventID,selectedSiteCounter,perSiteBinMap[siteCounter]);
        }

        strcpy(dialogString,"Prober driver hand test mode: bin devices");
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
	      updateSiteInfoSetPopdeactDeactivated(eventID);

	      getAllSelectedSiteInfo(eventID,&selectPopulatedSites);
	      updateSiteInfoSetPopulatedEmpty(eventID);

	      *result=PHEVENT_RES_HANDLED;
         /*   *funcError=PHPFUNC_ERR_OK;    */
                break;
        }
    } while (response != 1 && response != 7 && response != 8 );

    return PHEVENT_ERR_OK;    
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
    phPFuncError_t *funcError     /* the simulated get start result */
)
{
    char dialogString[PHEVENT_MAX_MESSAGE_LENGTH +1];
    long response;


    CheckSession(eventID);

    phLogEventMessage(eventID->loggerId,
                      PHLOG_TYPE_TRACE,
                      "phEventHandtestReprobe(P%p, P%p, P%p, P%p)", 
                      eventID,
                      result,
                      stReturn,
                      funcError);

    do
    {
        strcpy(dialogString,"Prober driver hand test mode: reprobe");
        strcat(dialogString,"\nDo you want to...");
        strcat(dialogString,"\nQUIT the testprogram");
        strcat(dialogString,"or \nCONTINUE with the testprogram. \n\n");
    
        phEventPopup(eventID, result, stReturn, 1, "menu",
	    dialogString,
	    "",
	    "",
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

		    /* accept the quit flag, but return with PASS to
		       avoid an error message from the application
		       model. */

                    *stReturn=PHTCOM_CI_CALL_PASS;
                }
                break;
            case 8: /* CONT */
                *result=PHEVENT_RES_HANDLED;
                *funcError=PHPFUNC_ERR_OK;
                break;
        }
    } while (response != 1 && response != 8 );

    return PHEVENT_ERR_OK;    
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
