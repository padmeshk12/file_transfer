/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : stepcontrol.c
 * CREATED  : 12 Jan 2000
 *
 * CONTENTS : Control of die and subdie stepping pattern
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 12 Jan 2000, Michael Vogt, created
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
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "sparsematrice.h"
#include "stepcontrol.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define ONE_LINE_LENGTH       1024
#define SPAT_START_STR        "STEPPING_PATTERN_START"
#define SPAT_START_LEN        22 /* strlen(SPAT_START_STR) */
#define SPAT_END_STR          "STEPPING_PATTERN_END"
#define SPAT_END_LEN          20 /* strlen(SPAT_END_STR) */



/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static int initDiePattern(struct stepPattern *sp)
{
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int x, y, z;*/
/*    phDieInfo_t *info = NULL;*/
/* End of Huatek Modification */
    phDieInfo_t *deflt = NULL;
    int i;

    sp->xStepList = PhNNew(long, sp->count);
    sp->yStepList = PhNNew(long, sp->count);
    if (!sp->xStepList || !sp->yStepList)
	return 0;

    deflt = PhNNew(phDieInfo_t, sp->sub);
    for (i=0; i<sp->sub; i++)
	deflt[i] = (phDieInfo_t) 0;

    sp->dieMap = initMatrice(sp->minX, sp->minY, sp->maxX, sp->maxY, 
	sp->count, sp->sub*sizeof(phDieInfo_t), deflt);
    free(deflt);

    if (!sp->dieMap)
	return 0;

#if 0
    for (x=sp->minX; x<=sp->maxX; x++)
    {
	for (y=sp->minY; y<=sp->maxY; y++)
	{
	    info = getElementRef(sp->dieMap, x, y);
	    for (z=0; info && z<sp->sub; z++)
	    {
		info[z] = (phDieInfo_t) 0;
	    }
	}
    }
#endif    

    return 1;
}

static int readWafermap(
    struct phFrameStruct *f             /* the framework data */
)
{
    int  param = 0, xmin = 0, ymin = 0, xmax = 0, ymax = 0, orientation = 0, quad = 0;
    char wafermap[CI_CPI_MAX_PATH_LEN + 1] = "";
    char oneLine[ONE_LINE_LENGTH + 1] = "";
    char *fgetsRetVal;

    FILE *waferFile = NULL;
    phTcomError_t tcomErr;
    long dieX, dieY;
    int stop;
    int i, j;

    long stepStart;
    phDieInfo_t *pDieInfo = NULL;

    /* get name of current wafer description file */
    tcomErr = phTcomGetWaferDescriptionFile(wafermap);
    if (tcomErr != PHTCOM_ERR_OK)
    {
#if 0
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not get name of wafer description file");
#endif
	return 0;
    }

    /* open the file */
    waferFile = fopen(wafermap, "r");
    if (!waferFile)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not open wafer description file '%s' for reading",
	    wafermap);
	return 0;
    }

#if 0
    /* parse and set the wafer dimensions */
    while ((fgets(oneLine,ONE_LINE_LENGTH,waferFile)) && (strcmp(oneLine,SPAT_START_STR) != 0))
    {
        if(sscanf(oneLine,"MIN_X:%d\n",&param) == 1)
            xmin = param;

        if(sscanf(oneLine,"MIN_Y:%d\n",&param) == 1)
            ymin = param;

        if(sscanf(oneLine,"MAX_X:%d\n",&param) == 1)
            xmax = param;

        if(sscanf(oneLine,"MAX_Y:%d\n",&param) == 1)
            ymax = param;

        if(sscanf(oneLine,"PHASE:%d\n",&param) == 1)
            orientation = param;

        if(sscanf(oneLine,"QUAD#:%d\n",&param) == 1)
            quad = param;
    }

    phTcomSetWaferDimensions(f->testerId, xmin, xmax, ymin, ymax, quad, orientation);

    rewind(waferFile);
#endif

    /* read until start of stepping pattern */
    do
    {
	fgetsRetVal = fgets(oneLine, ONE_LINE_LENGTH, waferFile);
	stop = strncmp(oneLine, SPAT_START_STR, SPAT_START_LEN) == 0 || 
	    !fgetsRetVal;
    }
    while (!stop);

    /* check for correct syntax */
    if (!fgetsRetVal)
    {
        fclose(waferFile);
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "'%s' not found in wafer description file '%s'",
	    SPAT_START_STR, wafermap);        
	return 0;	
    }

    /* store position for second run */
    stepStart = ftell(waferFile);
    if (stepStart == -1)
    {
        fclose(waferFile);;
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "can not get file position indicator of wafer description file '%s'",
	    wafermap);        
        return 0;
    }

    /* read the pattern (first run) */
    f->d_sp.count = 0;
    do
    {
	fgetsRetVal = fgets(oneLine, 1024, waferFile);
	stop = strncmp(oneLine, SPAT_END_STR, SPAT_END_LEN) == 0 ||
	    !fgetsRetVal;
	
	if (!stop)
	{
	    if (sscanf(oneLine, "%ld,%ld", &dieX, &dieY) != 2)
	    {
                fclose(waferFile);
		phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		    "could not parse die coordinates from \n"
		    "wafer description file '%s'", wafermap);
		return 0;
	    }
	    if (f->d_sp.count == 0)
	    {
		f->d_sp.minX = f->d_sp.maxX = dieX;
		f->d_sp.minY = f->d_sp.maxY = dieY;
	    }

	    f->d_sp.minX = dieX < f->d_sp.minX ? dieX : f->d_sp.minX;
	    f->d_sp.maxX = dieX > f->d_sp.maxX ? dieX : f->d_sp.maxX;
	    f->d_sp.minY = dieY < f->d_sp.minY ? dieY : f->d_sp.minY;
	    f->d_sp.maxY = dieY > f->d_sp.maxY ? dieY : f->d_sp.maxY;
	    f->d_sp.count++;
	}
    } while (!stop);

    /* check for correct syntax */
    if (!fgetsRetVal)
    {
        fclose(waferFile);
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "'%s' not found in wafer description file '%s'",
	    SPAT_END_STR, wafermap);
	return 0;	
    }

    /* prepare internal stepping pattern */

    f->d_sp.sub = f->isSubdieProbing ? f->sd_sp.count : 1;
    if (!initDiePattern(&(f->d_sp))) {
        fclose(waferFile);
	return 0;
    }

    /* do the second run, now collect the values. We don't expect any
       file errors here and we know how many entries to read */
    int returnValue = fseek(waferFile, stepStart, SEEK_SET);
    if(returnValue<0){
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
            "cannot set the pointer in wafer description file '%s'", wafermap);
    }
    for (i=0; i<f->d_sp.count; i++)
    {
	fgets(oneLine, 1024, waferFile);
	sscanf(oneLine, "%ld,%ld", &dieX, &dieY);
	f->d_sp.xStepList[i] = dieX;
	f->d_sp.yStepList[i] = dieY;
	if (f->isSubdieProbing)
	{
	    for (j=0; j<f->sd_sp.count; j++)
	    {
	        pDieInfo = (phDieInfo_t *) getElementRef(f->d_sp.dieMap, dieX, dieY);
	        if (pDieInfo != NULL)
	        {
	            pDieInfo[j] = PHFRAME_DIEINF_INUSE;
	        }
	    }
	}
	else
	{
	    pDieInfo = (phDieInfo_t *) getElementRef(f->d_sp.dieMap, dieX, dieY);
	    if (pDieInfo != NULL)
	    {
	        pDieInfo[0] = PHFRAME_DIEINF_INUSE;
	    }
	}
    }
    fclose(waferFile);

    return 1;
}


static int readSubDiePattern(
    struct phFrameStruct *f             /* the framework data */
)
{
    phConfType_t confType;
    int listSize = 0;
    int indexField[2];
    double numValue;
    long sdieX, sdieY;
    int i;
    phDieInfo_t *pSubDieInfo = NULL;

    /* first check, whether a sub-die pattern is configured at all */
    if (phConfConfIfDef(f->specificConfId, PHKEY_PR_SDPATTERN) !=
	PHCONF_ERR_OK)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "configuration \"%s\" is not given", PHKEY_PR_SDPATTERN);
	return 0;
    }

    /* check out how many step are there for the subdies. The
       configuration manager has already checked that the pattern
       definition is of correct type */
    phConfError_t confError = phConfConfType(f->specificConfId, PHKEY_PR_SDPATTERN, 0, NULL,
      &confType, &listSize);
    if(confError==PHCONF_ERR_OK){
      f->sd_sp.count = listSize;
    }

    /* find out the maximum and minimum values */
    for (i=0; i<f->sd_sp.count; i++)
    {
	indexField[0] = i;

	indexField[1] = 0;
	if(phConfConfNumber(f->specificConfId, PHKEY_PR_SDPATTERN, 
	    2, indexField, &numValue) == PHCONF_ERR_OK)
        {
	    sdieX = (long) numValue;
        }
        else
        {
	    sdieX = 9999;
        }

	indexField[1] = 1;
	if(phConfConfNumber(f->specificConfId, PHKEY_PR_SDPATTERN, 
	    2, indexField, &numValue) == PHCONF_ERR_OK)
        {
	    sdieY = (long) numValue;
        }
        else
        {
	    sdieY = 9999;
        }

	if (i == 0)
	{
	    f->sd_sp.minX = f->sd_sp.maxX = sdieX;
	    f->sd_sp.minY = f->sd_sp.maxY = sdieY;
	}

	f->sd_sp.minX = sdieX < f->sd_sp.minX ? sdieX : f->sd_sp.minX;
	f->sd_sp.maxX = sdieX > f->sd_sp.maxX ? sdieX : f->sd_sp.maxX;
	f->sd_sp.minY = sdieY < f->sd_sp.minY ? sdieY : f->sd_sp.minY;
	f->sd_sp.maxY = sdieY > f->sd_sp.maxY ? sdieY : f->sd_sp.maxY;
    }

    /* prepare internal stepping pattern */

    f->sd_sp.sub = 1;
    if (!initDiePattern(&(f->sd_sp)))
	return 0;

    /* do the second run, now collect the values. We don't expect any
       file errors here and we know how many entries to read */
    for (i=0; i<f->sd_sp.count; i++)
    {
	indexField[0] = i;

	indexField[1] = 0;
	if(phConfConfNumber(f->specificConfId, PHKEY_PR_SDPATTERN, 
	    2, indexField, &numValue) == PHCONF_ERR_OK)
        {
	    sdieX = (long) numValue;
        }
        else
        {
	    sdieX = 9999;
        }

	indexField[1] = 1;
	if(phConfConfNumber(f->specificConfId, PHKEY_PR_SDPATTERN, 
	    2, indexField, &numValue) == PHCONF_ERR_OK)
        {
	    sdieY = (long) numValue;
        }
        else
        {
	    sdieY = 9999;
        }

	f->sd_sp.xStepList[i] = sdieX;
	f->sd_sp.yStepList[i] = sdieY;
	pSubDieInfo = (phDieInfo_t *) getElementRef(f->sd_sp.dieMap, sdieX, sdieY);
	if (pSubDieInfo != NULL)
	{
	    pSubDieInfo[0] = PHFRAME_DIEINF_INUSE;
	}
    }
   
    return 1;
}



void phFrameStepControlInit(
    struct phFrameStruct *f             /* the framework data */
)
{
    int found = 0;
    int retValue = 0;

    /* find out who is going to control the stepping */
    phConfConfStrTest(&found, f->specificConfId, PHKEY_PR_STEPMODE,
	"smartest", "prober", "learnlist", NULL);
    switch (found)
    {
      case 0:
	phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
	    "configuration \"%s\" not given, assuming \"smartest\"",
	    PHKEY_PR_STEPMODE);
	f->warnings++;
	/* fall through into case 1 */
      case 1:
	f->d_sp.stepMode = PHFRAME_STEPMD_EXPLICIT;
	f->sd_sp.stepMode = PHFRAME_STEPMD_EXPLICIT;
	break;
      case 2:
	f->d_sp.stepMode = PHFRAME_STEPMD_AUTO;
	f->sd_sp.stepMode = PHFRAME_STEPMD_AUTO;
	break;
      case 3:
	f->d_sp.stepMode = PHFRAME_STEPMD_LEARN;
	f->sd_sp.stepMode = PHFRAME_STEPMD_LEARN;
	break;
      default:
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "unknown configuration value for \"%s\"", PHKEY_PR_STEPMODE);
	f->errors++;
	return;
    }

    /* find out whether we do sub-die probing */
    phConfConfStrTest(&found, f->specificConfId, PHKEY_PR_SUBDIE,
	"no", "yes", NULL);
    switch (found)
    {
      case 0:
	/* fall through, assume no sub-dies if not configured */
      case 1:
	f->isSubdieProbing = 0;
	f->d_sp.next = NULL;
	break;
      case 2:
	/* yes, we are doing sub-die probing today */
	f->isSubdieProbing = 1;
	f->d_sp.next = &(f->sd_sp);
	break;
      default:
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "unknown configuration value for \"%s\"", PHKEY_PR_SUBDIE);
	f->errors++;
	return;
    }

    /* prepare the sub-die stepping pattern, if necessary */
    if (f->isSubdieProbing &&
	(f->d_sp.stepMode == PHFRAME_STEPMD_EXPLICIT ||
	    f->d_sp.stepMode == PHFRAME_STEPMD_LEARN))
    {
	if (!readSubDiePattern(f))
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"for the given \"%s\" in conjunction with sub-die probing,\n"
		"the configuration value \"%s\" must be defined correctly\n",
		PHKEY_PR_STEPMODE, PHKEY_PR_SDPATTERN);
	    f->errors++;
	    return;	    
	}
    }

    retValue = readWafermap(f);
    /* prepare the die stepping pattern, if necessary */
    if (f->d_sp.stepMode == PHFRAME_STEPMD_EXPLICIT ||
	f->d_sp.stepMode == PHFRAME_STEPMD_LEARN)
    {
	if (retValue == 0)
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"for the given \"%s\", a wafer description\n"
		"file is a mandatory part of the testprogram.\n"
		"Please either define a wafer description file or chose a\n"
		"different \"%s\" in the prober driver configuration", 
		PHKEY_PR_SDPATTERN, PHKEY_PR_STEPMODE);
	    f->errors++;
	    return;
	}
    }
}

void phFrameStepResetPattern(phStepPattern_t *sp)
{
    int z;
    int c;
    phDieInfo_t *info = NULL;

    sp->pos = 0;
    for (c=0; c<sp->count; c++)
    {
	info = (phDieInfo_t *)getElementRef(sp->dieMap, sp->xStepList[c], sp->yStepList[c]);
	for (z=0; info && z<sp->sub; z++)
	    info[z] = (phDieInfo_t)(info[z] & ~PHFRAME_DIEINF_PROBED);
    }
    sp->mark = PHFRAME_STMARK_OUT;
}


int phFrameStepDoStep(
    phStepPattern_t *sp, 
    struct phFrameStruct *f             /* the framework data */
)
{
    int retVal = -1;
    int z;
    int cx = 0;
    int cy = 0; /* candidate position */
    int s;
    int pin; /* primary die included in probe offset list */
    phDieInfo_t *info = NULL;

    pin = 0;
    for (s=0; s<f->numSites && !pin; s++)
    {
	if (f->perSiteDieOffset[s].x == 0 &&
	    f->perSiteDieOffset[s].y == 0 &&
	    f->siteMask[s])
	    pin = 1;
    }
    
    do
    {
	sp->pos++;
	if (sp->pos < sp->count)
	{
	    for (z=0; z<sp->sub; z++)
	    {
		if (sp == &(f->d_sp))
		{
		    if (pin)
		    {
			cx = sp->xStepList[sp->pos];
			cy = sp->yStepList[sp->pos];
			info = (phDieInfo_t *)getElementRef(sp->dieMap, cx, cy);

			if ((info[z] & PHFRAME_DIEINF_INUSE) && !(info[z] & PHFRAME_DIEINF_PROBED))
			    retVal = 1;
		    }
		    else
		    {
			for (s=0; s<f->numSites; s++)
			{
			    if (!f->siteMask[s])
				continue;
			    cx = sp->xStepList[sp->pos] + f->perSiteDieOffset[s].x;
			    cy = sp->yStepList[sp->pos] + f->perSiteDieOffset[s].y;
			    if (cx < sp->minX || cx > sp->maxX || cy < sp->minY || cy > sp->maxY)
				continue;
			    info = (phDieInfo_t *)getElementRef(sp->dieMap, cx, cy);

			    if ((info[z] & PHFRAME_DIEINF_INUSE) && !(info[z] & PHFRAME_DIEINF_PROBED))
				retVal = 1;
			}
		    }
		    
		}
		else
		{
		    cx = sp->xStepList[sp->pos];
		    cy = sp->yStepList[sp->pos];
		    info = (phDieInfo_t *)getElementRef(sp->dieMap, cx, cy);
		    
		    if ((info[z] & PHFRAME_DIEINF_INUSE) && !(info[z] & PHFRAME_DIEINF_PROBED))
			retVal = 1;
		}
	    }
	}
	else 
	{
	    sp->pos=0;
	    retVal = 0;
	}
    } while (retVal == -1);

    return retVal;
}

int phFrameStepSnoopStep(
    phStepPattern_t *sp, 
    struct phFrameStruct *f             /* the framework data */
)
{
    int savedPos;
    int returnVal;

    savedPos = sp->pos;
    returnVal = phFrameStepDoStep(sp, f);
    sp->pos = savedPos;

    return returnVal;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
