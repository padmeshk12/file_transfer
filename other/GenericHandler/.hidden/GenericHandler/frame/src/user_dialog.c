/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : user_dialog.c
 * CREATED  : 9 May 2001
 *
 * CONTENTS : User Dialog Management
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 9 Mai 2001, Chris Joyce, created
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

/*--- module includes -------------------------------------------------------*/

#include "ci_types.h"

#include "ph_tools.h"
#include "user_dialog.h" 
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define USERDIAG_FREQ_NEVER        "never"
#define USERDIAG_FREQ_ONCE         "once"
#define USERDIAG_FREQ_REPEAT       "repeat"

#define USERDIAG_FORMAT_QUIT_CONT    "Quit-Cont"
#define USERDIAG_FORMAT_CONT         "Cont"



#define PH_SESSION_CHECK

#ifdef DEBUG
#define PH_SESSION_CHECK
#endif

#ifdef PH_SESSION_CHECK
#define CheckSession(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHUSERDIALOG_ERR_INVALID_HDL
#else
#define CheckSession(x)
#endif

/*--- typedefs --------------------------------------------------------------*/

typedef enum {
    ud_format_unknown,
    ud_format_quit_cont,
    ud_format_cont
} phUserDialogFormat_t;


struct userDialog
{
    struct userDialog *myself;               /* self reference for validity
                                                check */
    /* id handles needed */
    phLogId_t                      logId;
    phConfId_t                     specificConfId;
    phTcomId_t                     testerId;
    phEventId_t                    eventId;

    /* internal variables */
    const char                     *config_str;
    const char                     *display_title;
    const char                     *display_str;
    phUserDialogFormat_t           format;
    phUserDialogFreq_t             expected_freq;
    phUserDialogFreq_t             freq;
    int                            dialogDisplayed;
};


/*--- functions -------------------------------------------------------------*/
static phUserDialogError_t setupConfigUserDialog(phUserDialogId_t handle);

static char *convertConfigMsgIntoDialogMsg(const char* str)
{
    char *msg = strdup(str);

    char *pipeChar = strchr(msg,'|');

    while ( pipeChar )
    {
        *pipeChar='\n';

        pipeChar = strchr(pipeChar,'|');
    }

    return msg;
}

static phUserDialogFormat_t convertDialogFormatStrIntoType(const char* str)
{
    phUserDialogFormat_t  format=ud_format_unknown;

    if ( strcasecmp(str,USERDIAG_FORMAT_QUIT_CONT) == 0 )
        format=ud_format_quit_cont;
    else if ( strcasecmp(str,USERDIAG_FORMAT_CONT) == 0 )
        format=ud_format_cont;

    return format;
}

static phUserDialogFreq_t convertDialogFreqStrIntoType(const char* str)
{
    phUserDialogFreq_t  freq=ud_freq_unknown;

    if ( strcasecmp(str,USERDIAG_FREQ_NEVER) == 0 )
        freq=ud_freq_never;
    else if ( strcasecmp(str,USERDIAG_FREQ_ONCE) == 0 )
        freq=ud_freq_once;
    else if ( strcasecmp(str,USERDIAG_FREQ_REPEAT) == 0 )
        freq=ud_freq_repeatedly;

    return freq;
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
    phUserDialogId_t handle             /* user dialog data */
)
{
    phEventResult_t result;
    phTcomUserReturn_t stReturn;
    long response = 0;

    phLogFrameMessage(handle->logId,
                      PHLOG_TYPE_TRACE,
                      "confirmationDialog()");

    phEventPopup(handle->eventId, &result, &stReturn, 1, "menu",
        "Please confirm: \n"
        "Do you really want to... \n\n"
        "QUIT the testprogram or\n"
        "CONTINUE where you were.",
        "", "", "", "", "", "",
        &response, NULL);

    return (response == 1 || result==PHEVENT_RES_ABORT);
}

/*****************************************************************************
 *
 * Display User Dialog Quit-Cont
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 * Description: Display Quit-Cont user dialog.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
static phUserDialogError_t displayUserDialogQuitCont(
    phUserDialogId_t handle             /* user dialog data */)
{
    phEventResult_t result;
    phTcomUserReturn_t stReturn;
    long response;

    do
    {
        response=8;
        phEventPopup(handle->eventId, &result, &stReturn, 1, handle->display_title,
            handle->display_str,
            "", "", "", "", "", "", &response, NULL);

        if (result == PHEVENT_RES_ABORT)
            /* this is not a correct handling but better
               than nothing here */
            return PHUSERDIALOG_ERR_GUI;

        if ( response == 1 )
        {
            response=confirmationDialog(handle);
            if ( response == 1 )
            {
                phTcomSetSystemFlag(handle->testerId, PHTCOM_SF_CI_ABORT, 1);
            }
        }

    } while (response != 1 && response != 8);

    return PHUSERDIALOG_ERR_OK;
}


/*****************************************************************************
 *
 * Display User Dialog Quit-Cont
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 * Description: Display Quit-Cont user dialog.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
static phUserDialogError_t displayUserDialogCont(
    phUserDialogId_t handle             /* user dialog data */)
{
    phEventResult_t result;
    phTcomUserReturn_t stReturn;
    long response;

    do
    {
        response=2;
        phEventPopup(handle->eventId, &result, &stReturn, 0, handle->display_title,
            handle->display_str,
            "Continue", "", "", "", "", "", &response, NULL);

        if (result == PHEVENT_RES_ABORT)
            /* this is not a correct handling but better
               than nothing here */
            return PHUSERDIALOG_ERR_GUI;

    } while (response != 2 );

    return PHUSERDIALOG_ERR_OK;
}


/*****************************************************************************
 *
 * Initialize user dialog internal variables
 *
 * Authors: Chris Joyce
 *
 * Description: Called once during init for each defined phUserDialogId_t
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
phUserDialogError_t initUserDialog(
    phUserDialogId_t *handle     /* result user dialog ID */,
    phLogId_t logger             /* the data logger to be used */,
    phConfId_t configuration     /* the configuration manager */,
    phTcomId_t tester            /* the tester communication */,
    phEventId_t event            /* event handler */,
    const char* c                /* config key value */,
    const char* default_title    /* default dialog title */,
    const char* default_msg      /* default dialog message */,
    phUserDialogFreq_t exf       /* expected frequency of Dialog */
)
{
    phUserDialogError_t rtnValue=PHUSERDIALOG_ERR_OK;
    struct userDialog *tmpId;

    /* set handle to NULL in case anything goes wrong */
    *handle=NULL;

    tmpId = PhNew(struct userDialog);
    if (tmpId == 0)
    {
        phLogEventMessage(logger,
                          PHLOG_TYPE_FATAL,
                          "not enough memory during call to initUserDialog()");
        rtnValue=PHUSERDIALOG_ERR_MEMORY;
    }

    if ( rtnValue == PHUSERDIALOG_ERR_OK )
    {
        tmpId -> myself = tmpId;

        tmpId -> logId = logger;
        tmpId -> specificConfId = configuration;
        tmpId -> testerId = tester;
        tmpId -> eventId = event;
        tmpId ->config_str       = c;             /* configuration key value */ 
        tmpId ->display_title    = default_title; /* default dialog box display 
                                                    title */
        tmpId ->display_str      = default_msg;   /* default dialog box display 
                                                    message */
        tmpId ->format           = ud_format_quit_cont; /* default format 
                                                          quit cont buttons */
        tmpId ->expected_freq    = exf;           /* expected frequency for this 
                                                    dialog box either once or
                                                    unknown */
        tmpId ->freq             = ud_freq_once; /* default frequency once */
        tmpId ->dialogDisplayed  = 0; /* dialog has not yet been displayed */

        *handle = tmpId;
    }

    if ( rtnValue == PHUSERDIALOG_ERR_OK )
    {
        rtnValue=setupConfigUserDialog(*handle);
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Setup configuration values
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 * Description: Setup internal values according to settings in config file
 * Must be called once for each defined phUserDialogId_t.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
static phUserDialogError_t setupConfigUserDialog(
    phUserDialogId_t handle             /* user dialog data */)
{
/* Begin of Huatek Modification, Chris Wang, 06/14/2002 */
/* Issue Number: 315 */
    phUserDialogError_t rtnValue=PHUSERDIALOG_ERR_OK;
    phConfError_t confError;
    int listLength;
    phConfType_t defType;
    const char *cstr;
    int i;
/* End of Huatek Modification */

    CheckSession(handle);

    /* 
     *  First check to see if configuration has been defined and right
     *  number of strings are present.
     */

    confError = phConfConfIfDef(handle->specificConfId, handle->config_str);
    if ( confError != PHCONF_ERR_OK )
    {
        handle->freq=ud_freq_never;
    }
    else
    {
        listLength = 0;
        confError = phConfConfType(handle->specificConfId,
            handle->config_str, 0, NULL, &defType, &listLength);

        if (confError != PHCONF_ERR_OK)
        {
            rtnValue=PHUSERDIALOG_ERR_CONF;
        }
        else
        {
            if ( defType != PHCONF_TYPE_LIST || listLength == 0 || listLength > 4)
            {
                rtnValue=PHUSERDIALOG_ERR_CONF;
            }
            else
            {
                for (i=0; i<listLength && handle->freq!=ud_freq_never ; i++)
                {
                    phConfConfString(handle->specificConfId,
                                     handle->config_str, 1, &i, &cstr);
                    switch ( i )
                    {
                      case 0: /* user dialog title */
                        if ( strcasecmp(cstr,"none") == 0 || strlen(cstr) == 0 )
                        {
                            handle->display_title=" ";
                        }
                        else if ( strcasecmp(cstr,"standard") != 0 )
                        {
                            handle->display_title=cstr;
                        }
                        break; 
                      case 1: /* user dialog message */
                        if ( strcasecmp(cstr,"none") == 0 || strlen(cstr) == 0 )
                        {
                            handle->display_str=" ";
                        }
                        else if ( strcasecmp(cstr,"standard") != 0 )
                        {
                            handle->display_str=convertConfigMsgIntoDialogMsg(cstr);
                        }

                        if ( strcmp(handle->display_title," ") == 0 &&
                             strcmp(handle->display_str," ") == 0 )
                        {
                            handle->freq=ud_freq_never;
                        }
                        break; 
                      case 2: /* user dialog buttons format */
                        handle->format=convertDialogFormatStrIntoType(cstr);
                        if ( handle->format == ud_format_unknown )
                        {
                            rtnValue=PHUSERDIALOG_ERR_CONF;
                            handle->format=ud_format_quit_cont;
                        }
                        break; 
                      case 3: /* user dialog frequency */
                        handle->freq=convertDialogFreqStrIntoType(cstr);
                        if ( handle->freq == ud_freq_unknown )
                        {
                            rtnValue=PHUSERDIALOG_ERR_CONF;
                            handle->freq=ud_freq_once;
                        }
                        else if ( handle->freq==ud_freq_repeatedly && handle->expected_freq==ud_freq_once )
                        {
                            rtnValue=PHUSERDIALOG_ERR_CONF;
                            handle->freq=ud_freq_once;
                        }
                        break; 
                    }
                }
            }
        }
    }

    if ( rtnValue==PHUSERDIALOG_ERR_CONF )
    {
        if ( handle->expected_freq==ud_freq_once )
        {
            phLogFrameMessage(handle->logId, PHLOG_TYPE_ERROR,
                    "configuration '%s' of wrong type, expected format [\"title\" \"message\" \"diag-format\" \"diag-freq\"]\n"
                    "diag-format expected to be one of \"%s\" \"%s\"\n"
                    "diag-freq expected to be one of \"%s\" \"%s\"\n",
                    handle->config_str,
                    USERDIAG_FORMAT_QUIT_CONT,
                    USERDIAG_FORMAT_CONT,
                    USERDIAG_FREQ_NEVER,
                    USERDIAG_FREQ_ONCE);
        }
        else
        {
            phLogFrameMessage(handle->logId, PHLOG_TYPE_ERROR,
                    "configuration '%s' of wrong type, expected format [\"title\" \"message\" \"diag-format\" \"diag-freq\"]\n"
                    "diag-format expected to be one of \"%s\" \"%s\"\n"
                    "diag-freq expected to be one of \"%s\" \"%s\" \"%s\"\n",
                    handle->config_str,
                    USERDIAG_FORMAT_QUIT_CONT,
                    USERDIAG_FORMAT_CONT,
                    USERDIAG_FREQ_NEVER,
                    USERDIAG_FREQ_ONCE,
                    USERDIAG_FREQ_REPEAT);
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Display User Dialog
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 * Description: Check internal values and display user dialog when 
 *              appropriate.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
phUserDialogError_t displayUserDialog(
    phUserDialogId_t handle             /* user dialog data */)
{
    phUserDialogError_t rtnValue=PHUSERDIALOG_ERR_OK;
    long abortFlag = 0;
    long resetFlag = 0;
    long quitFlag = 0;
    long pauseFlag = 0;

    CheckSession(handle);

    if ( ( handle->freq==ud_freq_once && !handle->dialogDisplayed ) ||
           handle->freq==ud_freq_repeatedly )
    {
        /* get the current flag situation. The operator may have set some
           flag */
        phTcomGetSystemFlag(handle->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
        phTcomGetSystemFlag(handle->testerId, PHTCOM_SF_CI_RESET, &resetFlag);
        phTcomGetSystemFlag(handle->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
        phTcomGetSystemFlag(handle->testerId, PHTCOM_SF_CI_PAUSE, &pauseFlag);

        /* check no flags set before displaying user dialog */
        if ( !abortFlag && !resetFlag && !quitFlag && !pauseFlag )
        {
            switch( handle->format )
            {
              case ud_format_quit_cont:
                rtnValue=displayUserDialogQuitCont(handle);
                break;
              case ud_format_cont:
                rtnValue=displayUserDialogCont(handle);
                break;
              default:
                rtnValue=PHUSERDIALOG_ERR_INTERNAL;
                break;
            }
            handle->dialogDisplayed=1;
        }
    }

    return rtnValue;
}

/*Begin of Huatek Modifications, Donnie Tu, 04/24/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free user dialog internal variables
 *
 * Authors: Donnie Tu
 *
 * Description:
 *
 * Returns: None
 *
 ***************************************************************************/
void freeUserDialog( phUserDialogId_t *handle)
{
   if(handle && *handle)
   {
        free(*handle);
        *handle=NULL;
   }
}
/*End of Huatek Modifications*/


/*****************************************************************************
 * End of file
 *****************************************************************************/
