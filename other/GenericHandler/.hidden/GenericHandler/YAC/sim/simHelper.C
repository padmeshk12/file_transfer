/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simHelper.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Helper classes for prober handler simulators
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

#if __GNUC__ >= 3
#include <iostream>
#endif

#include <ctype.h>
#include <cstring>

/*--- module includes -------------------------------------------------------*/

#include "simHelper.h"
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



/*--- helper class implementations ------------------------------------------*/


/*--- BooleanFormatException ------------------------------------------------*/

BooleanFormatException::BooleanFormatException(const char *bStr) :
    boolString(bStr)
{
}

void BooleanFormatException::printMessage()
{
    Std::cerr << "Unable to convert \"" << boolString << "\" into a bool value";
}

/*--- FormatException -------------------------------------------------------*/

FormatException::FormatException(const char *fStr) :
    formatString(fStr)
{
}

void FormatException::printMessage()
{
    Std::cerr << formatString << "\n";
}

/*--- Timer -----------------------------------------------------------------*/

Timer::Timer(phLogId_t l) : LogObject(l)
{
    start();
}

void Timer::start()
{
    gettimeofday(&startTime, NULL);
}

double Timer::getElapsedMsecTime()
{
    struct timeval now;

    gettimeofday(&now, NULL);

    return (now.tv_sec - startTime.tv_sec)*1000.0 + (now.tv_usec - startTime.tv_usec)/1000.0;
}


/*--- StatusBit -------------------------------------------------------------*/

StatusBit::StatusBit() : name("none"), value(false), position(0)
{
}

StatusBit::StatusBit(const char* n, bool v, int p) : 
    name(n), value(v), position(p)
{
}

void StatusBit::setStatusBit(const char* n, bool b, int p)
{
    name=n;
    value=b;
    position=p;
}

void StatusBit::setBit(bool b)
{
    value=b;
}

void StatusBit::setPosition(int p)
{
    position=p;
}

void StatusBit::setName(const char* n)
{
    name=n;
}

int StatusBit::getBitAsInt() const
{
    return (position==No_position) ? 0 : value*(1<<position);
}

bool StatusBit::getBit() const
{
    return value;
}

const char *StatusBit::getName() const
{
    return name;
}


/*--- helper function implementations ---------------------------------------*/

/*****************************************************************************
 *
 * Find hex char
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Find first hexadecimal character in string or return null 
 *
 *****************************************************************************/
const char *findHexadecimalDigit(const char *s)
{
    while( *s && !isxdigit(*s) )
    {
        ++s;
    }

    return s;
}

/*****************************************************************************
 *
 * Convert hexadecimal character into int
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Find first hexadecimal character in string or return null 
 * use version of atoi which takes a char of a hexadecimal digit and
 * converts it into an integer
 *
 *****************************************************************************/
int convertHexadecimalDigitIntoInt(char s)
{
    char hexStr[2];

    hexStr[0]=s;
    hexStr[1]='\0';

    return int( strtol(hexStr, (char **)NULL, 16) );
}

/*****************************************************************************
 *
 * Convert string into bool
 *
 * Authors: Chris Joyce
 *
 * Description:
 * If string is:
 * "True"  | "true" |
 * "False" | "false"
 * boolean value is returned otherwise a BooleanFormatException is thrown.
 *
 *****************************************************************************/
bool convertStringIntoBool(const char *string)
{
    bool value;

    if (strcasecmp(string, "true") == 0)
    {
        value=true;
    }
    else if (strcasecmp(string, "false") == 0)
    {
        value=false;
    }
    else
    {
        throw BooleanFormatException(string);
    }

    return value;
}


/*****************************************************************************
 *
 * Remove EOC characters from the end of a string
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Remove '\n' and '\r' characters from the end of string
 *
 *****************************************************************************/
void removeEoc(char *string)
{
    int lastChar;
    bool eocChar=true;

    while ( eocChar )
    {
       lastChar=strlen(string)-1;

       if ( lastChar >= 0 )
       {
           eocChar=string[lastChar]=='\n' || string[lastChar]=='\r';
       }
       else
       {
           eocChar=false;
       }

       if ( eocChar )
       {
           string[lastChar]='\0';
       }

    }
}

/*****************************************************************************
 *
 * Set string to lower case
 *
 * Authors: Chris Joyce
 *
 * Description:
 * For each character in string set to lowercase until '\0' is found
 *
 *****************************************************************************/
void setStrlower(char *string)
{
    int i=0;

    while ( string[i] )
    {
        string[i]=tolower( string[i] );
        ++i;
    }
}


/*****************************************************************************
 *
 * Get Debug Level from log Mode
 *
 * Authors: Chris Joyce
 *
 * Description:
 *
 *****************************************************************************/
int getDebugFromLogMode(phLogId_t logId)
{
    int debug;
    phLogMode_t modeFlags;

    phLogGetMode(logId,&modeFlags);

    if ( modeFlags & PHLOG_MODE_DBG_0 )
    {
        debug=0;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_1 )
    {
        debug=1;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_2 )
    {
        debug=2;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_3 )
    {
        debug=3;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_4 )
    {
        debug=4;
    }
    else
    {
        debug=-1;
    }

    return debug;
}


/*****************************************************************************
 *
 * Set Log Mode from Debug Level
 *
 * Authors: Chris Joyce
 *
 * Description:
 *
 *****************************************************************************/
void setLogModeDebugLevel(
    phLogId_t  logId              /* log ID */,
    int        debugLevel         /* new debug level    */
)
{
    phLogMode_t modeFlags;
    unsigned long mask;


    /* get current log mode and remove all debug flags */
    phLogGetMode(logId, &modeFlags);

    mask = ~(PHLOG_MODE_DBG_0|PHLOG_MODE_DBG_1|PHLOG_MODE_DBG_2|
             PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4);

    modeFlags = phLogMode_t( (unsigned long)modeFlags & mask );


//    modeFlags &= ~PHLOG_MODE_DBG_0;
//    modeFlags &= ~PHLOG_MODE_DBG_1;
//    modeFlags &= ~PHLOG_MODE_DBG_2;
//    modeFlags &= ~PHLOG_MODE_DBG_3;
//    modeFlags &= ~PHLOG_MODE_DBG_4;

    switch (debugLevel)
    {
      case -1:
        /* quiet */
        mask = 0x0; 
        break;
      case 0:
        mask = PHLOG_MODE_DBG_0;
        break;
      case 1:
        mask = PHLOG_MODE_DBG_1;
        break;
      case 2:
        mask = PHLOG_MODE_DBG_2;
        break;
      case 3:
        mask = PHLOG_MODE_DBG_3;
        break;
      case 4:
        mask = PHLOG_MODE_DBG_4;
        break;
      default:
        /* incorrect value, do debug level 1 by default */
        mask = PHLOG_MODE_DBG_1;
        break;
    }

    modeFlags = phLogMode_t( (unsigned long)modeFlags | mask );


    /* replace log mode */
    phLogSetMode(logId, modeFlags);
}



/*****************************************************************************
 * End of file
 *****************************************************************************/
