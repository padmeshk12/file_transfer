/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_tcom.h
 * CREATED  : 27 May 1999
 *
 * CONTENTS : Communication interface to CPI and TPI
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 27 May 1999, Michael Vogt, created
 *            08 Jul 1999, Michael Vogt
 *                introduced the handle concept to this module
 *            08 Dec 1999, Chris Joyce
 *                added get and set wafer and die positions
 *                and their corresponding simulation calls.
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

#ifndef _PH_TCOM_H_
#define _PH_TCOM_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

typedef struct phTcomStruct *phTcomId_t;

typedef enum {
    PHTCOM_ERR_OK = 0                   /* no error */,
    PHTCOM_ERR_NOT_INIT                 /* the state controller was not
					   initialized successfully */,
    PHTCOM_ERR_INVALID_HDL              /* the passed ID is not valid */,
    PHTCOM_ERR_ERROR                    /* an error was reported by CPI or 
                                           TPI interface */,
    PHTCOM_ERR_MEMORY                    /* couldn't get memory from heap 
                                           with malloc() */
} phTcomError_t;

typedef enum {
    PHTCOM_MODE_ONLINE                  /* really talk to a CPI and
					   TPI interface */,
    PHTCOM_MODE_SIMULATED               /* talk to a simulated tester
					   (not yet implemented) */
} phTcomMode_t;


/* Enumeration type for driver accessible system flags. Do not change
   any of the given entries. New entries may be added at the bottom of
   the list only! */
typedef enum {
    PHTCOM_SF_CI_ABORT       = 0,
    PHTCOM_SF_CI_BAD_PART,
    PHTCOM_SF_CI_BIN_CODE,
    PHTCOM_SF_CI_BIN_NUMBER,
    PHTCOM_SF_CI_CHECK_DEV,
    PHTCOM_SF_CI_GOOD_PART,
    PHTCOM_SF_CI_LEVEL_END,
    PHTCOM_SF_CI_PAUSE,
    PHTCOM_SF_CI_QUIT,
    PHTCOM_SF_CI_REFERENCE,
    PHTCOM_SF_CI_RESET,
    PHTCOM_SF_CI_RETEST,
    PHTCOM_SF_CI_RETEST_W,
    PHTCOM_SF_CI_SITE_REPROBE,
    PHTCOM_SF_CI_SITE_SETUP,
    PHTCOM_SF_CI_SKIP,
    PHTCOM_SF_LOT_AVAILABLE,
    PHTCOM_SF_DEVICE_AVAILABLE,

    /* never change the next line */
    PHTCOM_SF__END
} phTcomSystemFlag_t;

/* Enumeration type for driver accessible testflow flags. Do not change
   any of the given entries. New entries may be added at the bottom of
   the list. */
typedef enum {
    PHTCOM_TFF_CI_DEBUG_MODE = 1
} phTcomTestflowFlag_t;

/* Enumeration type for user procedures return values (mainly used by
   hook functions, but defined here, since it belongs to the tester
   communication). */
typedef enum {
    PHTCOM_CI_CALL_PASS,
    PHTCOM_CI_CALL_ERROR,
    PHTCOM_CI_CALL_BREAKD,
    // below are new return codes from privateGetStart and privateLotStart
    PHTCOM_CI_CALL_JAM,
    PHTCOM_CI_CALL_LOT_START,
    PHTCOM_CI_CALL_LOT_DONE,
    PHTCOM_CI_CALL_DEVICE_START,
    PHTCOM_CI_CALL_TIMEOUT
} phTcomUserReturn_t;

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

/*****************************************************************************
 *
 * Initialize a communication back to the CPI and TPI interface
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function creates a new link to SmarTest's CPI and TPI
 * interfaces. The tester communication resulting out of this function
 * call should be the only link to call interface functions in CPI or
 * TPI. Never call CPI or TPI functions directly. 
 * The reasons for this are:
 *
 * Only calls through this interface are logged, if CPI or TPI
 * changes, this would be transparent to user of this module, the
 * driver has a chance of control about what is communicated to CPI or
 * TPI through hook functions (this control is not yet implemented but
 * this layer makes it possible to happen).
 *
 * For convenience, the function interfaces of this module are kept
 * similar to those of the CPI or TPI. This should simplify the
 * transfer of code parts that use the CPI of TPI directly so
 * far. Missing functionality may be added.
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
);

/*****************************************************************************
 *
 * Destroy a tester communication interface
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function destroys the tester communication interface
 * handle. Don't worry, the tester won't explode :-)
 *
 * Returns: error code
 *
 *****************************************************************************/
phTcomError_t phTcomDestroy(
    phTcomId_t testerID          /* the tester ID */
);


/*****************************************************************************
 * Testlog mode: clear all get system flags
 *****************************************************************************/
phTcomError_t phTcomLogStartGetList(phTcomId_t testerID);

/*****************************************************************************
 * Testlog mode: clear all set system flags
 *****************************************************************************/
phTcomError_t phTcomLogStartSetList(phTcomId_t testerID);

/*****************************************************************************
 * Testlog mode: return all get system flags
 *****************************************************************************/
char *phTcomLogStopGetList(phTcomId_t testerID);

/*****************************************************************************
 * Testlog mode: return all set system flags
 *****************************************************************************/
char *phTcomLogStopSetList(phTcomId_t testerID);

/*****************************************************************************
 * Simulation mode: clear all system flags
 *****************************************************************************/

phTcomError_t phTcomSimClearSystemFlags(void);

/*****************************************************************************
 * Simulation mode: set a system flag
 *****************************************************************************/

phTcomError_t phTcomSimSetSystemFlag(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long value                   /* value */
);

/*****************************************************************************
 * Simulation mode: set a system flag of site
 *****************************************************************************/

phTcomError_t phTcomSimSetSystemFlagOfSite(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long site                    /* which site to refer to */,
    long value                   /* value */
);

/*****************************************************************************
 * Simulation mode: set die position xy 
 *****************************************************************************/

phTcomError_t phTcomSimSetDiePosXY(
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
);

/*****************************************************************************
 * Simulation mode: set die position xy of site 
 *****************************************************************************/

phTcomError_t phTcomSimSetDiePosXYOfSite(
    long site                    /* which site to refer to */,
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
);

/*****************************************************************************
 * Simulation mode: set wafer description file 
 *****************************************************************************/

phTcomError_t phTcomSimSetWaferDescriptionFile(
    char *path                   /* wafer description file name */
);

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
);

/*****************************************************************************
 * Simulation mode: get a system flag
 *****************************************************************************/

phTcomError_t phTcomSimGetSystemFlag(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long *value                  /* value */
);

/*****************************************************************************
 * Simulation mode: get a system flag of site
 *****************************************************************************/

phTcomError_t phTcomSimGetSystemFlagOfSite(
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long site                    /* which site to refer to */,
    long *value                  /* value */
);

/*****************************************************************************
 * Simulation mode: define user button presses
 *****************************************************************************/

phTcomError_t phTcomSimDefineButtons(
    int count        /* the number of buttons defined in the button array */,
    long *buttonArray /* the button array, values will be used one
			after the other until <count> is hit. After
			that, simulation always returns 'Continue' */
);

/*****************************************************************************
 * Simulation mode: get die position xy 
 *****************************************************************************/

phTcomError_t phTcomSimGetDiePosXY(
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
);

/*****************************************************************************
 * Simulation mode: get die position xy of site 
 *****************************************************************************/

phTcomError_t phTcomSimGetDiePosXYOfSite(
    long site                    /* which site to refer to */,
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
);

/*****************************************************************************
 * Simulation mode: get wafer description file 
 *****************************************************************************/

phTcomError_t phTcomSimGetWaferDescriptionFile(
    char *path                   /* wafer description file name */
);

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
);


/*****************************************************************************
 * Call GetModelfileString() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetModelfileString(
    phTcomId_t testerID         /* the tester ID */,
    char *name                  /* model file variable */, 
    char *value                 /* returned value */
);

/*****************************************************************************
 * Call GetSystemFlag() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetSystemFlag(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long *value                  /* returned value */
);

/*****************************************************************************
 * Call GetSystemFlagOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetSystemFlagOfSite(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */,
    long site                    /* which site to refer to */,
    long *value                  /* returned value */
);

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
);

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
);

/*****************************************************************************
 * Call GetDiePosXY() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetDiePosXY(
    phTcomId_t testerID          /* the tester ID */,
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
);

/*****************************************************************************
 * Call GetDiePosXYOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetDiePosXYOfSite(
    phTcomId_t testerID          /* the tester ID */,
    long site                    /* which site to refer to */,
    long *x                      /* x die coordinate */,
    long *y                      /* y die coordinate */
);

/*****************************************************************************
 * Call GetWaferDescriptionFile() from CPI returns PHTCOM_ERR_ERROR if wafer 
 * description file is not available.
 *****************************************************************************/

phTcomError_t phTcomGetWaferDescriptionFile(
    char *path                   /* wafer description file name */
);

phTcomError_t phTcomSetWaferDescriptionFile(
    char *path                   /* wafer description file name */
);

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
);


/*****************************************************************************
 * Call SetModelfileString() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetModelfileString(
    phTcomId_t testerID          /* the tester ID */,
    char *name                   /* model file variable */,
    char *value                  /* value to set */
);

/*****************************************************************************
 * Call SetSystemFlag() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetSystemFlag(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long value                   /* value to set */
);

/*****************************************************************************
 * Call SetSystemFlagOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetSystemFlagOfSite(
    phTcomId_t testerID          /* the tester ID */,
    phTcomSystemFlag_t flag      /* phtcom system flag */, 
    long site                    /* which site to refer to */,
    long value                   /* value to set */
);

/*****************************************************************************
 * Call SetTestflowFlag() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetTestflowFlag(
    phTcomId_t testerID          /* the tester ID */,
    phTcomTestflowFlag_t flag    /* phtcom testflow flag */,
    long value                   /* value to set */
);

/*****************************************************************************
 * Call SetUserDouble() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetUserDouble(
    phTcomId_t testerID          /* the tester ID */,
    char *name                   /* user defined variable */,
    double value                 /* value to set */
);

/*****************************************************************************
 * Call SetUserDoubleOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetUserDoubleOfSite(
    phTcomId_t testerID          /* the tester ID */,
    char *name                   /* user defined variable */,
    long site                    /* which site to refer to */,
    double value                 /* value to set */
);

/*****************************************************************************
 * Call SetUserFlag() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetUserFlag(
    phTcomId_t testerID          /* the tester ID */,
    char *flag                   /* user defined flag */,
    long value                   /* value to set */
);

/*****************************************************************************
 * Call SetUserFlagOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetUserFlagOfSite(
    phTcomId_t testerID          /* the tester ID */,
    char *flag                   /* user defined flag */,
    long site                    /* which site to refer to */,
    long value                   /* value to set */
);

/*****************************************************************************
 * Call SetUserString() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetUserString(
    phTcomId_t testerID          /* the tester ID */,
    char *name                   /* user defined variable */,
    char *value                  /* value to set */
);

/*****************************************************************************
 * Call SetUserStringOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetUserStringOfSite(
    phTcomId_t testerID          /* the tester ID */,
    char *name                   /* user defined variable */,
    long site                    /* which site to refer to */,
    char *value                  /* value to set */
);

/*****************************************************************************
 * Call SetDiePosXY() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetDiePosXY(
    phTcomId_t testerID          /* the tester ID */,
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
);

/*****************************************************************************
 * Call SetDiePosXYOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomSetDiePosXYOfSite(
    phTcomId_t testerID          /* the tester ID */,
    long site                    /* which site to refer to */,
    long x                       /* x die coordinate */,
    long y                       /* y die coordinate */
);


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
);


/*****************************************************************************
 * Simulation mode: clear all softbins code and hardbins number
 *****************************************************************************/

phTcomError_t phTcomSimClearBins(void);


/*****************************************************************************
 * Simulation mode: set a softbin code and hardbin number
 *****************************************************************************/

phTcomError_t phTcomSimSetBin(
    const char* softbin          /*  soft-bin code */,
    long hardbin                 /*  hard-bin number */
);


/*****************************************************************************
 * Simulation mode: set a softbin code and hardbin number
 *****************************************************************************/

phTcomError_t phTcomSimSetBinOfSite(
    long site                    /*  which site to refer to */,
    const char* softbin          /*  soft-bin code */,
    long hardbin                 /*  hard-bin number */
);


/*****************************************************************************
 * Simulation mode: get softbin code and hardbin number
 *****************************************************************************/

phTcomError_t phTcomSimGetBin(
    char* softbin                /*  soft-bin code */,
    long* hardbin                /*  hard-bin number */
);


/*****************************************************************************
 * Simulation mode: get softbin code and hardbin number of site
 *****************************************************************************/

phTcomError_t phTcomSimGetBinOfSite(
    long site                    /*  which site to refer to */,
    char* softbin                /*  soft-bin code */,
    long* hardbin                /*  hard-bin number */
);


/*****************************************************************************
 * This function only use for dirver ART test stub
 *****************************************************************************/

phTcomError_t phTcomSetBinOfSite(
    phTcomId_t testerID          /*  the tester ID */,
    long site                    /*  which site to refer to */,
    const char* softbin          /*  soft-bin code */,
    long hardbin                 /*  hard-bin number */
);


/*****************************************************************************
 * Call GetBinOfSite() from CPI
 *****************************************************************************/

phTcomError_t phTcomGetBinOfSite(
    phTcomId_t testerID          /*  the tester ID */,
    long site                    /*  which site to refer to */,
    char* softbin                /*  soft-bin code */,
    long* hardbin                /*  hard-bin number */
);

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_TCOM_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
