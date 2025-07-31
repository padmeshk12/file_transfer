/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : device.c
 * CREATED  : 30 May 2000
 *
 * CONTENTS : Device handling functions
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 30 May 2000, Michael Vogt, created
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
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_hfunc.h"
#include "ph_hhook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "binner.h"
#include "exception.h"
#include "device.h"
#include "helpers.h"
#include "dev/DriverDevKits.hpp"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/
static int retry_retest_count = 0;

/*--- typedefs --------------------------------------------------------------*/
extern int phFramePanic(
    struct phFrameStruct *frame,
    const char *message
);
extern void phFrameHandlePluginResult(
    struct phFrameStruct *f,
    phFuncError_t funcError,
    phFuncAvailability_t p_call,
    int *completed,
    int *success,
    phTcomUserReturn_t *retVal
);

/*--- functions -------------------------------------------------------------*/

static void logBinDataOfSite(
    struct phFrameStruct *f             /* the framework data */,
    int site,
    char *stBinCode,
    long stBinNumber,
    phEstateSiteUsage_t population
)
{
    int bin = (int) f->deviceBins[site]; /* needs to be type int */
    const char *handlerSiteName = NULL;
    const char *handlerBinName = NULL;
    phConfType_t defType;
    int length;

    phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &site, 
        &handlerSiteName);

    /* first handle case of empty / not tested devices */
    if (population == PHESTATE_SITE_EMPTY || population == PHESTATE_SITE_DEACTIVATED)
    {
	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "handler site \"%s\" (SmarTest site %ld) currently not used, not binned", 
	    handlerSiteName, f->stToHandlerSiteMap[site]);
	return;
    }

    /* get bin name from configuration, if defined */
    handlerBinName = NULL;
    if (phConfConfIfDef(f->specificConfId, PHKEY_BI_HBIDS) == 
	PHCONF_ERR_OK)
    {
	phConfConfType(f->specificConfId, PHKEY_BI_HBIDS, 0, NULL, 
		       &defType, &length);
	if (bin >= 0 && bin < length)
	    phConfConfString(f->specificConfId, PHKEY_BI_HBIDS, 1, &bin, 
			     &handlerBinName);
	else if (bin >= length)
	{
	    /* working with default binning and defined handler bin IDs,
	       but handler bin ID list is not long enough */
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"bin number %d is out of range for configuration \"%s\"\n"
		"Will try to pass bin number to handler anyway", 
		bin, PHKEY_BI_HBIDS);
	}
    }

    /* write the log info */
    if (f->binMode == PHBIN_MODE_DEFAULT || f->binMode == PHBIN_MODE_HARDMAP)
    {
	if (handlerBinName)
	    phLogFrameMessage(f->logId, LOG_DEBUG,
		"will bin device at handler site \"%s\" "
		"(SmarTest site %ld, hardbin %ld) to handler bin \"%s\"",
		handlerSiteName, f->stToHandlerSiteMap[site], stBinNumber, handlerBinName);
	else
	    phLogFrameMessage(f->logId, LOG_DEBUG,
		"will bin device at handler site \"%s\" "
		"(SmarTest site %ld, hardbin %ld) to handler bin %ld",
		handlerSiteName, f->stToHandlerSiteMap[site], stBinNumber, f->deviceBins[site]);
    }
    else /* PHBIN_MODE_SOFTMAP */
    {
	if (handlerBinName)
	    phLogFrameMessage(f->logId, LOG_DEBUG,
		"will bin device at handler site \"%s\" "
		"(SmarTest site %ld, softbin \"%s\") to handler bin \"%s\"",
		handlerSiteName, f->stToHandlerSiteMap[site], stBinCode, handlerBinName);
	else
	    phLogFrameMessage(f->logId, LOG_DEBUG,
		"will bin device at handler site \"%s\" "
		"(SmarTest site %ld, softbin \"%s\") to handler bin %ld",
		handlerSiteName, f->stToHandlerSiteMap[site], stBinCode, f->deviceBins[site]);
    }
}

static void logReprobeDataOfSite(
    struct phFrameStruct *f             /* the framework data */,
    int site,
    phEstateSiteUsage_t population
)
{
    int reprobe = (int) f->deviceToReprobe[site]; /* needs to be type int */
    const char *handlerSiteName = NULL;
/*    char *handlerBinName = NULL;
 *   phConfType_t defType;
 *   int length;*/

    phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &site, 
        &handlerSiteName);

    /* first handle case of empty / not tested devices */
    if (population == PHESTATE_SITE_EMPTY || population == PHESTATE_SITE_DEACTIVATED)
    {
	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "handler site \"%s\" (SmarTest site %ld) currently not used, not reprobed", 
	    handlerSiteName, f->stToHandlerSiteMap[site]);
	return;
    }

    /* write the log info */
    phLogFrameMessage(f->logId, LOG_DEBUG,
	"will %s device at handler site \"%s\" "
	"(SmarTest site %ld)", 
	reprobe ? "reprobe" : "not reprobe",
	handlerSiteName, f->stToHandlerSiteMap[site]);
}

static int prepareBinOneSite(
      struct phFrameStruct *f,           /* the framework data */
      int site,                          /* the site to prepare */
      phStateSkipMode_t skipMode,
      phEstateSiteUsage_t sitePopulation,
      int *panics
)
{
    const char *handlerSiteName = NULL;
    phTcomError_t testerError;
    char stBinCode[BIN_CODE_LENGTH] = {0};
    long stBinNumber = -1L;
    long stGoodPart;

    phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &site, 
	&handlerSiteName);
    if (f->dontTrustBinning)
    {
	f->deviceBins[site] = -1L;
	f->devicePassed[site] = 1L;	    
    }
    else switch (sitePopulation)
    {
      case PHESTATE_SITE_POPULATED:
      case PHESTATE_SITE_POPDEACT:
	switch (skipMode)
	{
	  case PHSTATE_SKIP_NORMAL:
	  case PHSTATE_SKIP_NEXT:

	    /* get pass/fail result */
	    testerError = phTcomGetSystemFlagOfSite(f->testerId,
		PHTCOM_SF_CI_GOOD_PART, f->stToHandlerSiteMap[site],
		&stGoodPart);
	    if (testerError != PHTCOM_ERR_OK)
	    {
		if (phFramePanic(f, 
		    "couldn't get pass/fail result from SmarTest, "
		    "assuming 'passed'"))
		    return 0;
		stGoodPart = 0;
	    }
	    f->devicePassed[site] = stGoodPart;
		    
	    /* get binning code and remap */
	    switch (f->binMode)
	    {
	      case PHBIN_MODE_DEFAULT:
	      case PHBIN_MODE_HARDMAP:
		/* common part, get SmarTest bin number */
		testerError = phTcomGetBinOfSite(f->testerId,
		    f->stToHandlerSiteMap[site], stBinCode,
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
			f->deviceBins[site] = stBinNumber;
		    else
		    {
			/* SmarTest retest bin is remapped to some other SmarTest bin */
			if (phConfConfIfDef(f->specificConfId, PHKEY_BI_HRTBINS) == PHCONF_ERR_OK)
                        {
			    if (mapBin(f->binMapHdl, -1LL, &(f->deviceBins[site])))
                            {
                                    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                                        "\n" BIG_ERROR_STRING
                                        "SmarTest retest bin can not remapped to some other SmarTest bin");
			            (*panics)++;
                                    f->deviceBins[site] = -1L;
                            }
                        }
			else
                        {
			    f->deviceBins[site] = -1L;
                        }
		    }
		}
		else
		{
		    /* the bin NUMBER from SmarTest is taken as input
		       to the bin mapping */
		    if (mapBin(f->binMapHdl, (long long)stBinNumber, &(f->deviceBins[site])))
		    {
			if (mapBin(f->binMapHdl, -1LL, &(f->deviceBins[site])))
			{
			    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
				"\n" BIG_ERROR_STRING
				"SmarTest bin number %ld of current device "
				"not found in configuration \"%s\"\n"
				"and retest bins were not configured with \"%s\"",
				stBinNumber, PHKEY_BI_HARDMAP, PHKEY_BI_HRTBINS);
			    (*panics)++;
			    f->deviceBins[site] = -1L;
			}
			else
			{
			    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
				"SmarTest bin number %ld of current device "
				"not found in configuration \"%s\".\n"
				"Trying to bin to retest bin as configured in \"%s\".",
				stBinNumber, PHKEY_BI_HARDMAP, PHKEY_BI_HRTBINS);
			}
		    }
		}
		break;
	      case PHBIN_MODE_SOFTMAP:
		/* the bin CODE from SmarTest is taken as input
		   to the bin mapping */
		testerError = phTcomGetBinOfSite(f->testerId,
		    f->stToHandlerSiteMap[site], stBinCode,
		    &stBinNumber);
		if (testerError != PHTCOM_ERR_OK)
		{
		    strcpy(stBinCode, "db");
		    if (phFramePanic(f,
			"couldn't get bin code from SmarTest, trying 'db'"))
			return 0;
		}

                long long llCode = 0LL;
                strncpy((char *)&llCode, stBinCode, BIN_CODE_LENGTH);
		if (mapBin(f->binMapHdl, llCode, &(f->deviceBins[site])))
		{
		    if (mapBin(f->binMapHdl, -1LL, &(f->deviceBins[site])))
		    {
			phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
			    "\n" BIG_ERROR_STRING
			    "SmarTest bin code \"%s\" of current device "
			    "not found in configuration \"%s\"\n"
			    "and retest bins were not configured with \"%s\"",
			    stBinCode, PHKEY_BI_SOFTMAP, PHKEY_BI_HRTBINS);
			(*panics)++;
			f->deviceBins[site] = -1L;
		    }
		    else
		    {
			phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
			    "SmarTest bin code \"%s\" of current device "
			    "not found in configuration \"%s\".\n"
			    "Trying to bin to retest bin as configured in \"%s\".",
			    stBinCode, PHKEY_BI_SOFTMAP, PHKEY_BI_HRTBINS);
		    }
		}
		break;
	    } /* switch (binMode) */
	    break;
	  case PHSTATE_SKIP_CURRENT:
	  case PHSTATE_SKIP_NEXT_CURRENT:
	    /* the current devices were skipped, don't ink them but
	       send them to a retest bin */
	    f->devicePassed[site] = 1L;
	    stBinNumber = -1L;
	    if (mapBin(f->binMapHdl, -1LL, &(f->deviceBins[site])))
	    {
		phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		    "the device at site \"%s\" was not tested (skipped)\n"
		    "and retest bins were not configured with \"%s\"",
		    handlerSiteName, PHKEY_BI_HRTBINS);
		(*panics)++;
		f->deviceBins[site] = -1L;
	    }
	    else
	    {
		phLogFrameMessage(f->logId, LOG_NORMAL,
		    "the device at site \"%s\" was not tested (skipped).\n"
		    "Trying to bin to retest bin as configured in \"%s\".",
		    handlerSiteName, PHKEY_BI_HRTBINS);
	    }
	    break;
	} /* switch (skipMode) */
	break;

      case PHESTATE_SITE_EMPTY:
      case PHESTATE_SITE_DEACTIVATED:
	f->deviceBins[site] = -1L;
	f->devicePassed[site] = 1L;
	break;
    } /* switch (sitePopulation) */

    logBinDataOfSite(f, site, stBinCode, stBinNumber, sitePopulation);
    return 1;
}

static int prepareReprobeOneSite(
      struct phFrameStruct *f,           /* the framework data */
      int site,                          /* the site to prepare */
      phStateSkipMode_t skipMode,
      phEstateSiteUsage_t sitePopulation
)
{
    phTcomError_t testerError;
    long stReprobe = 0;

    if (f->dontTrustBinning)
    {
	f->deviceToReprobe[site] = 0;
    }
    else switch (sitePopulation)
    {
      case PHESTATE_SITE_POPULATED:
      case PHESTATE_SITE_POPDEACT:
	switch (skipMode)
	{
	  case PHSTATE_SKIP_NORMAL:
	  case PHSTATE_SKIP_NEXT:
	    /* get reprobe flag */
	    testerError = phTcomGetSystemFlagOfSite(f->testerId,
		PHTCOM_SF_CI_SITE_REPROBE, f->stToHandlerSiteMap[site],
		&stReprobe);
	    if (testerError != PHTCOM_ERR_OK)
	    {
		if (phFramePanic(f, 
		    "couldn't get reprobe flag from SmarTest, "
		    "assuming 'don't reprobe'"))
		    return 0;
		stReprobe = 0;
	    }
	    f->deviceToReprobe[site] = stReprobe;
	    break;
	  case PHSTATE_SKIP_CURRENT:
	  case PHSTATE_SKIP_NEXT_CURRENT:
	    /* the current devices were skipped, don't reprobe them */
	    f->deviceToReprobe[site] = 0;
	    break;
	} /* switch (skipMode) */
	break;

      case PHESTATE_SITE_EMPTY:
      case PHESTATE_SITE_DEACTIVATED:
	f->deviceToReprobe[site] = 0;
	break;
    } /* switch (sitePopulation) */

    logReprobeDataOfSite(f, site, sitePopulation);
    return 1;
}

/**
 * Get current tester mode
 * change tester mode to retest if bin to retest hanlder bin for auto_reprobe_on_retest_device
 * auto_reprobe_on_retest_device:"yes"
 * handler_bin_ids:[ "1" "2" "3" "4" "5" "6" "7" "8" "9" "A"  "B"  "C"  "D"  "E"  "F" ]
 * handler_retest_bins:[ 1 ]
 * hardbin_to_handler_bin_map:[ [1] [2] [3] [4] [5] [6] [7] [8] [9] [10] [11] [12] [13] [14] [15] ]
 *
 * return retest mode if bin to retest handler bin("2") else return current tester mode
 */
static void getCurrentTesterMode(
    struct phFrameStruct *f,             /* the framework data */
    phStateTesterMode_t *testerMode      /* current test mode */
)
{
    phStateGetTesterMode(f->stateId, testerMode);

    /* only change tester mode at auto_reprobe_on_retest_device */
    int confFound = 0;
    phConfConfStrTest(&confFound, f->specificConfId,
                      PHKEY_RP_RDAUTO, "yes", NULL);
    if(confFound != 1)  return;

    /* initialize PH variable TESTER_MODE */
    driver_dev_kits::AccessPHVariable& aphv = driver_dev_kits::AccessPHVariable::getInstance();
    aphv.setPHVariable("TESTER_MODE", "NORMAL");

    /* look for the handler retest count */
    double dRetestCount;
    if (phConfConfIfDef(f->specificConfId, PHKEY_RP_RETESTCOUNT) != PHCONF_ERR_OK
        || phConfConfNumber(f->specificConfId, PHKEY_RP_RETESTCOUNT, 0, NULL, &dRetestCount) != PHCONF_ERR_OK)
    {
        dRetestCount = 1; /* default retest retry */
    }

    phLogFrameMessage(f->logId, LOG_DEBUG,
        "current retest count is: %d and handler max retest count is: %d", retry_retest_count+1, (int)dRetestCount);

    if(retry_retest_count >= (int)dRetestCount)
    {
      retry_retest_count = 0;
      return;
    }

    phTcomError_t testerError;
    phEstateSiteUsage_t *sitePopulation = NULL;
    int sites = 0;
    char stBinCode[BIN_CODE_LENGTH] = {0};
    long stBinNumber = -1L;     /* smartest bin number */
    long hdBinNumber = -1L;     /* handler bin number */
    long rtBinNumber = -1L;     /* retest bin number */

    /* get retest handler bin number */
    if (phConfConfIfDef(f->specificConfId, PHKEY_BI_HRTBINS) == PHCONF_ERR_OK)
    {
        if (mapBin(f->binMapHdl, -1LL, &rtBinNumber))
        {
            return;
        }
    }

    phLogFrameMessage(f->logId, LOG_DEBUG,
            "handler retest bin is: [%ld]", rtBinNumber);

    phEstateAGetSiteInfo(f->estateId, &sitePopulation, &sites);
    /* loop all site */
    for (int site=0; site<sites; site++)
    {
        /* skip deactivated site */
        if (sitePopulation[site] != PHESTATE_SITE_EMPTY && sitePopulation[site] != PHESTATE_SITE_POPULATED) {
          continue;
        }

        /* get SmarTest bin number */
        testerError = phTcomGetBinOfSite(f->testerId,
            f->stToHandlerSiteMap[site], stBinCode,
            &stBinNumber);
        if (testerError != PHTCOM_ERR_OK)
        {
            return;
        }

        /* get binning code and remap */
        switch (f->binMode)
        {
          case PHBIN_MODE_DEFAULT:
              hdBinNumber = stBinNumber;
              break;
          case PHBIN_MODE_HARDMAP:
              /* smartest hard bin number map to handler bin number */
              if (mapBin(f->binMapHdl, (long long)stBinNumber, &hdBinNumber))
              {
                  return;
              }
              break;
          case PHBIN_MODE_SOFTMAP:
              long long llCode = 0LL;
              strncpy((char *)&llCode, stBinCode, BIN_CODE_LENGTH);
              /* smartest soft bin number map to handler bin number */
              if (mapBin(f->binMapHdl, llCode, &hdBinNumber))
              {
                return;
              }
              break;
        } /* switch (binMode) */

        phLogFrameMessage(f->logId, LOG_DEBUG,
            "binMode is: %d, on site %d got smartest bin number %ld map to handler bin number %ld",
            f->binMode, site+1, stBinNumber, hdBinNumber);

        /* bin to retest bin */
        if(hdBinNumber == rtBinNumber)
        {
            *testerMode = PHSTATE_TST_RETEST;
            phStateSetTesterMode(f->stateId, PHSTATE_TST_RETEST);

            /* set PH variable to RETEST */
            aphv.setPHVariable("TESTER_MODE", "RETEST");

            retry_retest_count++;
            break;
        }
    }

    phLogFrameMessage(f->logId, LOG_DEBUG, "current tester mode is: %d", *testerMode);
}

static int prepareBinning(
    struct phFrameStruct *f             /* the framework data */
)
{
    int sites = 0;
    phStateSkipMode_t skipMode = PHSTATE_SKIP_NORMAL;
    phEstateSiteUsage_t *sitePopulation = NULL;
    int i;
    int binPanic = 0;


    phStateGetSkipMode(f->stateId, &skipMode);
    phEstateAGetSiteInfo(f->estateId, &sitePopulation, &sites);
    for (i=0; i<sites; i++)
    {
	if (!prepareBinOneSite(f, i, skipMode, sitePopulation[i], &binPanic))
	    return 0;
    } /* for all sites loop */

    if (binPanic)
    {
	if (phFramePanic(f, 
	    "Couldn't determine correct handler bin for device(s).\n"
	    "On CONTINUE I will try to bin the device(s) to the handler's default\n"
	    "retest bin, if existent.\n"
	    "This operation may fail and another panic message would show up"))
	    return 0;	    
    }

    return 1;
}


static int prepareReprobeBinning(
    struct phFrameStruct *f             /* the framework data */
)
{
    int sites = 0;
    phStateSkipMode_t skipMode = PHSTATE_SKIP_NORMAL;
    phEstateSiteUsage_t *sitePopulation = NULL;
    int i;
    int binPanic = 0;


    phStateGetSkipMode(f->stateId, &skipMode);
    phEstateAGetSiteInfo(f->estateId, &sitePopulation, &sites);
    for (i=0; i<sites; i++)
    {
	if (!prepareReprobeOneSite(f, i, skipMode, sitePopulation[i]))
	    return 0;
	if (!f->deviceToReprobe[i])
	{
	    if (!prepareBinOneSite(f, i, skipMode, sitePopulation[i], &binPanic))
		return 0;
	}
    } /* for all sites loop */

    if (binPanic)
    {
	if (phFramePanic(f, 
	    "Couldn't determine correct handler bin for device(s).\n"
	    "On CONTINUE I will try to bin the device(s) to the handler's default\n"
	    "retest bin, if existent.\n"
	    "This operation may fail and another panic message would show up"))
	    return 0;	    
    }

    return 1;
}


static int setSiteSetup(struct phFrameStruct *f)
{
    phEstateSiteUsage_t *sitePopulation = NULL;
    int entries = 0;
    int i;
    
    int site;
    const char *handlerSiteName = NULL;
    

    /* get handler site information */
    phEstateAGetSiteInfo(f->estateId, &sitePopulation, &entries);
    if (entries != f->numSites)
    {
	if (phFramePanic(f, 
	    "the driver's internal site control is screwed up"))
	    return 0;
    }

    /* set device positions and population for all sites */
    for (i = 0; i < f->numSites; i++)
    {
        site = i;
	phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &site, 
            &handlerSiteName);

	switch (sitePopulation[i])
	{
	  case PHESTATE_SITE_POPULATED:
	  case PHESTATE_SITE_POPDEACT:
		phTcomSetSystemFlagOfSite(f->testerId, 
		    PHTCOM_SF_CI_SITE_SETUP,
		    f->stToHandlerSiteMap[i], CI_SITE_INSERTED_TO_TEST);
		    
	        phLogFrameMessage(f->logId, LOG_DEBUG,
		    "handler site \"%s\" (SmarTest site %ld) populated", 
	             handlerSiteName, f->stToHandlerSiteMap[site]);
	    break;
	  case PHESTATE_SITE_EMPTY:
	  case PHESTATE_SITE_DEACTIVATED:
	    phTcomSetSystemFlagOfSite(f->testerId, 
		PHTCOM_SF_CI_SITE_SETUP,
		f->stToHandlerSiteMap[i], CI_SITE_NOT_INSERTED);
		    
	        phLogFrameMessage(f->logId, LOG_DEBUG,
		    "handler site \"%s\" (SmarTest site %ld) currently not used", 
	             handlerSiteName, f->stToHandlerSiteMap[site]);
	    break;
	}
    }

    return 1;
}

static int clearSiteSetup(struct phFrameStruct *f)
{
    int i;

    for (i = 0; i < f->numSites; i++)
	phTcomSetSystemFlagOfSite(f->testerId, 
	    PHTCOM_SF_CI_SITE_SETUP,
	    f->stToHandlerSiteMap[i], CI_SITE_NOT_INSERTED);

    return 1;
}


/*****************************************************************************
 *
 * Wait for lot start signal from tester
 *
 * Authors: Ken Ward
 *
 * Description:
 * Waits for SRQ from handler that indicates lot start.
 * Returns error if does not get an appropriate SRQ.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameLotStart(
                                  struct phFrameStruct *f,             /* the framework data */
                                  char *parm                          /* if == NOTIMEOUT, then will not return until gets an SRQ */
                                  )
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int success = 0;
    int completed = 0;
    phFuncError_t funcError;

    if ( (f->funcAvail & PHFUNC_AV_LOT_START) )
    {
        while ( !success && !completed )
        {
            funcError = phPlugFuncLotStart(f->funcId, parm);

            /* analyze result from get lot start call and take action */
            phFrameHandlePluginResult(f, funcError, PHFUNC_AV_LOT_START, &completed, &success, &retVal);
        }
    }
    else
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                          "ph_lot_start is not available for the current handler driver");
    }

    return retVal;
}


/*****************************************************************************
 *
 * Clean up after lot done
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameLotDone(
                                 struct phFrameStruct *f,             /* the framework data */
                                 char *parm                          /* if == NOTIMEOUT, then will not return until gets an SRQ */
                                 )
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int success = 0;
    int completed = 0;
    phFuncError_t funcError;

    if ( (f->funcAvail & PHFUNC_AV_LOT_DONE) )
    {
        while ( !success && !completed )
        {
            funcError = phPlugFuncLotDone(f->funcId, parm);

            if (funcError == PHFUNC_ERR_OK)
            {
                /* To make the multilot application model enable, flag will be set in driver plugin */
                /* exit the lot level */
                /*phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);*/
            }

            /* analyze result from get lot start call and take action */
            phFrameHandlePluginResult(f, funcError, PHFUNC_AV_LOT_DONE, &completed, &success, &retVal);
        }
    }
    else
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                          "ph_lot_done is not available for the current handler driver");
    }

    return retVal;
}

/*magco li start Oct 24 2014 */
/*****************************************************************************
 *
 * Wait for strip start signal from tester
 *
 * Authors: magco li
 *
 * Description:
 * Waits for SRQ from handler that indicates strip start.
 * Returns error if does not get an appropriate SRQ.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameStripStart(
                                  struct phFrameStruct *f,             /* the framework data */
                                  char *parm                          /* if == NOTIMEOUT, then will not return until gets an SRQ */
                                  )
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int success = 0;
    int completed = 0;
    phFuncError_t funcError;

    if ( PHFUNC_AV_STRIP_START )
    {
        while ( !success && !completed )
        {
            funcError = phPlugFuncStripStart(f->funcId, parm);

            /* analyze result from get strip start call and take action */
            phFrameHandlePluginResult(f, funcError, PHFUNC_AV_STRIP_START, &completed, &success, &retVal);
        }
    }
    else
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                          "ph_strip_start is not available for the current handler driver");
    }

    return retVal;
}


/*****************************************************************************
 *
 * Clean up after strip done
 *
 * Authors: magco li
 *
 * Description:
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameStripDone(
                                 struct phFrameStruct *f,             /* the framework data */
                                 char *parm                          /* if == NOTIMEOUT, then will not return until gets an SRQ */
                                 )
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int success = 0;
    int completed = 0;
    phFuncError_t funcError;

    if ( PHFUNC_AV_STRIP_DONE )
    {
        while ( !success && !completed )
        {
            funcError = phPlugFuncStripDone(f->funcId, parm);

            if (funcError == PHFUNC_ERR_OK)
            {
                /* exit the strip level */
                //phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
            }

            /* analyze result from get strip start call and take action */
            phFrameHandlePluginResult(f, funcError, PHFUNC_AV_STRIP_DONE, &completed, &success, &retVal);
        }
    }
    else
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                          "ph_strip_done is not available for the current handler driver");
    }

    return retVal;
}
/*magco li end Oct 24 2014 */

/*****************************************************************************
 *
 * Step to next devices
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary device step actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameDeviceStart(
                                     struct phFrameStruct *f             /* the framework data */
                                     )
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int success = 0;
    int completed = 0;

    phStateTesterMode_t testerMode;
    phStateDrvMode_t driverMode;
    phFuncError_t funcError;

    phEventError_t eventError;
    phEventResult_t eventResult;
    phTcomUserReturn_t eventStReturn;

/*    phEstateSiteUsage_t *sitePopulation = NULL;*/
/*    int siteEntries = 0;*/
/*    int i;*/

    int confFound = 0;

    /* pseudo code:
     * 
     * if (retest or check device)
     *     write log
     *     if (auto reprobe)
     *         reprobe
     *     endif
     * else
     *     do
     *         do
     *             if (handtest)
     *                 handtest event: get part
     *             else
     *                 handler get part
     *             endif
     *             if (not got (heartbeat timeout))
     *                 check system flags
     *                 if (no flags)
     *                     event handler: waiting for parts
     *                 endif
     *             endif
     *         while (not got and no flags and not waiting for parts timeout)
     *         if (not got and no flags)
     *             event handler: timeout
     *                 if (ask operator)
     *                     continue waiting?
     *                     abort?
     *                     assume parts in socket?
     *                 endif
     *             endevent
     *         endif
     *     while (not got and not handled and no flags)
     * endif
     * set SmarTest site usage flags
     */

    /* check the tester mode to see, whether we need to insert new parts now */
    testerMode = PHSTATE_TST_NORMAL;
    phStateGetTesterMode(f->stateId, &testerMode);
    switch ( testerMode )
    {
        case PHSTATE_TST_NORMAL:
            /* this is the regular case.... */
            break;
        case PHSTATE_TST_RETEST:
            /* retesting a device */
            phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
                              "retesting device(s),\n"
                              "no new devices are requested from the handler now");
            if ( !setSiteSetup(f) )
                return PHTCOM_CI_CALL_ERROR;
            phConfConfStrTest(&confFound, f->specificConfId, 
                              PHKEY_RP_RDAUTO, "yes", NULL);
            return(confFound == 1) ? phFrameTryReprobe(f) : retVal;
        case PHSTATE_TST_CHECK:
            /* checking a device */
            phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
                              "checking device(s),\n"
                              "no new devices are requested from the handler now");
            if ( !setSiteSetup(f) )
                return PHTCOM_CI_CALL_ERROR;
            phConfConfStrTest(&confFound, f->specificConfId, 
                              PHKEY_RP_CDAUTO, "yes", NULL);
            return(confFound == 1) ? phFrameTryReprobe(f) : retVal;
    }

    /* this is the big working loop: wait for parts until got or
       aborted by operator */
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    
    while ( !success && !completed )
    {
        phStateGetDrvMode(f->stateId, &driverMode);
        if ( driverMode == PHSTATE_DRV_HAND || 
             (driverMode == PHSTATE_DRV_SST_HAND) )
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
                              "waiting for device(s) in hand test mode");
            funcError = PHFUNC_ERR_TIMEOUT;
            eventError = phEventHandtestGetStart(f->eventId,
                                                 &eventResult, &eventStReturn, &funcError);
            if ( eventError == PHEVENT_ERR_OK )
                switch ( eventResult )
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
                        return eventStReturn;
                    default:
                        /* not expected, this is a bug, print an error and
                           try to recover during the next iteration */
                        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                                          "unexpected return value (%ld) from handtest GetParts",
                                          (long) eventResult);
                        funcError = PHFUNC_ERR_TIMEOUT;
                        break;
                }
        }
        else
        {
            /* this is the regular case !!!!!!! */
            funcError = phPlugFuncGetStart(f->funcId);
        }

        /* analyze result from get parts call and take action */
        phFrameHandlePluginResult(f, funcError, PHFUNC_AV_START, &completed, &success, &retVal);
    }
    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
                      "stopped waiting for device(s) after %g seconds", 
                      phFrameStopTimer(f->totalTimer));


    /* set SmarTest site usage according to the stored site state */
    if ( success )
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
                          "got device(s)");
        if ( !setSiteSetup(f) )
            retVal = PHTCOM_CI_CALL_ERROR;
    }

    return retVal;
}

/*****************************************************************************
 *
 * bin current devices
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary device bin actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameDeviceDone(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int success = 0;
    int completed = 0;

    phStateTesterMode_t testerMode = PHSTATE_TST_NORMAL;
    phStateDrvMode_t driverMode = PHSTATE_DRV_NORMAL;

    phEventError_t eventError;
    phEventResult_t eventResult;
    phTcomUserReturn_t eventStReturn;

    phFuncError_t funcError = PHFUNC_ERR_OK;

    /* pseudo code:
     * 
     * if (retest or check device)
     *     write log
     *     return
     * endif
     *
     * if (current devices skipped)
     *     write log
     *     bin to retest
     * else
     *     get bin information
     * endif
     *
     * if (handtest)
     *     handtest event: bin devices
     * else
     *     handler bin devices
     * endif
     */

    testerMode = PHSTATE_TST_NORMAL;
    // get current tester mode
    getCurrentTesterMode(f, &testerMode);
    switch (testerMode)
    {
      case PHSTATE_TST_NORMAL:
	/* this is the regular case.... */
	f->binAction = PHFRAME_BINACT_DOBIN;
	break;
      case PHSTATE_TST_RETEST:
	/* retesting a device */
	phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
	    "device(s) scheduled for retest,\n"
	    "no devices are binned now");
	f->binAction = PHFRAME_BINACT_SKIPBIN;
	break;
      case PHSTATE_TST_CHECK:
	/* checking a device */
	phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
	    "device(s) scheduled for check,\n"
	    "no devices are binned now");
	f->binAction = PHFRAME_BINACT_SKIPBIN;
	break;
    }
    
    /* early return, if do not bin now */
    if (f->binAction == PHFRAME_BINACT_SKIPBIN)
	return retVal;

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

    /* this is the big working loop: bins for parts are determined,
       now bin them until done or aborted by operator */
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    
    while (!success && !completed)
    {
	phStateGetDrvMode(f->stateId, &driverMode);
	if (driverMode == PHSTATE_DRV_HAND || 
	    (driverMode == PHSTATE_DRV_SST_HAND))
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
		"binning device(s) in hand test mode");
	    eventError = phEventHandtestBinDevice(f->eventId,
		&eventResult, &eventStReturn, f->deviceBins);
	    if (eventError == PHEVENT_ERR_OK)
		switch (eventResult)
		{
		  case PHEVENT_RES_HANDLED:
		    /* Bin devices has been simulated, go on with
		       success, etc... funcError was set accordingly */
		    funcError = PHFUNC_ERR_OK;
		    break;
		  case PHEVENT_RES_ABORT:
		    /* testprogram aborted by operator, QUIT flag has
		       been set, return to test cell client with
		       suggest value */
		    phLogFrameMessage(f->logId, PHLOG_TYPE_FATAL,
			"aborting handler driver");
		    return eventStReturn;
		  default:
		    /* not expected, this is a bug, print an error and
		       try to recover during the next iteration */
		    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		       "unexpected return value (%ld) from handtest BinDevice",
			(long) eventResult);
		    funcError = PHFUNC_ERR_BINNING;
		    break;
		}
	}
	else
	{
	    /* this is the regular case !!!!!!! */
	    funcError = phPlugFuncBinDevice(f->funcId, f->deviceBins);
	}

	/* analyze result from get parts call and take action */
	phFrameHandlePluginResult(f, funcError, PHFUNC_AV_BIN, &completed, &success, &retVal);

	if (funcError == PHFUNC_ERR_BINNING)
	    if (phFramePanic(f, 
		"SEVERE ERROR WHICH MIGHT AFFECT DEVICE TEST QUALITY"))
		return PHTCOM_CI_CALL_ERROR;
	
    }
    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
	"stopped binning device(s) after %g seconds", 
	phFrameStopTimer(f->totalTimer));

    /* reset SmarTest site usage according to the stored site state */
    if (success)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
	    "binned device(s)");
	if (!clearSiteSetup(f))
	    retVal = PHTCOM_CI_CALL_ERROR;
    }
    else
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "binning device(s) stopped unsuccessfully");
    }

    return retVal;
}

/*****************************************************************************
 *
 * Reprobe current devices, if possible
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
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int success = 0;
    int completed = 0;
    int success2 = 0;
    int completed2 = 0;
    int success3 = 0;
    int completed3 = 0;
    int implicitCall = 0;

    phStateDrvMode_t driverMode;
    phFuncError_t funcError;
    phEventError_t eventError;
    phEventResult_t eventResult;
    phTcomUserReturn_t eventStReturn;
    phStateLevel_t currentState = PHSTATE_LEV_DONTCARE;

/*    int done = 0;*/
    int putBackInPause = 0;
    int handlerPaused = 0;
/*    phEventResult_t exceptionResult = PHEVENT_RES_VOID;*/
/*    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;*/

    /* find out whether we are called during a pause operation or
       during normal execution of a device test */
    phStateGetLevel(f->stateId, &currentState);
    switch (currentState)
    {
      case PHSTATE_LEV_DEV_STARTED:     /* implicitly called by ph_reprobe from SmarTest */
      case PHSTATE_LEV_DEV_DONE:        /* implicitly called by bin to retest */
	implicitCall = 1;
	break;
      case PHSTATE_LEV_PAUSE_STARTED:
	/* explicitely called by Operator, nothing to do */
	break;
      default:
	/* unexpected call, print warning and behave like in the
           explicit call */
	phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
	    "ph_reprobe() called while not expected, trying to reprobe all devices");
	break;
    }

    /* In implicit reprobe it is configurable whether we should try to
       reprobe only the devices which are marked for reprobe by
       SmarTest or whether we just reprobe everything */
    if (implicitCall && f->autoReprobe == PHFRAME_RPMODE_OFF)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
	    "ph_reprobe() is not performed due to set configuration value of \"%s\"",
	    PHKEY_RP_AMODE);
	return retVal;
    }
    
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    
    phStateGetDrvMode(f->stateId, &driverMode);
    if (!implicitCall ||
        (implicitCall && (f->autoReprobe == PHFRAME_RPMODE_ALL)))
    {
	/* this is an explicit call or it is an implicit call, and we
           want to reprobe all devices. The site population as known
           by SmarTest does not change */

	if ((f->funcAvail & PHFUNC_AV_REPROBE))
	{

	    phEstateAGetPauseInfo(f->estateId, &handlerPaused);
	    if (handlerPaused && (f->funcAvail & PHFUNC_AV_UNPAUSE))
	    {
		while (!success2 && !completed2)
		{
		    funcError = phPlugFuncSTUnpaused(f->funcId);
		    phFrameHandlePluginResult(f, funcError, PHFUNC_AV_UNPAUSE, 
			&completed2, &success2, &retVal);
		}
		putBackInPause = 1;
	    }

	    while (!success && !completed)
	    {
		if (driverMode == PHSTATE_DRV_HAND || 
		    driverMode == PHSTATE_DRV_SST_HAND)
		{
		    phLogFrameMessage(f->logId, LOG_DEBUG,
			"reprobing device(s) in hand test mode");
		    funcError = PHFUNC_ERR_TIMEOUT;
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
			    funcError = PHFUNC_ERR_TIMEOUT;
			    break;
			}
		}
		else
		{
		    /* this is the regular case !!!!!!! */
		    funcError = phPlugFuncReprobe(f->funcId);
		}
		/* analyze result from reprobe call and take action */
		phFrameHandlePluginResult(f, funcError, PHFUNC_AV_REPROBE, 
		    &completed, &success, &retVal);
		
		if (funcError == PHFUNC_ERR_BINNING)
		    if (phFramePanic(f, 
			"SEVERE ERROR WHICH MIGHT AFFECT DEVICE TEST QUALITY"))
			return PHTCOM_CI_CALL_ERROR;
	    }

	    if (putBackInPause && (f->funcAvail & PHFUNC_AV_PAUSE))
	    {
		while (!success3 && !completed3)
		{
		    funcError = phPlugFuncSTPaused(f->funcId);
		    phFrameHandlePluginResult(f, funcError, PHFUNC_AV_PAUSE, 
			&completed3, &success3, &retVal);
		}
	    }

	}
	else
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
		"ph_reprobe() is not available for the current handler driver");
	}

    }
    else
    {
	/* this is an implicit call and we want to reprobe on a per
           site basis. At the same time some devices may be binned and
           disappear from the site population data */

	if ((f->funcAvail & PHFUNC_AV_BINREPR))
	{
	    /* prepare binning first */
	    if (!prepareReprobeBinning(f))
	    {
		/* error during binning preparation */
		return PHTCOM_CI_CALL_ERROR;
	    }

	    while (!success && !completed)
	    {
		if (driverMode == PHSTATE_DRV_HAND || 
		    driverMode == PHSTATE_DRV_SST_HAND)
		{
		    phLogFrameMessage(f->logId, LOG_DEBUG,
			"binning and reprobing device(s) in hand test mode");
		    funcError = PHFUNC_ERR_TIMEOUT;
		    eventError = phEventHandtestBinReprobe(f->eventId,
			&eventResult, &eventStReturn,
			f->deviceToReprobe, f->deviceBins, &funcError);
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
				"unexpected return value (%ld) from handtest BinReprobe",
				(long) eventResult);
			    funcError = PHFUNC_ERR_TIMEOUT;
			    break;
			}
		}
		else
		{
		    /* this is the regular case !!!!!!! */
		    funcError = phPlugFuncBinReprobe(f->funcId, f->deviceToReprobe, f->deviceBins);
		}
		/* analyze result from reprobe call and take action */
		phFrameHandlePluginResult(f, funcError, PHFUNC_AV_BINREPR, 
		    &completed, &success, &retVal);

		if (funcError == PHFUNC_ERR_BINNING)
		    if (phFramePanic(f, 
			"SEVERE ERROR WHICH MIGHT AFFECT DEVICE TEST QUALITY"))
			return PHTCOM_CI_CALL_ERROR;
	    }
	}
	else
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
		"ph_reprobe() with implicit binning is not available for the current handler driver");
	}
    }
    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
	"stopped reprobing device(s) after %g seconds", 
	phFrameStopTimer(f->totalTimer));

    /* set SmarTest site usage according to the stored site state */
    if (success)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_1,
	    "reprobed device(s)");
	if (!setSiteSetup(f))
	    retVal = PHTCOM_CI_CALL_ERROR;
    }
    else
    {
	if (implicitCall)
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"reprobing device(s) stopped unsuccessfully, will enter pause");
	    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_PAUSE, 1L);
	}
	else
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"reprobing device(s) stopped unsuccessfully");
    }

    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
