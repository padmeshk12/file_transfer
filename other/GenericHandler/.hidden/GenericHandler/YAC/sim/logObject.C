/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : logObject.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Log and Timer definitions
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce , created
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

/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/

/*--- class implementations -------------------------------------------------*/



/*--- LogObject -------------------------------------------------------------*/
LogObject::LogObject(phLogId_t l) : logId(l)
{
}

LogObject::LogObject(int debug)
  :logId(NULL)
{
    phLogInit(&logId, getLogMode(debug) , "-", NULL, "-", NULL, -1);
}


/*--- LogObject private member functions ------------------------------------*/

phLogMode_t LogObject::getLogMode(int debugLevel)
{
    phLogMode_t modeFlags = PHLOG_MODE_NORMAL;

    /* open message logger */
    switch (debugLevel)
    {
      case -1:
        modeFlags = PHLOG_MODE_QUIET;
        break;
      case 0:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_0 );
        break;
      case 1:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_1 );
        break;
      case 2:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_2 );
        break;
      case 3:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_3 );
        break;
      case 4:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_4 );
        break;
      case 5:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_TRACE | PHLOG_MODE_DBG_4 );
        break;
    }

    return modeFlags;
}





/*--- Exception -------------------------------------------------------------*/


/*****************************************************************************
 * End of file
 *****************************************************************************/
