/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 * *----------------------------------------------------------------------------- *
 * MODULE   : ph_tcom.c
 * CREATED  : 19 Jul 1999
 *
 * CONTENTS : Tester communication interface
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 19 Jul 1999, Michael Vogt, created
 *            22 Jul 1999, Chris Joyce, implemented functions
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

#include "ph_tools.h"
#include "ph_tcom.h"
#include "ph_tcom_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define PH_SESSION_CHECK

#ifdef DEBUG
#define PH_SESSION_CHECK
#endif

#ifdef PH_SESSION_CHECK
#define CheckSession(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHTCOM_ERR_INVALID_HDL
#else
#define CheckSession(x)
#endif

static char simulatorString[MAX_FLAG_STRING_LEN];
static long simSysFlags[PHTCOM_SF__END][SIMULATED_SITES+1];
static struct {char binCode[BIN_CODE_LENGTH]; int binNumber;} simBins[SIMULATED_SITES+1];
static long *simButtonValues = NULL;
static int simButtonCount = 0;
static int simButtonPos = 0;
static long simDiePositionX[SIMULATED_SITES+1] = {0};
static long simDiePositionY[SIMULATED_SITES+1] = {0};
static char simWaferDescriptionFile[SIMULATED_MAX_PATH_LEN+1] = "";
static long simWaferDimMinX = 0;
static long simWaferDimMaxX = 0;
static long simWaferDimMinY = 0;
static long simWaferDimMaxY = 0;
static long simWaferDimQuadrant = 0;
static long simWaferDimOrientation = 0;

static char testlogSetList[1024];
static char testlogGetList[1024];

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

long  MsgDisplayMsgGetResponse(char *msg, char *b2s, char *b3s,
      char *b4s, char *b5s, char *b6s, char *b7s,
      long *response)
{
  return 0;
}

long  MsgDisplayMsgGetStringResponse(char *msg, char *b2s,
      char *b3s, char *b4s, char *b5s, char *b6s,
      char *b7s, long *response, char *string)
{
  return 0;
}

static char *testlogShort(phTcomSystemFlag_t flag)
{
    static char name[8];

    switch (flag)
    {
      case PHTCOM_SF_CI_ABORT:
	strcpy(name, "AB"); break;
      case PHTCOM_SF_CI_BAD_PART:
	strcpy(name, "BP"); break;
      case PHTCOM_SF_CI_BIN_CODE:
	strcpy(name, "BC"); break;
      case PHTCOM_SF_CI_BIN_NUMBER:
	strcpy(name, "BN"); break;
      case PHTCOM_SF_CI_CHECK_DEV:
	strcpy(name, "CD"); break;
      case PHTCOM_SF_CI_GOOD_PART:
	strcpy(name, "GP"); break;
      case PHTCOM_SF_CI_LEVEL_END:
	strcpy(name, "LE"); break;
      case PHTCOM_SF_CI_PAUSE:
	strcpy(name, "PA"); break;
      case PHTCOM_SF_CI_QUIT:
	strcpy(name, "QU"); break;
      case PHTCOM_SF_CI_RESET:
	strcpy(name, "RS"); break;
      case PHTCOM_SF_CI_RETEST:
	strcpy(name, "RD"); break;
      case PHTCOM_SF_CI_SITE_REPROBE:
	strcpy(name, "SR"); break;
      case PHTCOM_SF_CI_SITE_SETUP:
	strcpy(name, "SS"); break;
      case PHTCOM_SF_CI_SKIP:
	strcpy(name, "SK"); break;
      case PHTCOM_SF_CI_RETEST_W:
	strcpy(name, "RW"); break;
      case PHTCOM_SF_CI_REFERENCE:
	strcpy(name, "RF"); break;
      default:
	strcpy(name, "XX"); break;
    }

    return name;
}

static void testlogSet(phTcomSystemFlag_t flag, long value)
{
    char oneFlag[64];
    char oneFlagVal[64];
    sprintf(oneFlag, "%s", testlogShort(flag));
    sprintf(oneFlagVal, "%s%ld", oneFlag, value);
    if (strlen(testlogSetList) != 0)
    {
	if (strstr(testlogSetList, oneFlagVal))
	    return;
	else if (strstr(testlogSetList, oneFlag))
	    strcat(testlogSetList, ",***");
	else
	    strcat(testlogSetList, ",");
    }
    strcat(testlogSetList, oneFlagVal);
}

static void testlogSetOfSite(phTcomSystemFlag_t flag, long site, long value)
{
    char oneFlag[64];
    char oneFlagVal[64];
    sprintf(oneFlag, "%s%ld", testlogShort(flag), site);
    sprintf(oneFlagVal, "%s:%ld", oneFlag, value);
    if (strlen(testlogSetList) != 0)
    {
	if (strstr(testlogSetList, oneFlagVal))
	    return;
	else if (strstr(testlogSetList, oneFlag))
	    strcat(testlogSetList, ",***");
	else
	    strcat(testlogSetList, ",");
    }
    strcat(testlogSetList, oneFlagVal);
}

static void testlogGet(phTcomSystemFlag_t flag, long value)
{
    char oneFlag[64];
    char oneFlagVal[64];
    sprintf(oneFlag, "%s", testlogShort(flag));
    sprintf(oneFlagVal, "%s%ld", oneFlag, value);
    if (strlen(testlogGetList) != 0)
    {
	if (strstr(testlogGetList, oneFlagVal))
	    return;
	else if (strstr(testlogGetList, oneFlag))
	    strcat(testlogGetList, ",***");
	else
	    strcat(testlogGetList, ",");
    }
    strcat(testlogGetList, oneFlagVal);
}

static void testlogGetOfSite(phTcomSystemFlag_t flag, long site, long value)
{
    char oneFlag[64];
    char oneFlagVal[64];
    sprintf(oneFlag, "%s%ld", testlogShort(flag), site);
    sprintf(oneFlagVal, "%s:%ld", oneFlag, value);
    if (strlen(testlogGetList) != 0)
    {
	if (strstr(testlogGetList, oneFlagVal))
	    return;
	else if (strstr(testlogGetList, oneFlag))
	    strcat(testlogGetList, ",***");
	else
	    strcat(testlogGetList, ",");
    }
    strcat(testlogGetList, oneFlagVal);
}


static char *getSystemFlagString(phTcomSystemFlag_t sflag)
{
    static char systemFlagString[MAX_FLAG_STRING_LEN];

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
      case PHTCOM_SF_CI_SITE_REPROBE:
        sprintf(systemFlagString, "site_reprobe");
        break;
      case PHTCOM_SF_CI_SITE_SETUP:
        sprintf(systemFlagString, "site_setup");
        break;
      case PHTCOM_SF_CI_SKIP:
        sprintf(systemFlagString, "skip");
        break;
      case PHTCOM_SF_CI_REFERENCE:
        sprintf(systemFlagString, "reference");
        break;
      case PHTCOM_SF_CI_RETEST_W:
        sprintf(systemFlagString, "retest_w");
        break;
      default:
        sprintf(systemFlagString, "Unknown PHTCOM System flag!");
        break;
    }
    return &systemFlagString[0];
}

/*****************************************************************************
 *
 * Initialize a communication back to the CPI and TPI interface
 *
 * Authors: Chris Joyce
 *
 * Description:
 *
 * Please refer to ph_tcom.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phTcomError_t phTcomInit(
    phTcomId_t *testerID         /* the resulting tester ID */,
    phLogId_t loggerID           /* ID of the logger to use */,
    phTcomMode_t mode            /* the communication mode (for future
                                    improvements, currently only
				    PHTCOM_MODE_ONLINE is valid) */
)
{
    phTcomError_t rtnValue=PHTCOM_ERR_OK;
    struct phTcomStruct *tmpId;
    phTcomSystemFlag_t flag;
    int site;
    
    /* set testerID to NULL in case anything goes wrong */
    *testerID = NULL;

    phLogTesterMessage(loggerID,
                    PHLOG_TYPE_TRACE,
                    "phTcomInit(P%p, P%p, %s)",
                    testerID,
                    loggerID,
                    mode==PHTCOM_MODE_ONLINE ? "ONLINE" : "SIMULATED");

    /* allocate new communication data type */
    tmpId = PhNew(struct phTcomStruct);
    if (tmpId == 0)
    {
        phLogTesterMessage(loggerID,
                           PHLOG_TYPE_FATAL,
                           "not enough memory during call to phTcomInit()");
        return PHTCOM_ERR_MEMORY;
    }

    /* if have not returned yet everything is okay */
    tmpId -> myself = tmpId;
    tmpId -> mode = mode;
    tmpId -> loggerId = loggerID;
    tmpId -> createGetTestLog = 0;
    tmpId -> createSetTestLog = 0;
    tmpId -> quitAlreadySet = 0;
    tmpId -> abortAlreadySet = 0;


    /* set-up simulation variables */
    strcpy(simulatorString,"sim string");
    for (flag = PHTCOM_SF_CI_ABORT; flag < PHTCOM_SF__END; flag = (phTcomSystemFlag_t)(flag + 1))
    {
      for (site = 0; site <= SIMULATED_SITES; site++)
      {
        if(flag == PHTCOM_SF_CI_SITE_REPROBE)
        {
          //SMT8 doesn't have reprobe bin, so assume all
          //the sites need to be reprobed when PHSession::reprobe() is called
          simSysFlags[flag][site] = 1;
        }
        else
        {
          simSysFlags[flag][site] = 0;
        }
      }
    }



    *testerID  = tmpId;
    return rtnValue;
}



/*****************************************************************************
 *
 * Destroy a tester communication interface
 *
 * Authors: Chris Joyce
 *
 * Description:
 *
 * Please refer to ph_tcom.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phTcomError_t phTcomDestroy(
    phTcomId_t testerID          /* the tester ID */
)
{
    phTcomError_t rtnValue=PHTCOM_ERR_OK;

    CheckSession(testerID);
    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomDestroy(%p)",
                    testerID);

    /* make phComId invalid */
    testerID -> myself = NULL;
    free(testerID);

    return rtnValue;
}

/*****************************************************************************
 * Testlog mode: clear all get system flags
 *****************************************************************************/
phTcomError_t phTcomLogStartGetList(phTcomId_t testerID)
{
    testerID->createGetTestLog = 1;
    testlogGetList[0] = '\0';
    return PHTCOM_ERR_OK;
}

/*****************************************************************************
 * Testlog mode: clear all set system flags
 *****************************************************************************/
phTcomError_t phTcomLogStartSetList(phTcomId_t testerID)
{
    testerID->createSetTestLog = 1;
    testlogSetList[0] = '\0';
    return PHTCOM_ERR_OK;
}

/*****************************************************************************
 * Testlog mode: return all get system flags
 *****************************************************************************/
char *phTcomLogStopGetList(phTcomId_t testerID)
{
    testerID->createGetTestLog = 0;
    return testlogGetList;
}

/*****************************************************************************
 * Testlog mode: return all set system flags
 *****************************************************************************/
char *phTcomLogStopSetList(phTcomId_t testerID)
{
    testerID->createSetTestLog = 0;
    return testlogSetList;
}

/*****************************************************************************
 * Simulation mode: clear all system flags
 *****************************************************************************/
phTcomError_t phTcomSimClearSystemFlags(void)
{
    phTcomSystemFlag_t flag;
    int site;
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    for (flag = PHTCOM_SF_CI_ABORT; flag < PHTCOM_SF__END; flag = (phTcomSystemFlag_t)(flag + 1))
    {
      for (site = 0; site <= SIMULATED_SITES; site++)
      {
        if(flag == PHTCOM_SF_CI_SITE_REPROBE)
        {
          //SMT8 doesn't have reprobe bin, so assume all
          //the sites need to be reprobed when PHSession::reprobe() is called
          simSysFlags[flag][site] = 1;
        }
        else
        {
          simSysFlags[flag][site] = 0;
        }
      }
    }

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: set a system flag
 *****************************************************************************/
phTcomError_t phTcomSimSetSystemFlag(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long value                   /* value */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    simSysFlags[flag][0] = value;

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: set a system flag of site
 *****************************************************************************/
phTcomError_t phTcomSimSetSystemFlagOfSite(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long site                    /* which site to refer to */,
    long value                   /* value */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    if (site>0 && site <= SIMULATED_SITES)
	simSysFlags[flag][site] = value;
    else
    {
	fprintf(stderr, "Tester SIMULATION: "
	    "illegale site %ld requested, simulation restricted to 1 to %d\n",
	    site, SIMULATED_SITES);
	phRtnValue = PHTCOM_ERR_ERROR;
    }

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: set die position xy
 *****************************************************************************/

phTcomError_t phTcomSimSetDiePosXY(
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    simDiePositionX[0] = x;
    simDiePositionY[0] = y;

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: set die position xy of site 
 *****************************************************************************/

phTcomError_t phTcomSimSetDiePosXYOfSite(
    long site                    /* which site to refer to */,
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    if (site>0 && site <= SIMULATED_SITES)
    {
	simDiePositionX[site] = x;
	simDiePositionY[site] = y;
    }
    else
    {
	fprintf(stderr, "Tester SIMULATION: "
	    "illegale site %ld requested, simulation restricted to 1 to %d\n",
	    site, SIMULATED_SITES);
	phRtnValue = PHTCOM_ERR_ERROR;
    }

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: set wafer description file 
 *****************************************************************************/

phTcomError_t phTcomSimSetWaferDescriptionFile(
    char *path                   /* wafer description file name */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    strcpy(simWaferDescriptionFile, "");
    strncat(simWaferDescriptionFile, path, SIMULATED_MAX_PATH_LEN);

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: set wafer dimensions
 *****************************************************************************/

phTcomError_t phTcomSimSetWaferDimensions(
    long minX                    /* minimum x value */,
    long maxX                    /* maximum x value */, 
    long minY                    /* minimum y value */,
    long maxY                    /* maximum y value */, 
    long quadrant                /* which quadrant 1-4 defines one of 
                                    increasing value */,
    long orientation             /* angle of flatted wafer */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    simWaferDimMinX=minX;
    simWaferDimMaxX=maxX;
    simWaferDimMinY=minY;
    simWaferDimMaxY=maxY;
    simWaferDimQuadrant=quadrant;
    simWaferDimOrientation=orientation;

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: get a system flag
 *****************************************************************************/

phTcomError_t phTcomSimGetSystemFlag(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long *value                  /* value */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    *value = simSysFlags[flag][0];

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: get a system flag of site
 *****************************************************************************/

phTcomError_t phTcomSimGetSystemFlagOfSite(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long site                    /* which site to refer to */,
    long *value                  /* value */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    if (site>0 && site <= SIMULATED_SITES)
	*value = simSysFlags[flag][site];
    else
    {
	fprintf(stderr, "Tester SIMULATION: "
	    "illegale site %ld requested, simulation restricted to 1 to %d\n",
	    site, SIMULATED_SITES);
	phRtnValue = PHTCOM_ERR_ERROR;
    }

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: define user button presses
 *****************************************************************************/

phTcomError_t phTcomSimDefineButtons(
    int count        /* the number of buttons defined in the button array */,
    long *buttonArray /* the button array, values will be used one
			after the other until <count> is hit. After
			that, simulation always returns 'Continue' */
)
{
    simButtonValues = buttonArray;
    simButtonCount = count;
    simButtonPos = 0;

    return PHTCOM_ERR_OK;
}

/*****************************************************************************
 * Simulation mode: get die position xy 
 *****************************************************************************/

phTcomError_t phTcomSimGetDiePosXY(
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    *x = simDiePositionX[0];
    *y = simDiePositionY[0];

    return phRtnValue;
}

/*****************************************************************************
 * Simulation mode: get die position xy of site 
 *****************************************************************************/

phTcomError_t phTcomSimGetDiePosXYOfSite(
    long site                    /* which site to refer to */,
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    if (site>0 && site <= SIMULATED_SITES)
    {
	*x = simDiePositionX[site];
	*y = simDiePositionY[site];
    }
    else
    {
	fprintf(stderr, "Tester SIMULATION: "
	    "illegale site %ld requested, simulation restricted to 1 to %d\n",
	    site, SIMULATED_SITES);
	phRtnValue = PHTCOM_ERR_ERROR;
    }

    return phRtnValue;
}


/*****************************************************************************
 * Simulation mode: get wafer description file 
 *****************************************************************************/

phTcomError_t phTcomSimGetWaferDescriptionFile(
    char *path                   /* wafer description file name */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    strcpy(path, "");
    strncat(path, simWaferDescriptionFile, SIMULATED_MAX_PATH_LEN - 1);

    if ( !(*simWaferDescriptionFile) )
    {
        phRtnValue=PHTCOM_ERR_ERROR;
    }

    return phRtnValue;
}



/*****************************************************************************
 * Simulation mode: get wafer dimensions 
 *****************************************************************************/

phTcomError_t phTcomSimGetWaferDimensions(
    long *minX                   /* minimum x value */,
    long *maxX                   /* maximum x value */, 
    long *minY                   /* minimum y value */,
    long *maxY                   /* maximum y value */, 
    long *quadrant               /* which quadrant 1-4 defines one of 
                                    increasing value */,
    long *orientation            /* angle of flatted wafer */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    *minX=simWaferDimMinX;
    *maxX=simWaferDimMaxX;
    *minY=simWaferDimMinY;
    *maxY=simWaferDimMaxY;
    *quadrant=simWaferDimQuadrant;
    *orientation=simWaferDimOrientation;

    return phRtnValue;
}

/*****************************************************************************
 * Call GetModelfileString() from CPI
 *****************************************************************************/
phTcomError_t phTcomGetModelfileString(
    phTcomId_t testerID         /* the tester ID */,
    char *name                  /* model file variable */,
    char *value                 /* returned value */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;
    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomGetModelfileString(P%p, \"%s\", P%p)",
                    testerID,
                    name,
                    value);

    value=simulatorString;
    phLogTesterMessage(testerID->loggerId,
                       LOG_NORMAL,
                       "GetModelfileString(%s) return %s",
                       name,
                       value);

    return phRtnValue;
}



/*****************************************************************************
 * Call GetSystemFlag() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetSystemFlag(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */,
    long *value                  /* returned value */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    char* flagString = NULL;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomGetSystemFlag(P%p, %d, P%p)",
                    testerID,
                    flag,
                    value);

    flagString=getSystemFlagString(flag);

    phLogTesterMessage(testerID->loggerId,
                    LOG_VERBOSE,
                    "flag %d converted to %s",
                    flag,
                    flagString);

    *value = simSysFlags[flag][0];
    phLogTesterMessage(testerID->loggerId,
                       LOG_NORMAL,
                       "GetSystemFlag %s return %ld",
                       flagString,
                       *value);

    /* if the quit/abort is already set by the driver, we need to return it */
    if(flag == PHTCOM_SF_CI_QUIT)
    {
        if(*value == 1) testerID->quitAlreadySet = 1;
        *value = *value || testerID->quitAlreadySet;
    }

    if((flag == PHTCOM_SF_CI_ABORT || flag == PHTCOM_SF_CI_RESET))
    {
        if(*value == 1) testerID->abortAlreadySet = 1;
        *value = *value || testerID->abortAlreadySet;
    }

    phLogTesterMessage(testerID->loggerId,
                       LOG_DEBUG,
                       "GetSystemFlag %s final return %ld",
                       flagString,
                       *value);

    if (testerID->createGetTestLog)
	testlogGet(flag, *value);
    return phRtnValue;
}


/*****************************************************************************
 * Call GetSystemFlagOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetSystemFlagOfSite(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */,
    long site                    /* which site to refer to */,
    long *value                  /* returned value */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    char* flagString;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomGetSystemFlagOfSite(P%p, %d, %ld, P%p)",
                    testerID,
                    flag,
                    site,
                    value);

    flagString=getSystemFlagString(flag);

    phLogTesterMessage(testerID->loggerId,
                    LOG_VERBOSE,
                    "flag %d converted to %s",
                    flag,
                    flagString);

    if (site>0 && site <= SIMULATED_SITES)
    {
        *value = simSysFlags[flag][site];
        phLogTesterMessage(testerID->loggerId,
                           LOG_NORMAL,
                           "GetSystemFlagOfSite %s %ld return %ld",
                           flagString,
                           site,
                           *value);
    }
    else
    {
        phLogTesterMessage(testerID->loggerId, PHLOG_TYPE_ERROR,
                           "illegale site %ld requested, restricted to 1 to %d",
                           site, SIMULATED_SITES);
        *value=0;
        phRtnValue = PHTCOM_ERR_ERROR;
    }

    if (testerID->createGetTestLog)
	testlogGetOfSite(flag, site, *value);
    return phRtnValue;
}

/*****************************************************************************
 * msgDisplayMsg button set-up / display
 *****************************************************************************/
static char const* msgDisplayMsgButton[9];

static void setUpButtons(
    char const *b2s         /* user button 2 string */,
    char const *b3s         /* user button 3 string */,
    char const *b4s         /* user button 4 string */,
    char const *b5s         /* user button 5 string */,
    char const *b6s         /* user button 6 string */,
    char const *b7s         /* user button 7 string */
)
{
    /* set-up msgDisplayMsgButton array */
    msgDisplayMsgButton[1]="Quit";
    msgDisplayMsgButton[2]=b2s;
    msgDisplayMsgButton[3]=b3s;
    msgDisplayMsgButton[4]=b4s;
    msgDisplayMsgButton[5]=b5s;
    msgDisplayMsgButton[6]=b6s;
    msgDisplayMsgButton[7]=b7s;
    msgDisplayMsgButton[8]="Continue";
}


static char const* displayButton(int button)
{
    char const* buttonStr;

    if ( button >= 1 && button <= 8 )
    {
        buttonStr=msgDisplayMsgButton[button];
    }
    else
    {
        buttonStr="Undefined button";
    }

    return buttonStr;
}

/*****************************************************************************
 * Call MsgDisplayMsgGetResponse() from CPI
 *****************************************************************************/

phTcomError_t phTcomMsgDisplayMsgGetResponse(
    phTcomId_t testerID         /* the tester ID */,
    char *msg                   /* message to be displayed */,
    char *b2s                   /* user button 2 string */,
    char *b3s                   /* user button 3 string */,
    char *b4s                   /* user button 4 string */,
    char *b5s                   /* user button 5 string */,
    char *b6s                   /* user button 6 string */,
    char *b7s                   /* user button 7 string */,
    long *response              /* return button selected */
)
{

    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    CheckSession(testerID);

    /* set-up buttons array */
    setUpButtons(b2s,b3s,b4s,b5s,b6s,b7s);

#ifdef DEBUG
    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomMsgDisplayMsgGetResponse(P%p, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", P%p)",
                    testerID,
                    msg,
                    b2s,
                    b3s,
                    b4s,
                    b5s,
                    b6s,
                    b7s,
                    response);
#endif

    /* set-up buttons array */
    setUpButtons(b2s,b3s,b4s,b5s,b6s,b7s);

    if ( testerID->mode == PHTCOM_MODE_ONLINE )
    {
        if ( MsgDisplayMsgGetResponse(msg,b2s,b3s,b4s,b5s,b6s,b7s,response) == 0 ) /* 0 = ok */
        {
            phLogTesterMessage(testerID->loggerId,
		PHLOG_TYPE_MESSAGE_0,
		"User Dialog:\n"
		"%s\n"
		"User Dialog: 'Quit' '%s' '%s' '%s' '%s' '%s' '%s' 'Continue'\n"
		"User Dialog: User pressed '%s' ",
		msg,
		b2s,
		b3s,
		b4s,
		b5s,
		b6s,
		b7s,
		displayButton(*response));
        }
        else
        {
            phLogTesterMessage(testerID->loggerId,
                            PHLOG_TYPE_ERROR,
                            "phTcomMsgDisplayMsgGetResponse FAILED: disallowed");
            phRtnValue=PHTCOM_ERR_ERROR;
        }
    }
    else
    {
	if (simButtonValues && simButtonPos < simButtonCount)
	{
	    /* take a simulation value from the provided array */
	    *response = simButtonValues[simButtonPos];
	    simButtonPos++;
	}
	else
	    /* simulate continue */
	    *response = 8L;

        phLogTesterMessage(testerID->loggerId,
	    PHLOG_TYPE_MESSAGE_0,
	    "SIMULATOR User Dialog:\n"
	    "%s\n"
	    "User Dialog: 'Quit' '%s' '%s' '%s' '%s' '%s' '%s' 'Continue'\n"
	    "User Dialog: User pressed '%s' ",
	    msg,
	    b2s,
	    b3s,
	    b4s,
	    b5s,
	    b6s,
	    b7s,
	    displayButton(*response));
    }

    return phRtnValue;
}


/*****************************************************************************
 * Call MsgDisplayMsgGetStringResponse() from CPI
 *****************************************************************************/

phTcomError_t phTcomMsgDisplayMsgGetStringResponse(
    phTcomId_t testerID         /* the tester ID */,
    char *msg                   /* message to be displayed */,
    char *b2s                   /* user button 2 string */,
    char *b3s                   /* user button 3 string */,
    char *b4s                   /* user button 4 string */,
    char *b5s                   /* user button 5 string */,
    char *b6s                   /* user button 6 string */,
    char *b7s                   /* user button 7 string */,
    long *response              /* return button selected */,
    char *string                /* return string */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    CheckSession(testerID);

    /* set-up buttons array */
    setUpButtons(b2s,b3s,b4s,b5s,b6s,b7s);

#ifdef DEBUG
    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomMsgDisplayMsgGetStringResponse(P%p, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", P%p, P%p)",
                    testerID,
                    msg,
                    b2s,
                    b3s,
                    b4s,
                    b5s,
                    b6s,
                    b7s,
                    response,
                    string);
#endif

    /* set-up buttons array */
    setUpButtons(b2s,b3s,b4s,b5s,b6s,b7s);

    if ( testerID->mode == PHTCOM_MODE_ONLINE )
    {
        if ( MsgDisplayMsgGetStringResponse(msg,b2s,b3s,b4s,b5s,b6s,b7s,response,string) == 0 ) /* 0 = ok */
        {
            phLogTesterMessage(testerID->loggerId,
		PHLOG_TYPE_MESSAGE_0,
		"User Dialog:\n"
		"%s\n"
		"User Dialog: 'Quit' '%s' '%s' '%s' '%s' '%s' '%s' 'Continue'\n"
		"User Dialog: User entered \"%s\" and pressed '%s' ",
		msg,
		b2s,
		b3s,
		b4s,
		b5s,
		b6s,
		b7s,
		string,
		displayButton(*response));
        }
        else
        {
            phLogTesterMessage(testerID->loggerId,
                            PHLOG_TYPE_ERROR,
                            "phTcomMsgDisplayMsgGetStringResponse FAILED: disallowed");
            phRtnValue=PHTCOM_ERR_ERROR;
        }
    }
    else
    {
	if (simButtonValues && simButtonPos < simButtonCount)
	{
	    /* take a simulation value from the provided array */
	    *response = simButtonValues[simButtonPos];
	    simButtonPos++;
	}
	else
	    /* simulate continue */
	    *response = 8L;

        string=simulatorString;
        phLogTesterMessage(testerID->loggerId,
	    PHLOG_TYPE_MESSAGE_0,
	    "SIMULATOR User Dialog:\n"
	    "%s\n"
	    "User Dialog: 'Quit' '%s' '%s' '%s' '%s' '%s' '%s' 'Continue'\n"
	    "User Dialog: User entered \"%s\" and pressed '%s' ",
	    msg,
	    b2s,
	    b3s,
	    b4s,
	    b5s,
	    b6s,
	    b7s,
	    string,
	    displayButton(*response));

    }

    return phRtnValue;
}


/*****************************************************************************
 * Call GetDiePosXY() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetDiePosXY(
    phTcomId_t testerID          /* the tester ID */,
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                       PHLOG_TYPE_TRACE,
                       "phTcomGetDiePosXY(P%p, P%p, P%p)",
                       testerID, x, y);

    phRtnValue=phTcomSimGetDiePosXY(x,y);
    phLogTesterMessage(testerID->loggerId,
                       LOG_NORMAL,
                       "GetDiePosXY return x=%ld y=%ld",*x,*y);

    return phRtnValue;
}

/*****************************************************************************
 * Call GetDiePosXYOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetDiePosXYOfSite(
    phTcomId_t testerID          /* the tester ID */,
    long site                    /* which site to refer to */,
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomGetDiePosXYOfSite(P%p, %ld, P%p, P%p)",
                    testerID, site, x, y);

    phRtnValue=phTcomSimGetDiePosXYOfSite(site,x,y);
    if (phRtnValue != PHTCOM_ERR_OK)
    {
        phLogTesterMessage(testerID->loggerId, PHLOG_TYPE_ERROR,
                           "phTcomSimGetDiePosXYOfSite() failed ");
    }
    else
    {
        phLogTesterMessage(testerID->loggerId, LOG_NORMAL,
                           "GetDiePosXYOfSite(%ld) return x=%ld y=%ld",
                            site,*x,*y);
    }

    return phRtnValue;
}



/*****************************************************************************
 * Call GetWaferDescriptionFile() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetWaferDescriptionFile(
    char *path                   /* wafer description file name */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

#if 0
    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                       PHLOG_TYPE_TRACE,
                       "phTcomGetWaferDescriptionFile(P%p, P%p)",
                       testerID, path);
#endif


     phRtnValue=phTcomSimGetWaferDescriptionFile(path);

    return phRtnValue;
}


phTcomError_t phTcomSetWaferDescriptionFile(
    char *path                   /* wafer description file name */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

#if 0
    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                       PHLOG_TYPE_TRACE,
                       "phTcomGetWaferDescriptionFile(P%p, P%p)",
                       testerID, path);
#endif


     phRtnValue=phTcomSimSetWaferDescriptionFile(path);

    return phRtnValue;
}


/*****************************************************************************
 * Call GetWaferDimensions() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetWaferDimensions(
    phTcomId_t testerID          /* the tester ID */,
    long *minX                   /* minimum x value */,
    long *maxX                   /* maximum x value */, 
    long *minY                   /* minimum y value */,
    long *maxY                   /* maximum y value */, 
    long *quadrant               /* which quadrant 1-4 defines one of 
                                    increasing value */,
    long *orientation            /* angle of flatted wafer */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                       PHLOG_TYPE_TRACE,
                       "phTcomGetWaferDimensions(P%p, P%p, P%p, P%p, P%p, P%p, P%p)",
                       testerID, minX, maxX, minY, maxY, quadrant, orientation);

    phRtnValue=phTcomSimGetWaferDimensions(minX,maxX,minY,maxY,quadrant,orientation);
    phLogTesterMessage(testerID->loggerId,
                       LOG_NORMAL,
                       "phTcomSimGetWaferDimensions returns minX=%ld maxX=%ld minY=%ld maxY=%ld quadrant=%ld orientation=%ld",
                       *minX, *maxX, *minY, *maxY, *quadrant, *orientation);

    return phRtnValue;
}


/*****************************************************************************
 * Call SetModelfileString() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetModelfileString(
    phTcomId_t testerID          /* the tester ID */,
    char *name                   /* model file variable */,
    char *value                  /* value to set */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomSetModelfileString(P%p, %s, %s)",
                    testerID,
                    name,
                    value);

    phLogTesterMessage(testerID->loggerId,
                       LOG_NORMAL,
                       "SetModelfileString(%s) set %s",
                       name,
                       value);

    return phRtnValue;
}

/*****************************************************************************
 * Call SetSystemFlag() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetSystemFlag(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */,
    long value                   /* value to set */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    char* flagString;

    CheckSession(testerID);

    /* if the quit/abort is set by the driver, we need to save it; sometimes
       the values will be reset by external program which may have impact on
       the behavior of the driver. The driver should manage its own expectation and
       should not be impacted by other programs */
    if(flag == PHTCOM_SF_CI_QUIT)
        testerID->quitAlreadySet = value;

    if(flag == PHTCOM_SF_CI_ABORT || flag == PHTCOM_SF_CI_RESET)
        testerID->abortAlreadySet = value;

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomSetSystemFlag(P%p, %d, %ld)",
                    testerID,
                    flag,
                    value);

    flagString=getSystemFlagString(flag);

    phLogTesterMessage(testerID->loggerId,
                    LOG_VERBOSE,
                    "flag %d converted to \"%s\" ",
                    flag,
                    flagString);

    simSysFlags[flag][0] = value;
    phLogTesterMessage(testerID->loggerId,
                       LOG_NORMAL,
                       "SetSystemFlag %s set %ld",
                       flagString,
                       value);

    if (testerID->createSetTestLog)
	testlogSet(flag, value);
    return phRtnValue;
}


/*****************************************************************************
 * Call SetSystemFlagOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetSystemFlagOfSite(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */,
    long site                    /* which site to refer to */,
    long value                   /* value to set */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    char* flagString;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomSetSystemFlagOfSite(P%p, %d, %ld, %ld)",
                    testerID,
                    flag,
                    site,
                    value);

    flagString=getSystemFlagString(flag);

    phLogTesterMessage(testerID->loggerId,
                    LOG_VERBOSE,
                    "flag %d converted to \"%s\" ",
                    flag,
                    flagString);

    if (site>0 && site <= SIMULATED_SITES)
    {
        simSysFlags[flag][site] = value;
        phLogTesterMessage(testerID->loggerId,
                           LOG_NORMAL,
                           "SetSystemFlagOfSite(%s %ld) set %ld",
                           flagString,
                           site,
                           value);
    }
    else
    {
        phLogTesterMessage(testerID->loggerId, PHLOG_TYPE_ERROR,
                           "illegale site %ld requested, simulation restricted to 1 to %d",
                           site, SIMULATED_SITES);
        phRtnValue = PHTCOM_ERR_ERROR;
    }

    if (testerID->createSetTestLog)
      testlogSetOfSite(flag, site, value);

    return phRtnValue;
}

/*****************************************************************************
 * Call SetDiePosXY() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetDiePosXY(
    phTcomId_t testerID          /* the tester ID */,
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                       PHLOG_TYPE_TRACE,
                       "phTcomSetDiePosXY(P%p, %ld, %ld)",
                       testerID, x, y);

     phRtnValue=phTcomSimSetDiePosXY(x,y);
     phLogTesterMessage(testerID->loggerId,
                        LOG_NORMAL,
                        "phTcomSimSetDiePosXY x=%ld y=%ld", x, y);

    return phRtnValue;
}


/*****************************************************************************
 * Call SetDiePosXYOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetDiePosXYOfSite(
    phTcomId_t testerID          /* the tester ID */,
    long site                    /* which site to refer to */,
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                       PHLOG_TYPE_TRACE,
                       "phTcomSetDiePosXYOfSite(P%p, %ld, %ld, %ld)",
                       testerID, site, x, y);

    phRtnValue=phTcomSimSetDiePosXYOfSite(site,x,y);

    if (phRtnValue != PHTCOM_ERR_OK)
    {
        phLogTesterMessage(testerID->loggerId, PHLOG_TYPE_ERROR,
                           "phTcomSimSetDiePosXYOfSite error ");
    }
    else
    {
        phLogTesterMessage(testerID->loggerId, LOG_NORMAL,
                           "SetDiePosXYOfSite(%ld) x=%ld y= %ld ", site, x, y);
    }

    return phRtnValue;
}

/*****************************************************************************
 * Call SetWaferDimensions() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetWaferDimensions(
    phTcomId_t testerID          /* the tester ID */,
    long minX                    /* minimum x value */,
    long maxX                    /* maximum x value */, 
    long minY                    /* minimum y value */,
    long maxY                    /* maximum y value */, 
    long quadrant                /* which quadrant 1-4 defines one of 
                                    increasing value */,
    long orientation             /* angle of flatted wafer */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;
    long cpiRtnValue;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                       PHLOG_TYPE_TRACE,
                       "phTcomSetWaferDimensions(P%p, %ld, %ld, %ld, %ld, %ld, %ld)",
                       testerID, minX, maxX, minY, maxY, quadrant, orientation);

    phRtnValue=phTcomSimSetWaferDimensions(minX,maxX,minY,maxY,quadrant,orientation);
    phLogTesterMessage(testerID->loggerId,
                       LOG_NORMAL,
                       "phTcomSimSetWaferDimensions minX=%ld maxX=%ld minY=%ld maxY=%ld quadrant=%ld orientation=%ld",
                       minX,
                       maxX,
                       minY,
                       maxY,
                       quadrant,
                       orientation);

    return phRtnValue;
}


/*****************************************************************************
 * Simulation mode: clear all softbins code and hardbins number
 *****************************************************************************/
phTcomError_t phTcomSimClearBins(void)
{
    int site;
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    for (site = 0; site <= SIMULATED_SITES; site++)
    {
        strcpy(simBins[site].binCode, "db");
        simBins[site].binNumber = -1;
    }
    return phRtnValue;
}


/*****************************************************************************
 * Simulation mode: get softbin code and hardbin number
 *****************************************************************************/
phTcomError_t phTcomSimGetBin(
    char* softbin                /*   soft-bin code */,
    long* hardbin                /*   hard-bin number */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    if(softbin != NULL)
    {
        strcpy(softbin, simBins[0].binCode);
    }
    if(hardbin != NULL)
    {
        *hardbin = simBins[0].binNumber;
    }

    return phRtnValue;
}


/*****************************************************************************
 * Simulation mode: get softbin code and hardbin number of site
 *****************************************************************************/
phTcomError_t phTcomSimGetBinOfSite(
    long site                    /*   which site to refer to */,
    char* softbin                /*   soft-bin code */,
    long* hardbin                /*   hard-bin number */
)
{
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    if (site>0 && site <= SIMULATED_SITES)
    {
        if(softbin != NULL)
        {
            strcpy(softbin, simBins[site].binCode);
        }
        if(hardbin != NULL)
        {
            *hardbin = simBins[site].binNumber;
        }
    }
    else
    {
        fprintf(stderr, "illegale site %ld requested, restricted to 1 to %d\n",
                site, SIMULATED_SITES);
        phRtnValue = PHTCOM_ERR_ERROR;
    }

    return phRtnValue;
}


/*****************************************************************************
 * This function only use for dirver ART test stub
 *****************************************************************************/
phTcomError_t phTcomSetBinOfSite(
    phTcomId_t testerID          /*  the tester ID */,
    long site                    /*  which site to refer to */,
    const char* softbin          /*   soft-bin code */,
    long hardbin                 /*   hard-bin number */
)
{
    char binCode[BIN_CODE_LENGTH] = {0};
    if(softbin != NULL)
    {
        strncpy(binCode, softbin, BIN_CODE_LENGTH);
    }
    int binNumber = (int) hardbin;
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomSetBinOfSite(P%p, %ld, %s, %ld)",
                    testerID,
                    site,
                    binCode,
                    hardbin);

    if (site>0 && site <= SIMULATED_SITES)
    {
        strcpy(simBins[site].binCode, binCode);
        simBins[site].binNumber = binNumber;
        phLogTesterMessage(testerID->loggerId,
                           LOG_NORMAL,
                           "SetBinOfSite(%ld) set %s, %d",
                           site,
                           binCode,
                           binNumber);
    }
    else
    {
        phLogTesterMessage(testerID->loggerId, PHLOG_TYPE_ERROR,
                           "illegale site %ld requested, restricted to 1 to %d",
                           site, SIMULATED_SITES);
        phRtnValue = PHTCOM_ERR_ERROR;
    }

    return phRtnValue;
}


/*****************************************************************************
 * Call GetBinOfSite() from CPI
 *****************************************************************************/
phTcomError_t phTcomGetBinOfSite(
    phTcomId_t testerID          /*  the tester ID */,
    long site                    /*  which site to refer to */,
    char* softbin                /*  soft-bin code */,
    long* hardbin                /*  hard-bin number */
)
{
    char binCode[BIN_CODE_LENGTH+1] = {0};
    int binNumber = 0;
    phTcomError_t phRtnValue=PHTCOM_ERR_OK;

    CheckSession(testerID);

    phLogTesterMessage(testerID->loggerId,
                    PHLOG_TYPE_TRACE,
                    "phTcomGetBinOfSite(P%p, %ld, P%p, P%p)",
                    testerID,
                    site,
                    softbin,
                    hardbin);

    if (site>0 && site <= SIMULATED_SITES)
    {
        strcpy(binCode, simBins[site].binCode);
        binNumber = simBins[site].binNumber;
        phLogTesterMessage(testerID->loggerId,
                           LOG_NORMAL,
                           "GetBinOfSite %ld return %s, %d",
                           site,
                           strcmp(binCode,"")==0 ? "empty":binCode, binNumber);
    }
    else
    {
        phLogTesterMessage(testerID->loggerId, PHLOG_TYPE_ERROR,
                           "illegale site %ld requested, restricted to 1 to %ld",
                           site, SIMULATED_SITES);
        binCode[0]='\0';
        binNumber=0;
        phRtnValue = PHTCOM_ERR_ERROR;
    }

    if(softbin != NULL)
    {
        strcpy(softbin, binCode);
    }
    if(hardbin != NULL)
    {
        *hardbin = binNumber;
    }

    return phRtnValue;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
