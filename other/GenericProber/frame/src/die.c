/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : die.c
 * CREATED  : 17 Dec 1999
 *
 * CONTENTS : die and sub-die handling
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Dec 1999, Michael Vogt, created
 *            10 Apr 2001, logging of populated sites inserted setSiteSetup()
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
#include <errno.h>
#include <limits.h>

/*--- module includes -------------------------------------------------------*/

#include "ci_types.h"

#include "ph_tools.h"
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "stepcontrol.h"
#include "binner.h"
#include "clean.h"
#include "exception.h"
#include "die.h"
#include "helpers.h"
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
#include "clean.h"
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modification */

/*--- defines ---------------------------------------------------------------*/

/* #undef ALLOW_PAUSED_BINNING */
#define ALLOW_PAUSED_BINNING

/*--- typedefs --------------------------------------------------------------*/
static int binningNumberExistsInMap = 0;

typedef enum {
    PHFRAMESTEP_ERR_OK,
    PHFRAMESTEP_ERR_PC,
    PHFRAMESTEP_ERR_ERROR,
    PHFRAMESTEP_ERR_PAUSED
} stepActionError_t;

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * covert the bin code to number
 *
 * Authors: Adam Huang
 *
 * Description:
 * Performs convertion
 *
 * Returns: 0 on success, -1 on failure
 *
 ***************************************************************************/
static int softBinCodeToBinNumber(struct phFrameStruct *f, char * stBinCode, int * binNumber)
{
    char * endptr;
    long val;

    errno = 0;
    val = strtol(stBinCode, &endptr, 10);

    /* Check for various possible errors */
    /* the bin code is out of range or an error happened in the convertion or there is no number in bin code */
    if ( (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0) || (endptr == stBinCode))
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "SmarTest bin code \"%s\" of current die cannot be parsed a number", stBinCode);
        return -1;
    }

    /* If we got here, strtol() successfully parsed a number */
    if (*endptr != '\0')
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "SmarTest bin code \"%s\" of current die be parsed a number \"%d\", there may be some code is lost", stBinCode, val);
    else
        phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3, "SmarTest bin code \"%s\" of current die successfully be parsed to a number \"%ld\"", stBinCode, val);
    *binNumber = val;
    return 0;
}

/* 
 *  Convert from exception handling understood exceptionActionError_t to 
 *  this modules understood stepActionError_t.
 */
static stepActionError_t convertExceptionActionToStepAction(exceptionActionError_t exceptionAction)
{
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    stepActionError_t  stepAction=PHFRAMESTEP_ERR_OK;
/* End of Huatek Modification */

    switch ( exceptionAction )
    {
      case PHFRAME_EXCEPTION_ERR_OK:
        stepAction=PHFRAMESTEP_ERR_OK;
        break;
      case PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE:
        stepAction=PHFRAMESTEP_ERR_PC;
        break;
      case PHFRAME_EXCEPTION_ERR_ERROR:
        stepAction=PHFRAMESTEP_ERR_ERROR;
        break;
      case PHFRAME_EXCEPTION_ERR_PAUSED:
        stepAction=PHFRAMESTEP_ERR_PAUSED;
        break;
    }

    return stepAction;
}


static void printPetriStatus(struct phFrameStruct *f)
{
    const char *markNames[] = {
        "inactive",
        "waiting for start",
        "waiting for start (paused)",
        "active",
        "waiting for stop",
        "waiting for stop (paused)",
        "snooping for next position",
        "ahead to step to next position",
        "pattern completed"
    };

    phLogFrameMessage(f->logId, LOG_NORMAL,
        "die level is in state \"%s\"", markNames[f->d_sp.mark]);
    if (f->isSubdieProbing)
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "sub-die level is in state \"%s\"", markNames[f->sd_sp.mark]);
}

static stepActionError_t doNextDie(
    struct phFrameStruct *f, 
    phStepPattern_t *sp,
    phTcomUserReturn_t *pRetVal
)
{
    stepActionError_t retVal = PHFRAMESTEP_ERR_OK;
    exceptionActionError_t exceptionAction=PHFRAME_EXCEPTION_ERR_OK;

    int isSubDie = 0;
    char dieString[8];

    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;
    phEventResult_t exceptionResult = PHEVENT_RES_VOID;
    long dieX = -1;
    long dieY = -1;

/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int paused;*/
    phStateDrvMode_t driverMode;

/*    long pauseFlag;*/
/* End of Huatek Modification */
    

    /* find out whether we are working on a die or sub-die level here */
    isSubDie = (sp == &(f->sd_sp));
    if (isSubDie)
        strcpy(dieString, "sub-die");
    else
        strcpy(dieString, "die");

    /* If working in explicit mode, get die location from stepcontrol */
    switch (sp->stepMode)
    {
      case PHFRAME_STEPMD_EXPLICIT:
        dieX = sp->xStepList[sp->pos];
        dieY = sp->yStepList[sp->pos];
        phLogFrameMessage(f->logId, LOG_DEBUG,
            "probing explicit %s [%ld, %ld]", dieString, dieX, dieY);
        break;
      case PHFRAME_STEPMD_AUTO:
        phLogFrameMessage(f->logId, LOG_DEBUG,
            "performing automatic %s step", dieString);
        break;
      case PHFRAME_STEPMD_LEARN:
        phLogFrameMessage(f->logId, LOG_DEBUG,
            "performing learned %s step", dieString);
        break;
    }

    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        phStateGetDrvMode(f->stateId, &driverMode);
        if (driverMode == PHSTATE_DRV_HAND || 
            (driverMode == PHSTATE_DRV_SST_HAND))
        {
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "waiting for die(s) in hand test mode");
            funcError = PHPFUNC_ERR_TIMEOUT;
            phEventHandtestGetStart(f->eventId,
                &exceptionResult, pRetVal, &funcError);
            switch (exceptionResult)
            {
              case PHEVENT_RES_CONTINUE:
              case PHEVENT_RES_HANDLED:
                /* Get parts has been simulated, go on with
                   success, timeout, etc... 
                   funcError was set accordingly */
                break;
              case PHEVENT_RES_ABORT:
                /* testprogram aborted by operator, QUIT flag has
                   been set, return to test cell client with
                   suggest value */
                phLogFrameMessage(f->logId, PHLOG_TYPE_FATAL,
                    "aborting handler driver");
                return PHFRAMESTEP_ERR_ERROR;
              default:
                /* not expected, this is a bug, print an error and
                   try to recover during the next iteration */
                phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "unexpected return value (%ld) from handtest GetParts",
                    (long) exceptionResult);
                funcError = PHPFUNC_ERR_TIMEOUT;
                break;
            }
        }
        else
        {
            /* this is the regular case !!!!!!! */
            funcError = isSubDie ?
                phPlugPFuncGetSubDie(f->funcId, &dieX, &dieY) :
                phPlugPFuncGetDie(f->funcId, &dieX, &dieY);
        }

        /* analyze result from get (sub) die call and take action */
        phFrameHandlePluginResult(f, funcError, 
                                  isSubDie ? PHPFUNC_AV_GETSUBDIE : PHPFUNC_AV_GETDIE,
                                  &completed, &success, pRetVal, &exceptionAction);
        retVal=convertExceptionActionToStepAction(exceptionAction);

        if ( success )
        {
            /* die has been probed, everything went fine */
            sp->currentX = dieX;
            sp->currentY = dieY;
        }
    } 

    return retVal;
}

static stepActionError_t doBinDie(
    struct phFrameStruct *f, 
    phStepPattern_t *sp,
    phTcomUserReturn_t *pRetVal
)
{
    stepActionError_t retVal = PHFRAMESTEP_ERR_OK;
    exceptionActionError_t exceptionAction=PHFRAME_EXCEPTION_ERR_OK;

    int isSubDie = 0;
    char dieString[8];

    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;
    phEventResult_t exceptionResult = PHEVENT_RES_VOID;
    phStateDrvMode_t driverMode;

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    long *binMap=NULL;
    long *passMap=NULL;
/* End of Huatek Modification */

/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int paused;*/

/*    long pauseFlag;*/
/* End of Huatek Modification */

    /* find out whether we are working on a die or sub-die level here */
    isSubDie = (sp == &(f->sd_sp));
    if (isSubDie)
        strcpy(dieString, "sub-die");
    else
        strcpy(dieString, "die");

    /* select the prepared bin data. Only take it from the framework,
       if we are doing sub-die probing and this is the sub-die bin
       action or if we are doing no sub-die probing and this is the
       die bin action */
    if ((isSubDie && f->isSubdieProbing) ||
        (!isSubDie && !f->isSubdieProbing))
    {
        switch (f->binAction)
        {
          case PHFRAME_BINACT_SKIPBIN:
            binMap = NULL;
            passMap = NULL;
            break;
          case PHFRAME_BINACT_DOBIN:
            binMap = f->dieBins;
            passMap = f->diePassed;
            break;
        }
    }
    else
    {
        binMap = NULL;
        passMap = NULL;
    }

    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        phStateGetDrvMode(f->stateId, &driverMode);
        if (driverMode == PHSTATE_DRV_HAND || 
            (driverMode == PHSTATE_DRV_SST_HAND))
        {
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "binning die(s) in hand test mode");
            if (binMap != NULL)
            {
                phEventHandtestBinDevice(f->eventId,
                    &exceptionResult, pRetVal, binMap, passMap);
                switch (exceptionResult)
                {
                  case PHEVENT_RES_HANDLED:
                    /* Bin devices has been simulated, go on with
                       success, etc... funcError was set accordingly */
                    funcError = PHPFUNC_ERR_OK;
                    break;
                  case PHEVENT_RES_ABORT:
                    /* testprogram aborted by operator, QUIT flag has
                       been set, return to test cell client with
                       suggest value */
                    phLogFrameMessage(f->logId, PHLOG_TYPE_FATAL,
                        "aborting prober driver");
                    return PHFRAMESTEP_ERR_ERROR;
                  default:
                    /* not expected, this is a bug, print an error and
                           try to recover during the next iteration */
                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "unexpected return value (%ld) from handtest BinDevice",
                        (long) exceptionResult);
                    funcError = PHPFUNC_ERR_BINNING;
                    break;
                }
            }
            else
            {
                phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "Null binMap");
                funcError = PHPFUNC_ERR_BINNING;
            }
        }
        else
        {
            /* this is the regular case !!!!!!! */
            funcError = isSubDie ?
                phPlugPFuncBinSubDie(f->funcId, binMap, passMap) :
                phPlugPFuncBinDie(f->funcId, binMap, passMap);
        }

        /* analyze result from bin (sub) die call and take action */
        phFrameHandlePluginResult(f, funcError, 
                                  isSubDie ? PHPFUNC_AV_BINSUBDIE : PHPFUNC_AV_BINDIE,
                                  &completed, &success, pRetVal, &exceptionAction);
        retVal=convertExceptionActionToStepAction(exceptionAction);

    } 
    return retVal;
}

static stepActionError_t doSnoop(
    struct phFrameStruct *f, 
    phStepPattern_t *sp
)
{
    stepActionError_t retVal = PHFRAMESTEP_ERR_OK;

    switch (sp->stepMode)
    {
      case PHFRAME_STEPMD_EXPLICIT:
        /* we control the stepping, so we can look it up */
        if (!phFrameStepSnoopStep(sp, f))
            retVal = PHFRAMESTEP_ERR_PC;
        break;
      case PHFRAME_STEPMD_AUTO:
      case PHFRAME_STEPMD_LEARN:
        /* we don't know yet, whether the prober will provide another die..*/
        retVal = PHFRAMESTEP_ERR_OK;
        break;
    }

    return retVal;
}

static stepActionError_t doStep(
    struct phFrameStruct *f, 
    phStepPattern_t *sp
)
{
    stepActionError_t retVal = PHFRAMESTEP_ERR_OK;

    switch (sp->stepMode)
    {
      case PHFRAME_STEPMD_EXPLICIT:
        /* we control the stepping, so do it */
        if (!phFrameStepDoStep(sp, f))
            retVal = PHFRAMESTEP_ERR_PC;
        break;
      case PHFRAME_STEPMD_AUTO:
      case PHFRAME_STEPMD_LEARN:
        /* we don't know yet, whether the prober will provide another die..*/
        retVal = PHFRAMESTEP_ERR_OK;
        break;
    }

    return retVal;
}

static stepActionError_t doReset(
    struct phFrameStruct *f, 
    phStepPattern_t *sp
)
{
    phFrameStepResetPattern(sp);
    return PHFRAMESTEP_ERR_OK;
}

/* Since die and/or sub-die handling may be necessary during both
 * die_start and die_done requests, a petri network is used to model the
 * correct calls of th eplugin. The reason for this is basically, that
 * the plugin may return a PATTERN COMPLETE error code on either of the
 * following calls: getDie, getSubDie, binDie, binSubDie. The framework
 * does not know what to expect in advance and it may be necessary to
 * step to the next die, if we are currently running out of sub-dies
 * (etc.).
 * 
 * The following petri network is used to handle and store all the
 * different cases available within one level (die or sub-die):
 * 
 * Mark in State:               at   DieStart   DieDone   additional Condition
 * 
 * OUT:   outside level              S/E        E         all nested levels OUT
 * WS:    wait for die start                              all nested levels OUT
 * ACT:   processing die             S/E        S/E       
 * WD:    wait for die done                               all nested levels OUT
 * STEP:  do step                                         all nested levels OUT
 * HALT:  halt before next die       S          E         all nested levels OUT
 * END:   end level                                       all nested levels OUT
 * 
 * 
 * Transitions:
 * 
 * State                       Action         Result      next State
 * 
 * OUT & Start from higher l.  nop()          -           WS >
 * 
 * WS                          nextDie()      OK          ACT & Start lower l.
 * WS                          nextDie()      PC          END >
 * WS                          nextDie()      PAUSED      WSP
 * WS                          nextDie()      ERROR       WS
 *
 * WSP                         nop()          -           WS
 * 
 * ACT & Stop from lower l.    nop()          -           WD >
 * 
 * WD                          binDie()       OK          SNOOP >
 * WD                          binDie()       PC          END >
 * WD                          binDie()       PAUSED      WDP
 * WD                          binDie()       ERROR       WD
 * 
 * WDP                         nop()          -           WD
 * 
 * SNOOP                       snoop()        OK          STEP
 * SNOOP                       snoop()        PC          END >
 * 
 * STEP                        step()         -           WS >
 * 
 * END                         reset()        -           OUT & Stop upper l.
 */

static int runDieStepper(
    struct phFrameStruct *f, 
    phStepPattern_t *sp,
    phTcomUserReturn_t *pRetVal
)
{
    int output_mark = 0;
    int done_mark;
    int stop = 0;

    if (!sp)
    {
        /* end of recursion, (below deepest level), return done or
           active based on current overall action */
        return f->currentAMCall == PHFRAME_ACT_DIE_START ? 0 : 1;
    }

    /* the stepping level exists, run the petri network */
    do 
    {
#ifdef DEBUG
        printPetriStatus(f);
#endif
        switch (sp->mark)
        {
          case PHFRAME_STMARK_OUT:
            sp->mark = PHFRAME_STMARK_WS;
            break;
          case PHFRAME_STMARK_WS:
            switch (doNextDie(f, sp, pRetVal))
            {
              case PHFRAMESTEP_ERR_OK:
                sp->mark = PHFRAME_STMARK_ACT;
                break;
              case PHFRAMESTEP_ERR_PC:
                sp->mark = PHFRAME_STMARK_END;
                break;
              case PHFRAMESTEP_ERR_PAUSED:
                sp->mark = PHFRAME_STMARK_WSP;
                stop = 1;
                break;
              case PHFRAMESTEP_ERR_ERROR:
                stop = 1;
                break;
            }
            break;
          case PHFRAME_STMARK_WSP:
            sp->mark = PHFRAME_STMARK_WS;
            break;
          case PHFRAME_STMARK_ACT:
            done_mark = runDieStepper(f, sp->next, pRetVal);
            if (done_mark)
                sp->mark = PHFRAME_STMARK_WD;
            else
                stop = 1;
            break;
          case PHFRAME_STMARK_WD:
            switch (doBinDie(f, sp, pRetVal))
            {
              case PHFRAMESTEP_ERR_OK:
                sp->mark = PHFRAME_STMARK_SNOOP;
                break;
              case PHFRAMESTEP_ERR_PC:
                sp->mark = PHFRAME_STMARK_END;
                break;
#ifdef ALLOW_PAUSED_BINNING
              case PHFRAMESTEP_ERR_PAUSED:
                sp->mark = PHFRAME_STMARK_WDP;
                stop = 1;
                break;
#endif
              case PHFRAMESTEP_ERR_ERROR:
                stop = 1;
                break;
            }
            break;
          case PHFRAME_STMARK_WDP:
            sp->mark = PHFRAME_STMARK_WD;
            break;
          case PHFRAME_STMARK_SNOOP:
            if (doSnoop(f, sp) == PHFRAMESTEP_ERR_OK)
            {
                sp->mark = PHFRAME_STMARK_STEP; 
                stop = 1;
            }
            else
                sp->mark = PHFRAME_STMARK_END;
            break;
          case PHFRAME_STMARK_STEP:
            if (doStep(f, sp) == PHFRAMESTEP_ERR_OK)
                sp->mark = PHFRAME_STMARK_WS;
            else
                sp->mark = PHFRAME_STMARK_END;
            break;
          case PHFRAME_STMARK_END:
            doReset(f, sp);
            sp->mark = PHFRAME_STMARK_OUT; 
            stop = 1;
            output_mark = 1;
            break;
        }
#ifdef DEBUG
        printPetriStatus(f);
#endif
    } while (!stop);
    return output_mark;
}

static void logBinDataOfSite(
    struct phFrameStruct *f             /* the framework data */,
    int site,
    char *stBinCode,
    long stBinNumber,
    phEstateSiteUsage_t population
)
{
    int bin = (int) f->dieBins[site]; /* needs to be type int */
    const char *proberSiteName = NULL;
    const char *proberBinName = NULL;
    phConfType_t defType;
    int length;

    phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &site, 
        &proberSiteName);

    /* first handle case of empty / not tested dies */
    if (population == PHESTATE_SITE_EMPTY || population == PHESTATE_SITE_DEACTIVATED)
    {
        phLogFrameMessage(f->logId, LOG_DEBUG,
            "prober site \"%s\" (SmarTest site %ld) currently not used, not binned", 
            proberSiteName, f->stToProberSiteMap[site]);
        return;
    }

    if (population == PHESTATE_SITE_POPREPROBED)
    {
        phLogFrameMessage(f->logId, LOG_DEBUG,
            "prober site \"%s\" (SmarTest site %ld) was tested earlier, not binned", 
            proberSiteName, f->stToProberSiteMap[site]);
        return;
    }

    /* get bin name from configuration, if defined */
    proberBinName = NULL;
    if (phConfConfIfDef(f->specificConfId, PHKEY_BI_HBIDS) == PHCONF_ERR_OK)
    {
        phConfConfType(f->specificConfId, PHKEY_BI_HBIDS, 0, NULL, &defType, &length);
        if(f->binPartialMode == 0)
        {
            /* partial binning mode disabled */
            if (bin >= 0 && bin < length)
                phConfConfString(f->specificConfId, PHKEY_BI_HBIDS, 1, &bin, &proberBinName);
        }
        else
        {
            if(binningNumberExistsInMap == 1)
                phConfConfString(f->specificConfId, PHKEY_BI_HBIDS, 1, &bin, &proberBinName);
        }
    }

    /* write the log info */
    if (f->binMode == PHBIN_MODE_DEFAULT)
    {
        phLogFrameMessage(f->logId, LOG_DEBUG,
                "will bin \"%s\" die at prober site \"%s\" "
                "(SmarTest site %ld, hardbin %ld) to prober bin %ld",
                f->diePassed[site] ? "good" : "bad",
                proberSiteName, f->stToProberSiteMap[site], stBinNumber, f->dieBins[site]);
    }
    else if (f->binMode == PHBIN_MODE_DEFAULT_SOFTBIN)
    {
        phLogFrameMessage(f->logId, LOG_DEBUG,
                "will bin \"%s\" die at prober site \"%s\" "
                "(SmarTest site %ld, softbin \"%s\") to prober bin %ld",
                f->diePassed[site] ? "good" : "bad",
                proberSiteName, f->stToProberSiteMap[site], stBinCode, f->dieBins[site]);
    }
    else if (f->binMode == PHBIN_MODE_HARDMAP)
    {
        if (proberBinName)
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "will bin \"%s\" die at prober site \"%s\" "
                "(SmarTest site %ld, hardbin %ld) to prober bin \"%s\"",
                f->diePassed[site] ? "good" : "bad",
                proberSiteName, f->stToProberSiteMap[site], stBinNumber, proberBinName);
        else
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "will bin \"%s\" die at prober site \"%s\" "
                "(SmarTest site %ld, hardbin %ld) to prober bin %ld",
                f->diePassed[site] ? "good" : "bad",
                proberSiteName, f->stToProberSiteMap[site], stBinNumber, f->dieBins[site]);
    }
    else /* PHBIN_MODE_SOFTMAP */
    {
        if (proberBinName)
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "will bin \"%s\" die at prober site \"%s\" "
                "(SmarTest site %ld, softbin \"%s\") to prober bin \"%s\"", 
                f->diePassed[site] ? "good" : "bad",
                proberSiteName, f->stToProberSiteMap[site], stBinCode, proberBinName);
        else
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "will bin \"%s\" die at prober site \"%s\" "
                "(SmarTest site %ld, softbin \"%s\") to prober bin %ld", 
                f->diePassed[site] ? "good" : "bad",
                proberSiteName, f->stToProberSiteMap[site], stBinCode, f->dieBins[site]);
    }
}

static int prepareBinActions(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomError_t testerError;
    char stBinCode[BIN_CODE_LENGTH] = {0};
    long stBinNumber = -1L;
    int sites = 0;
    phStateSkipMode_t skipMode = PHSTATE_SKIP_NORMAL;
    phEstateSiteUsage_t *sitePopulation = NULL;
    const char *proberSiteName = NULL;
    int i;
    phBinActType_t thisAction = PHBIN_ACT_NONE;

    /* early return with default actions, if no actions are defined */
    f->binActCurrent = PHBIN_ACT_NONE;
    if (f->binActUsed == PHBIN_ACT_NONE)
        return 1;

    phStateGetSkipMode(f->stateId, &skipMode);
    phEstateAGetSiteInfo(f->estateId, &sitePopulation, &sites);
    for (i=0; i<sites; i++)
    {
        thisAction = PHBIN_ACT_NONE;
        phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &i, 
            &proberSiteName);
        if ((skipMode == PHSTATE_SKIP_NORMAL || 
            skipMode == PHSTATE_SKIP_NEXT) && 
            !f->dontTrustBinning )
        {
            switch (sitePopulation[i])
            {
              case PHESTATE_SITE_POPULATED:
              case PHESTATE_SITE_POPDEACT:
                /* get SmarTest bin number and bin code */
                testerError = phTcomGetBinOfSite(f->testerId, f->stToProberSiteMap[i],
                                                 stBinCode, &stBinNumber);
                if (f->binActMode == PHBIN_ACTMODE_HARDBINS)
                {
                    if (testerError != PHTCOM_ERR_OK)
                    {
                        if (phFramePanic(f, 
                            "couldn't get bin number from SmarTest, "
                            "assuming '-1'"))
                            return 0;
                        stBinNumber = -1L;
                    }
                    mapAct(f->binActMapHdl, (long long)stBinNumber, &thisAction);
                }
                else
                {
                    if (testerError != PHTCOM_ERR_OK)
                    {
                        strcpy(stBinCode, "db");
                        if (phFramePanic(f,
                            "couldn't get bin code from SmarTest, trying 'db'"))
                            return 0;
                    }
                    long long llCode = 0LL;
                    strncpy((char *)&llCode, stBinCode, BIN_CODE_LENGTH);
                    mapAct(f->binActMapHdl, llCode, &thisAction);
                }
                f->binActCurrent = (phBinActType_t)(f->binActCurrent | thisAction);
                break;
              case PHESTATE_SITE_EMPTY:
              case PHESTATE_SITE_DEACTIVATED:
              case PHESTATE_SITE_POPREPROBED:
                break;
            } /* switch (sitePopulation[i]) */
            

        /* log any special binning actions */
            if (thisAction != PHBIN_ACT_NONE)
            {
                if (thisAction & PHBIN_ACT_CLEAN)
                {
                    /* perform automatic probe needle cleaning */
                    phLogFrameMessage(f->logId, LOG_DEBUG,
                        "will perform probe needle cleaning due to bin result at prober site \"%s\" "
                        "(SmarTest site %ld)",
                        proberSiteName, f->stToProberSiteMap[i]);
            
                }

                /* to be expanded for more actions..... */
            }

        } /* if regular skip mode */

    } /* for all sites loop */

    return 1;
}


static int prepareBinning(
    struct phFrameStruct *f             /* the framework data */
)
{
    binningNumberExistsInMap = 0;
    phTcomError_t testerError;
    char stBinCode[BIN_CODE_LENGTH] = {0};
    long stBinNumber = -1L;
    long stGoodPart;
    int sites = 0;
    phStateSkipMode_t skipMode = PHSTATE_SKIP_NORMAL;
    phEstateSiteUsage_t *sitePopulation = NULL;
    int i;
    const char *proberSiteName = NULL;
    int binPanic = 0;

    phStateGetSkipMode(f->stateId, &skipMode);
    phEstateAGetSiteInfo(f->estateId, &sitePopulation, &sites);
    for (i=0; i<sites; i++)
    {
        phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &i, 
            &proberSiteName);
        if (f->dontTrustBinning)
        {
            f->dieBins[i] = -1L;
            f->diePassed[i] = 1L;            
        }
        else switch (sitePopulation[i])
        {
          case PHESTATE_SITE_POPULATED:
          case PHESTATE_SITE_POPDEACT:
            switch (skipMode)
            {
              case PHSTATE_SKIP_NORMAL:
              case PHSTATE_SKIP_NEXT:

                /* get pass/fail result */
                testerError = phTcomGetSystemFlagOfSite(f->testerId,
                    PHTCOM_SF_CI_GOOD_PART, f->stToProberSiteMap[i],
                    &stGoodPart);
                if (testerError != PHTCOM_ERR_OK)
                {
                    if (phFramePanic(f, 
                        "couldn't get pass/fail result from SmarTest, "
                        "assuming 'passed'"))
                        return 0;
                    stGoodPart = 0;
                }
                f->diePassed[i] = stGoodPart;
                    
                /* get binning code and remap */
                switch (f->binMode)
                {
                  case PHBIN_MODE_DEFAULT:
                  case PHBIN_MODE_HARDMAP:
                    /* common part, get SmarTest bin number */
                    testerError = phTcomGetBinOfSite(f->testerId,
                        f->stToProberSiteMap[i], stBinCode,
                        &stBinNumber);
                    if (testerError != PHTCOM_ERR_OK)
                    {
                        if (phFramePanic(f, 
                            "couldn't get bin number from SmarTest, "
                            "assuming '-1'"))
                            return 0;
                        stBinNumber = -1L;
                    }

                    if (f->binMode == PHBIN_MODE_DEFAULT)
                    {
                        /* use default SmarTest hardbin mapping.
                           Usually bin numbers are directly
                           transfered to the handler unless it is
                           the retest bin number -1 */
                        if (stBinNumber != -1L)
                            /* the bin NUMBER from SmarTest is taken as the
                               handler bin (after index correction) */
                            f->dieBins[i] = stBinNumber;
                        else
                        {
                            /* SmarTest retest bin is remapped to some other SmarTest bin */
                            if (phConfConfIfDef(f->specificConfId, PHKEY_BI_HRTBINS) == PHCONF_ERR_OK)
                            {
                                if (mapBin(f->binMapHdl, -1LL, &(f->dieBins[i])))
                                {
                                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                                        "\n" BIG_ERROR_STRING
                                        "SmarTest retest bin can not remapped to some other SmarTest bin");
                                    binPanic++;
                                    f->dieBins[i] = -1L;
                                }
                                else
                                {
                                    /* we found retest bin in configuration file */
                                    if (phConfConfIfDef(f->specificConfId, PHKEY_BI_HBIDS) == PHCONF_ERR_OK)
                                    {
                                        int length = 0;
                                        int bin = f->dieBins[i];
                                        phConfType_t defType;
                                        const char *proberBinName = NULL;
                                        phConfError_t confTypeError = phConfConfType(f->specificConfId, PHKEY_BI_HBIDS, 0,
                          NULL, &defType, &length);
                                        if (confTypeError == PHCONF_ERR_OK) {
                                            phConfError_t confStringError = phConfConfString(f->specificConfId, PHKEY_BI_HBIDS, 1, &bin, &proberBinName);
                                            if (confStringError == PHCONF_ERR_OK) {
                                            if(NULL != proberBinName)
                                            {
                                                int realBin = -1;
                                                /* the attr file constants the proberBinName must be number */
                                                softBinCodeToBinNumber(f, (char*)proberBinName, &realBin);
                                                f->dieBins[i] = realBin;
                                            }
                                          }

                                        }
                                    }
                                }
                            }
                            else
                            {
                                f->dieBins[i] = -1L;
                            }
                        }
                    }
                    else
                    {
                        /* the bin NUMBER from SmarTest is taken as input
                           to the bin mapping */
                        if (mapBin(f->binMapHdl, (long long)stBinNumber, &(f->dieBins[i])))
                        {
                            /* cannot find the bin number in the defined map */
                            if(f->binPartialMode == 1 && stBinNumber != -1)
                            {
                                /* if defined the partial binning mode, try to directly map the bin number to the hard bin number */
                                f->dieBins[i] = stBinNumber;
                            }
                            else
                            {
                                /* try the retest bin first, if we didn't define the partial binning mode */
                                if (mapBin(f->binMapHdl, -1LL, &(f->dieBins[i])))
                                {
                                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                                        "\n" BIG_ERROR_STRING
                                        "SmarTest bin number %ld of current die "
                                        "not found in configuration \"%s\"\n"
                                        "and retest bins were not configured with \"%s\"",
                                        stBinNumber, PHKEY_BI_HARDMAP, PHKEY_BI_HRTBINS);
                                    binPanic++;
                                    f->dieBins[i] = -1L;
                                }
                                else
                                {
                                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                                        "\n" BIG_ERROR_STRING
                                        "SmarTest bin number %ld of current die "
                                        "not found in configuration \"%s\".\n"
                                        "Trying to bin to retest bin as configured in \"%s\".",
                                        stBinNumber, PHKEY_BI_HARDMAP, PHKEY_BI_HRTBINS);
                                }
                            }
                            binningNumberExistsInMap = 0;
                        }
                        else
                        {
                            binningNumberExistsInMap = 1;
                        }
                    }
                    break;
                  case PHBIN_MODE_SOFTMAP:
                  case PHBIN_MODE_DEFAULT_SOFTBIN:
                    /* the bin CODE from SmarTest is taken as input
                       to the bin mapping */
                    testerError = phTcomGetBinOfSite(f->testerId,
                        f->stToProberSiteMap[i], stBinCode,
                        &stBinNumber);
                    if (testerError != PHTCOM_ERR_OK)
                    {
                        strcpy(stBinCode, "db");
                        if (phFramePanic(f,
                            "couldn't get bin code from SmarTest, trying 'db'"))
                            return 0;
                    }

                    if (f->binMode == PHBIN_MODE_DEFAULT_SOFTBIN)
                    {
                        int binNumber = 0;
                        /* if we got db bin from the result, at it cannot be transformed to number, it will still be db */
                        /* for smt7, this code is used to restrict smt to transfer the NUMERICAL string to prober,
                         * otherwise it will be binned to "db" bin */
                        int iRet = softBinCodeToBinNumber(f, stBinCode, &binNumber);
                        if(iRet == -1)
                            strcpy(stBinCode, "db");
                        else
                            f->dieBins[i] = binNumber;
                    }

                    /* case of not default softbin mapping or we got "db" bin beforehand */
                    if (f->binMode != PHBIN_MODE_DEFAULT_SOFTBIN || strncmp(stBinCode, "db", strlen("db")) == 0)
                    {
                        long long llCode = 0LL;
                        strncpy((char *)&llCode, stBinCode, BIN_CODE_LENGTH);

                        /* for "db" bin, will find in the map first, if the result is negative, then find the defined retest bin */
                        /* for other soft bin code can not be find in the map, will try the retest bin firstly */
                        if (mapBin(f->binMapHdl, llCode, &(f->dieBins[i])))
                        {
                            binningNumberExistsInMap = 0;
                            int binNumber = 0;
                            /* cannot find the bin code in the defined map */
                            if(f->binPartialMode == 1 && softBinCodeToBinNumber(f, stBinCode, &binNumber) != -1)
                            {
                                /* if defined the partial binning mode, try to directly map the bin code to the bin number */
                                f->dieBins[i] = binNumber;
                            }
                            else
                            {
                                /* in case of we didn't define the partial binning mode or
                                 * we defined the partial binning mode, but the conversion if failed*/
                                if (mapBin(f->binMapHdl, -1LL, &(f->dieBins[i])))
                                {
                                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                                        "\n" BIG_ERROR_STRING
                                        "SmarTest bin code \"%s\" of current die "
                                        "not found in configuration \"%s\"\n"
                                        "and retest bins were not configured with \"%s\"",
                                        stBinCode, PHKEY_BI_SOFTMAP, PHKEY_BI_HRTBINS);
                                    binPanic++;
                                    f->dieBins[i] = -1L;
                                }
                                else
                                {
                                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                                        "\n" BIG_ERROR_STRING
                                        "SmarTest bin code \"%s\" of current die "
                                        "not found in configuration \"%s\".\n"
                                        "Trying to bin to retest bin as configured in \"%s\".",
                                        stBinCode, PHKEY_BI_SOFTMAP, PHKEY_BI_HRTBINS);
                                    binningNumberExistsInMap = 1;
                                    if(f->binMode == PHBIN_MODE_DEFAULT_SOFTBIN && phConfConfIfDef(f->specificConfId, PHKEY_BI_HBIDS) == PHCONF_ERR_OK)
                                    {
                                        int length = 0;
                                        int bin = f->dieBins[i];
                                        phConfType_t defType;
                                        const char *proberBinName = NULL;
                                        phConfConfType(f->specificConfId, PHKEY_BI_HBIDS, 0, NULL, &defType, &length);
                                        phConfConfString(f->specificConfId, PHKEY_BI_HBIDS, 1, &bin, &proberBinName);
                                        if(NULL != proberBinName)
                                        {
                                            int realBin = -1;
                                            /* the attr file constants the proberBinName must be number */
                                            softBinCodeToBinNumber(f, (char*)proberBinName, &realBin);
                                            f->dieBins[i] = realBin;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            binningNumberExistsInMap = 1;
                        }
                    }
                    break;
                } /* switch (binMode) */
                break;
              case PHSTATE_SKIP_CURRENT:
              case PHSTATE_SKIP_NEXT_CURRENT:
                /* the current dies were skipped, don't ink them but
                   send them to a retest bin */
                f->diePassed[i] = 1L;
                stBinNumber = -1L;
                if (mapBin(f->binMapHdl, -1LL, &(f->dieBins[i])))
                {
                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "the die at site \"%s\" was not tested (skipped)\n"
                        "and retest bins were not configured with \"%s\"",
                        proberSiteName, PHKEY_BI_HRTBINS);
                    binPanic++;
                    f->dieBins[i] = -1L;
                }
                else
                {
                    phLogFrameMessage(f->logId, LOG_NORMAL,
                        "the die at site \"%s\" was not tested (skipped).\n"
                        "Trying to bin to retest bin as configured in \"%s\".",
                        proberSiteName, PHKEY_BI_HRTBINS);
                }
                break;
            } /* switch (skipMode) */
            break;

          case PHESTATE_SITE_EMPTY:
          case PHESTATE_SITE_DEACTIVATED:
          case PHESTATE_SITE_POPREPROBED:
            f->dieBins[i] = -1L;
            f->diePassed[i] = 1L;
            break;
        } /* switch (sitePopulation[i]) */

        logBinDataOfSite(f, i, stBinCode, stBinNumber, sitePopulation[i]);

        //get real bin when the bin mode is mapped mode
        if(PHBIN_MODE_HARDMAP == f->binMode || PHBIN_MODE_SOFTMAP == f->binMode)
        {
            int length = 0;
            int bin = f->dieBins[i];
            phConfType_t defType;
            const char *proberBinName = NULL;

            phConfConfType(f->specificConfId, PHKEY_BI_HBIDS, 0, NULL, &defType, &length);

            if(f->binPartialMode == 0)
            {
                if (bin >= 0 && bin < length)
                    phConfConfString(f->specificConfId, PHKEY_BI_HBIDS, 1, &bin, &proberBinName);
            }
            else
            {
                if(binningNumberExistsInMap == 1)
                    phConfConfString(f->specificConfId, PHKEY_BI_HBIDS, 1, &bin, &proberBinName);
            }

            if(NULL != proberBinName)
            {
                int realBin = -1;
                /* the attr file constants the proberBinName must be number */
                softBinCodeToBinNumber(f, (char*)proberBinName, &realBin);
                f->dieBins[i] = realBin;
            }
        }

    } /* for all sites loop */

    if (binPanic)
    {
        if (phFramePanic(f, 
            "Couldn't determine correct prober bin for die(s).\n"
            "On CONTINUE I will try to bin the die(s) to the probers's default\n"
            "retest bin, if existent.\n"
            "This operation may fail and another panic message would show up"))
            return 0;            
    }

    return 1;
}


static void setSiteSetup(struct phFrameStruct *f)
{
    phEstateSiteUsage_t *sitePopulation = NULL;
    int entries = 0;
    int i;
    
    int site;
    const char *proberSiteName = NULL;

    phEstateAGetSiteInfo(f->estateId, &sitePopulation, &entries);

    /* set die positions and population for all sites */
    for (i = 0; i < f->numSites; i++)
    {
        site = i;
        phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &site, &proberSiteName);
        
        switch (sitePopulation[i])
        {
          case PHESTATE_SITE_POPULATED:
          case PHESTATE_SITE_POPDEACT:
                phTcomSetSystemFlagOfSite(f->testerId, 
                    PHTCOM_SF_CI_SITE_SETUP,
                    f->stToProberSiteMap[i], CI_SITE_INSERTED_TO_TEST);
                    
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "prober site \"%s\" (SmarTest site %ld) populated", 
                proberSiteName, f->stToProberSiteMap[site]);
            
            break;
          case PHESTATE_SITE_EMPTY:
          case PHESTATE_SITE_DEACTIVATED:
          case PHESTATE_SITE_POPREPROBED:
            phTcomSetSystemFlagOfSite(f->testerId, 
                PHTCOM_SF_CI_SITE_SETUP,
                f->stToProberSiteMap[i], CI_SITE_NOT_INSERTED);
                
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "prober site \"%s\" (SmarTest site %ld) currently not used", 
                proberSiteName, f->stToProberSiteMap[site]);
                
            break;
        }
    }
}

static void clearSiteSetup(struct phFrameStruct *f)
{
    int i;

    for (i = 0; i < f->numSites; i++)
        phTcomSetSystemFlagOfSite(f->testerId, 
            PHTCOM_SF_CI_SITE_SETUP,
            f->stToProberSiteMap[i], CI_SITE_NOT_INSERTED);
}


/*****************************************************************************
 *
 * Step to next die
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary die step actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameDieStart(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    int patternDone;
    int consistent = 1;
    phEstateSiteUsage_t *sitePopulation = NULL;
    phEstateSiteUsage_t correctedSitePopulation[PHESTATE_MAX_SITES];
    int entries = 0;
    int i;
    long siteDieX;
    long siteDieY;
    int sdPos;

    phStateTesterMode_t testerMode = PHSTATE_TST_NORMAL;
    int confFound;
    long quitFlag = 0;
    phDieInfo_t *pDieInfo = NULL;

    /* check the tester mode to see, whether we need to insert new parts now */
    phStateGetTesterMode(f->stateId, &testerMode);
    switch (testerMode)
    {
      case PHSTATE_TST_NORMAL:
        /* this is the regular case.... */
        break;
      case PHSTATE_TST_RETEST:
        /* retesting a device */
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "retesting die(s), "
            "no new die(s) are requested from the prober now");
        setSiteSetup(f);
        phConfConfStrTest(&confFound, f->specificConfId, 
            PHKEY_RP_RDAUTO, "yes", NULL);
        return (confFound == 1) ? phFrameTryReprobe(f) : retVal;
      case PHSTATE_TST_CHECK:
        /* checking a device */
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "checking die(s), "
            "no new die(s) are requested from the prober now");
        setSiteSetup(f);
        phConfConfStrTest(&confFound, f->specificConfId, 
            PHKEY_RP_CDAUTO, "yes", NULL);
        return (confFound == 1) ? phFrameTryReprobe(f) : retVal;
    }

    /* if the prober is paused and we reach this point, it means that
       SmarTest should also go into pause mode now. We do not ask the
       prober to do anything and asume that the site population has
       not yet changed */

    if ((!f->isSubdieProbing &&
            f->d_sp.mark == PHFRAME_STMARK_WDP) ||
        (f->isSubdieProbing && 
            f->d_sp.mark == PHFRAME_STMARK_ACT && 
            f->sd_sp.mark == PHFRAME_STMARK_WDP))
    {
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "prober is paused,\n"
            "no new die(s) are requested from the prober now.\n"
            "reseting die population for SmarTest to avoid a retest");
        clearSiteSetup(f);
        return retVal;
    }

    /* check whether we need to perform a probe needle cleaning,
       before 'officially' stepping to the next die. In case the
       prober performs the stepping, we may already be on the new die
       issued by the last binDie action */
    if (f->cleaningRatePerDie > 0)
    {
        /* only to be done, if a per die cleaning rate has been
           defined */
        f->cleaningDieCount =
            (f->cleaningDieCount + 1) % f->cleaningRatePerDie;
        if (f->cleaningDieCount == 0)
        {
            /* perform the cleaning action */
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "will perform probe needle cleaning because another %d die (groups) have been probed",
                f->cleaningRatePerDie);
            retVal = phFrameCleanProbe(f);
            if (retVal != PHTCOM_CI_CALL_PASS)
                return retVal;
        }

        /* early return in case the above operation ended in a quit situation */
        phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
        if (quitFlag)
            return retVal;
    }

    
    /* perform the die step */
    patternDone = runDieStepper(f, &(f->d_sp), &retVal);

    /* check consistency of die stepper petri networks */
    if (f->isSubdieProbing)
    {
        consistent = (
            (f->d_sp.mark == PHFRAME_STMARK_OUT && 
            f->sd_sp.mark == PHFRAME_STMARK_OUT &&
            patternDone) ||
            (f->d_sp.mark == PHFRAME_STMARK_ACT && 
            f->sd_sp.mark == PHFRAME_STMARK_ACT &&
            !patternDone)
            );
    }
    else
    {
        consistent = (
            (f->d_sp.mark == PHFRAME_STMARK_OUT && patternDone) ||
            (f->d_sp.mark == PHFRAME_STMARK_ACT && !patternDone)
            );
    }
    
    if (consistent)
    {
        if (patternDone)
            phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
        else
        {
            phEstateAGetSiteInfo(f->estateId, &sitePopulation, &entries);
            /* set die positions and population for all sites */
            for (i = 0; i < f->numSites; i++)
            {
                siteDieX = f->d_sp.currentX + f->perSiteDieOffset[i].x;
                siteDieY = f->d_sp.currentY + f->perSiteDieOffset[i].y;

                phTcomSetDiePosXYOfSite(f->testerId, 
                    f->stToProberSiteMap[i], siteDieX, siteDieY);

                correctedSitePopulation[i] = sitePopulation[i];

                switch (sitePopulation[i])
                {
                  case PHESTATE_SITE_POPULATED:
                  case PHESTATE_SITE_POPDEACT:
                    if (f->d_sp.stepMode == PHFRAME_STEPMD_EXPLICIT)
                    {
                        /* If doing explicit stepping:
                           only set site information for dies active, if they are:
                           1. within the min-max range of the wafermap
                           2. in use concerning of the wafermap 
                           3. not yet probed (tested) */
                        sdPos = (f->isSubdieProbing) ? f->sd_sp.pos : 0;
                        pDieInfo = (phDieInfo_t *) getElementRef(f->d_sp.dieMap, siteDieX, siteDieY);
                        if (siteDieX >= f->d_sp.minX && siteDieX <= f->d_sp.maxX &&
                            siteDieY >= f->d_sp.minY && siteDieY <= f->d_sp.maxY &&
                            pDieInfo != NULL &&
                            (pDieInfo[sdPos] & PHFRAME_DIEINF_INUSE) &&
                            !(pDieInfo[sdPos] & PHFRAME_DIEINF_PROBED))
                        {
                            pDieInfo[sdPos] = (phDieInfo_t)(pDieInfo[sdPos] | PHFRAME_DIEINF_PROBED);
                        }
                        else
                        {
                            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                                "not testing die [%ld, %ld], since it was probed earlier "
                                "or is outside of the defined wafermap",
                                siteDieX, siteDieY);
                            correctedSitePopulation[i] = PHESTATE_SITE_POPREPROBED;
                        }
                    }
                    break;
                  case PHESTATE_SITE_EMPTY:
                    if (f->d_sp.stepMode == PHFRAME_STEPMD_EXPLICIT)
                    {
                        /* If doing explicit stepping:
                           only set site information for dies active, if they are:
                           1. within the min-max range of the wafermap
                           2. in use concerning of the wafermap 
                           3. not yet probed (tested) */
                        sdPos = (f->isSubdieProbing) ? f->sd_sp.pos : 0;
                        pDieInfo = (phDieInfo_t *) getElementRef(f->d_sp.dieMap, siteDieX, siteDieY);
                        if (siteDieX >= f->d_sp.minX && siteDieX <= f->d_sp.maxX &&
                            siteDieY >= f->d_sp.minY && siteDieY <= f->d_sp.maxY &&
                            pDieInfo != NULL &&
                            (pDieInfo[sdPos] & PHFRAME_DIEINF_INUSE) &&
                            !(pDieInfo[sdPos] & PHFRAME_DIEINF_PROBED))
                        {
                            pDieInfo[sdPos] = (phDieInfo_t)(pDieInfo[sdPos] | PHFRAME_DIEINF_PROBED);
                        }
                    }
                    break;
                  default:
                    /* nothing special to do here */
                    break;
                } /* END switch (sitePopulation[i]) */
            } /* END for (i = 0; i < f->numSites; i++) */

            /* apply the correct site information, if doing explicit probing */
            if (f->d_sp.stepMode == PHFRAME_STEPMD_EXPLICIT)
                phEstateASetSiteInfo(f->estateId, correctedSitePopulation, entries);

            /* send site setup to smartest */
            setSiteSetup(f);

        } /* END if (!pattern_done) */

    } /* END if (consistent) */

    else if (
        (!f->isSubdieProbing &&
            f->d_sp.mark == PHFRAME_STMARK_WSP && !patternDone) ||
        (f->isSubdieProbing && 
            f->d_sp.mark == PHFRAME_STMARK_ACT && 
            f->sd_sp.mark == PHFRAME_STMARK_WSP && !patternDone))
    {
        /* prober has been paused, pause SmarTest too */
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "prober has been paused\n"
            "assuming no die(s) to test and entering pause mode");
        clearSiteSetup(f);
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_PAUSE, 1L);
    }
    else
    {
        /* timeout or possible error in stepping control */
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
            "could not step to next die for unknown reason\n");
        printPetriStatus(f);
        /* retVal was set by die stepper */
        /* retVal = PHTCOM_CI_CALL_ERROR; */
    }

    return retVal;
}

/*****************************************************************************
 *
 * bin die
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary bin actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameDieDone(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    
    int patternDone;
    int consistent = 1;
    long quitFlag = 0;

    phStateTesterMode_t testerMode = PHSTATE_TST_NORMAL;

    /* find out, whether we want to bin devices now or run another
       test on this device */
    phStateGetTesterMode(f->stateId, &testerMode);
    switch (testerMode)
    {
      case PHSTATE_TST_NORMAL:
        /* this is the regular case.... */
        f->binAction = PHFRAME_BINACT_DOBIN;
        break;
      case PHSTATE_TST_RETEST:
        /* retesting a device */
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "die(s) scheduled for retest, "
            "no die(s) are binned now");
        f->binAction = PHFRAME_BINACT_SKIPBIN;
        break;
      case PHSTATE_TST_CHECK:
        /* checking a device */
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "die(s) scheduled for check, "
            "no die(s) are binned now");
        f->binAction = PHFRAME_BINACT_SKIPBIN;
        break;
    }
    
    /* check whether we came out of a prober paused state (paused
       during waiting for dies) in this case we don't bin devices now */
    if (
        (f->isSubdieProbing && f->d_sp.mark == PHFRAME_STMARK_WSP) ||
        (!f->isSubdieProbing && f->d_sp.mark == PHFRAME_STMARK_ACT && 
            f->sd_sp.mark == PHFRAME_STMARK_WSP))
    {
        /* prober was paused, no dies to bin */
        /* don't event run the die stepper. We need to keep the state
           for th enext get die operation */
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "prober was paused previously during get die operation, "
            "no die(s) to bin now");
        f->binAction = PHFRAME_BINACT_SKIPBIN;
    }

    /* early return, if do not bin now */
    if (f->binAction == PHFRAME_BINACT_SKIPBIN)
        return retVal;

    /* check whether we came out of a prober paused state (paused
       during binning of dies) in this case we don't trust the old
       binning information. */

    if ((!f->isSubdieProbing &&
            f->d_sp.mark == PHFRAME_STMARK_WDP) ||
        (f->isSubdieProbing && 
            f->d_sp.mark == PHFRAME_STMARK_ACT && 
            f->sd_sp.mark == PHFRAME_STMARK_WDP))
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
            "prober was paused previously during bin die operation.\n"
            "Old binning information is not trusted");
        f->dontTrustBinning = 1;
    }
    else
        f->dontTrustBinning = 0;

    /* prepare the binning operation. It could be, that we don't bin
       here and now for various reasons:
       1. being in die done action while doing sub-die probing
       2. scheduling a retest or skip device procedure
       3. being in handtest mode
    */
    if (!prepareBinning(f))
    {
        /* error during binning preparation */
        return PHTCOM_CI_CALL_ERROR;
    }
    if (!prepareBinActions(f))
    {
        /* error during binning preparation */
        return PHTCOM_CI_CALL_ERROR;
    }

    /* check whether there are any special actions to be done first */
    if (f->binActCurrent != PHBIN_ACT_NONE)
    {
        if (f->binActCurrent & PHBIN_ACT_CLEAN)
        {
            /* do a probe needle cleaning */
            retVal = phFrameCleanProbe(f);
            if (retVal != PHTCOM_CI_CALL_PASS)
                return retVal;
        }
        
        /* to be expanded .... */


        /* early return in case the above operation ended in a quit situation */
        phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
        if (quitFlag)
            return retVal;
    }

    /* perform the die step */
    patternDone = runDieStepper(f, &(f->d_sp), &retVal);

    /* check consistency of die stepper petri networks */
    if (f->isSubdieProbing)
    {
        consistent = (
            (f->d_sp.mark == PHFRAME_STMARK_OUT && 
            f->sd_sp.mark == PHFRAME_STMARK_OUT &&
            patternDone) ||
            (f->d_sp.mark == PHFRAME_STMARK_STEP && 
            f->sd_sp.mark == PHFRAME_STMARK_OUT &&
            !patternDone) ||
            (f->d_sp.mark == PHFRAME_STMARK_ACT && 
            f->sd_sp.mark == PHFRAME_STMARK_STEP &&
            !patternDone)
            );
    }
    else
    {
        consistent = (
            (f->d_sp.mark == PHFRAME_STMARK_OUT && patternDone) ||
            (f->d_sp.mark == PHFRAME_STMARK_STEP && !patternDone)
            );
    }
    
    if (consistent)
    {
        if (patternDone)
            phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
        else
        {
            /* nothing special to do, dies are binned during
               runDieStepper operation */
        }
    }
#ifdef ALLOW_PAUSED_BINNING
    else if (
        (!f->isSubdieProbing &&
            f->d_sp.mark == PHFRAME_STMARK_WDP && !patternDone) ||
        (f->isSubdieProbing && 
            f->d_sp.mark == PHFRAME_STMARK_ACT && 
            f->sd_sp.mark == PHFRAME_STMARK_WDP && !patternDone))
    {
        /* prober has been paused, pause SmarTest too */
        phLogFrameMessage(f->logId, LOG_NORMAL,
            "prober has been paused\n"
            "assuming die(s) are not finally binned yet and entering pause mode");
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_PAUSE, 1L);
    }
#endif
    else
    {
        /* timeout or possible error in stepping control */
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
            "could not perform die end actions for unknown reason\n");
        printPetriStatus(f);
        /* retVal was set by die stepper */
        /* retVal = PHTCOM_CI_CALL_ERROR; */
    }

    return retVal;
}

/*****************************************************************************
 *
 * Reprobe current dies, if possible
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary reprobe actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameTryReprobe(
    struct phFrameStruct *f             /* the framework data */
)
{
    phStateDrvMode_t driverMode;
    phPFuncError_t funcError;
    phEventResult_t eventResult;
    phTcomUserReturn_t eventStReturn;
    phEventError_t eventError;

    exceptionActionError_t exceptionAction=PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    phEventResult_t exceptionResult = PHEVENT_RES_VOID;*/
/*    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;*/
/* End of Huatek Modification */

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    int alreadyReprobed = 0;
    long quitFlag = 0;

    /* we may need to perform some special actions before reprobing */
    if (!prepareBinActions(f))
    {
        /* error during binning preparation */
        return PHTCOM_CI_CALL_ERROR;
    }

    /* check whether there are any special actions to be done first */
    if (f->binActCurrent != PHBIN_ACT_NONE)
    {
        if (f->binActCurrent & PHBIN_ACT_CLEAN)
        {
            /* do a probe needle cleaning */
            retVal = phFrameCleanProbe(f);
            if (retVal != PHTCOM_CI_CALL_PASS)
                return retVal;
            if (f->funcAvail & PHPFUNC_AV_CLEAN)
            {
                /* cleaning (and reprobe) was done */
                alreadyReprobed = 1;
            }
        }
        
        /* to be expanded .... */



        /* early return in case the above operation ended in a quit situation */
        phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
        if (quitFlag)
            return retVal;
    }
    
    if (alreadyReprobed)
    {
        phLogFrameMessage(f->logId, LOG_DEBUG,
            "will not reprobe dies, since needle cleaning was just performed");
        return retVal;
    }

    if ((f->funcAvail & PHPFUNC_AV_REPROBE))
    {
        phStateGetDrvMode(f->stateId, &driverMode);
        if (driverMode == PHSTATE_DRV_HAND)
        {
            phLogFrameMessage(f->logId, LOG_DEBUG,
                "reprobing die(s) in hand test mode");
            funcError = PHPFUNC_ERR_TIMEOUT;
            eventError = phEventHandtestReprobe(f->eventId,
                &eventResult, &eventStReturn, &funcError);
            if (eventError == PHEVENT_ERR_OK)
                switch (eventResult)
                {
                  case PHEVENT_RES_HANDLED:
                    /* Reprobe parts has been simulated, go on with
                       success, etc... funcError was set accordingly */
                    break;
                  case PHEVENT_RES_ABORT:
                    /* testprogram aborted by operator, QUIT flag has
                       been set, return to test cell client with
                       suggest value */
                    phLogFrameMessage(f->logId, PHLOG_TYPE_FATAL,
                        "aborting prober driver");
                    return eventStReturn;
                  default:
                    /* not expected, this is a bug, print an error and
                       try to recover during the next iteration */
                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "unexpected return value (%ld) from handtest Reprobe",
                        (long) eventResult);
                    funcError = PHPFUNC_ERR_TIMEOUT;
                    break;
                }
        }
        else
        {
            /* this is the regular case !!!!!!! */

            while (!success && !completed)
            {
                funcError = phPlugPFuncReprobe(f->funcId);

                /* analyze result from bin (sub) die call and take action */
                phFrameHandlePluginResult(f, funcError, 
                                          PHPFUNC_AV_REPROBE,
                                          &completed, &success, &retVal, &exceptionAction);
            }
        }
    }
    else        
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
            "ph_reprobe() is not available for the current prober driver");

    return retVal;
}
/*****************************************************************************
 * End of file
 *****************************************************************************/
