/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_frame_private.h
 * CREATED  : 14 Jul 1999
 *
 * CONTENTS : Framework private header file
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
 *            13 Dec 1999, Michael Vogt, expanded for prober drivers
 *            Dec 2005, Jiawei Lin, CR28409
 *              allow the user to enable/disable diagnose window by specifying
 *              the driver parameter "enable_diagnose_window"
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

#ifndef _PH_FRAME_PRIVATE_H_
#define _PH_FRAME_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

#include <time.h>

/*--- module includes -------------------------------------------------------*/

#include "binner.h"
#include "sparsematrice.h"
#include "user_dialog.h"

/*--- defines ---------------------------------------------------------------*/

#define BIG_ERROR_STRING \
    "\n" \
    " ######  #####   #####    ####   #####  \n" \
    " #       #    #  #    #  #    #  #    # \n" \
    " #####   #    #  #    #  #    #  #    # \n" \
    " #       #####   #####   #    #  #####  \n" \
    " #       #   #   #   #   #    #  #   #  \n" \
    " ######  #    #  #    #   ####   #    # \n" \
    "\n"

#define BIG_FATAL_STRING \
    "\n" \
    " ######    ##     #####    ##    #      \n" \
    " #        #  #      #     #  #   #      \n" \
    " #####   #    #     #    #    #  #      \n" \
    " #       ######     #    ######  #      \n" \
    " #       #    #     #    #    #  #      \n" \
    " #       #    #     #    #    #  ###### \n" \
    "\n"

#define LOG_NORMAL   PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG    PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE  PHLOG_TYPE_MESSAGE_2

#define TEST_LOG_ENV_VAR "_PHDRIVER_TEST_LOG_OUT"

/*--- typedefs --------------------------------------------------------------*/

typedef enum {
/*    PHFRAME_BINACT_NULL, */
    PHFRAME_BINACT_SKIPBIN,
    PHFRAME_BINACT_DOBIN
} phBinAction_t;

typedef enum {
    PHFRAME_STEPMD_EXPLICIT,
    PHFRAME_STEPMD_AUTO,
    PHFRAME_STEPMD_LEARN
} phStepMode_t;

typedef struct dieOffset
{
    long x;
    long y;
} phOffset_t;

typedef enum {
    /* this is a bit field... */
    PHFRAME_DIEINF_INUSE     = 0x0001,
    PHFRAME_DIEINF_PROBED    = 0x0002
} phDieInfo_t;

typedef enum {
    PHFRAME_STMARK_OUT = 0,
    PHFRAME_STMARK_WS,
    PHFRAME_STMARK_WSP,
    PHFRAME_STMARK_ACT,
    PHFRAME_STMARK_WD,
    PHFRAME_STMARK_WDP,
    PHFRAME_STMARK_SNOOP,
    PHFRAME_STMARK_STEP,
    PHFRAME_STMARK_END
} phStepMark_t;

typedef struct stepPattern
{
    phStepMode_t stepMode;
    int count;
    int pos;
    long minX;
    long maxX;
    long minY;
    long maxY;
    int sub;
    long *xStepList;
    long *yStepList;
    /*  phDieInfo_t ***dieInfo; */
    phMatrice_t dieMap;

    phStepMark_t mark;
    long currentX;                 /* valid, if mark == PHFRAME_STMARK_ACT */
    long currentY;                 /* valid, if mark == PHFRAME_STMARK_ACT */

    struct stepPattern *next;
} phStepPattern_t;

struct phFrameStruct
{
    int                            isInitialized;
    int                            initTried;
    int                            errors;
    int                            warnings;

    phFrameAction_t                currentAMCall;

    phLogId_t                      logId;
    phConfId_t                     globalConfId;
    phConfId_t                     specificConfId;
    phAttrId_t                     attrId;
    phTcomId_t                     testerId;
    phStateId_t                    stateId;
    phEstateId_t                   estateId;
    phComId_t                      comId;
    phPFuncId_t                    funcId;
    phEventId_t                    eventId;

    phPFuncAvailability_t          funcAvail;
    phHookAvailability_t           hookAvail;

    int                            numSites;
    long                           stToProberSiteMap[PHESTATE_MAX_SITES];
    int                            siteMask[PHESTATE_MAX_SITES];

    phBinMode_t                    binMode;
    int                            binPartialMode;
    phBinMapId_t                   binMapHdl;

    phBinActType_t                 binActUsed;
    phBinActMode_t                 binActMode;
    phBinActMapId_t                binActMapHdl;
    phBinActType_t                 binActCurrent;

    long                           dieBins[PHESTATE_MAX_SITES];
    phBinAction_t                  binAction;
    long                           diePassed[PHESTATE_MAX_SITES];
    int                            dontTrustBinning;

    int                            enableDiagnoseWindow;  /* CR28409 */

    int                            cleaningRatePerDie;   /* from config */
    int                            cleaningRatePerWafer;
    int                            cleaningDieCount;     /* actual counters */
    int                            cleaningWaferCount;

    int                            isSubdieProbing;
    phOffset_t                     perSiteDieOffset[PHESTATE_MAX_SITES];

    int                            cassetteLevelUsed;
    int                            lotLevelUsed;

/*
    int                            stDieLocIsByIndex;
    int                            probDieLocIsByIndex;
    phOffset_t                     dieSize;
    phOffset_t                     subdieSize;
    phOffset_t                     dieLocOffset;
    phOffset_t                     subdieLocOffset;
*/

    phStepPattern_t                d_sp;
    phStepPattern_t                sd_sp;
    
    struct timerElement            *totalTimer;
    struct timerElement            *shortTimer;

    long                           initialPauseFlag;
    long                           initialQuitFlag;

    /* user dialog handles */
    phUserDialogId_t               dialog_comm_start;
    phUserDialogId_t               dialog_config_start;
    phUserDialogId_t               dialog_lot_start;
    phUserDialogId_t               dialog_wafer_start;

    int                            createTestLog;
    FILE                           *testLogOut;
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

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: <your name(s)>
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * <Blank lines, like the one just ahead, may be used to separate paragraphs>
 *
 * Input parameter format: 
 * <Describe the format of the input parameter string here>
 *
 * Output result format:
 * <Describe the format of the output parameter string here>
 * 
 * Warnings: <delete this, if not used>
 *
 * Notes: <delete this, if not used>
 *
 * Returns: <the function's return value, delete this if void>
 *
 ***************************************************************************/

#endif /* ! _PH_FRAME_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
