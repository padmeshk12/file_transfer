/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simHelper.h
 * CREATED  : 19 May 1999
 *
 * CONTENTS : Interface header for helper classes for prober handler simulators
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce, created
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

#ifndef _SIMHELPER_H_
#define _SIMHELPER_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "logObject.h"

/*--- defines ---------------------------------------------------------------*/



/*--- typedefs --------------------------------------------------------------*/


/*--- class definition  -----------------------------------------------------*/

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
 * BooleanFormatException 
 *
 * Authors: Chris Joyce
 *
 * Description:
 * String string does not conform to expected boolean format.
 *
 *****************************************************************************/

class BooleanFormatException : public Exception
{
public:
    BooleanFormatException(const char *bStr);
    virtual void printMessage();
private:
    Std::string boolString;
};

/*****************************************************************************
 *
 * FormatException 
 *
 * Authors: Chris Joyce
 *
 * Description:
 * String string does not conform to expected boolean format.
 *
 *****************************************************************************/

class FormatException : public Exception
{
public:
    FormatException(const char *fStr);
    virtual void printMessage();
private:
    Std::string formatString;
};

/*****************************************************************************
 *
 * RangeException 
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Out of range error.
 *
 *****************************************************************************/

class RangeException : public Exception
{
public:
    RangeException(const char *rStr);
    virtual void printMessage();
private:
    Std::string rangeString;
};

/*****************************************************************************
 *
 * FileException
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Error occurred while trying to read file
 *
 *****************************************************************************/

class FileException : public Exception
{
public:
    FileException(int errNum);
    virtual void printMessage();
private:
    int errorNumber;
};












/*****************************************************************************
 *
 * Simple timer
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Timer timer is started from the start() call and elapsed time in msec
 * is returned from getElapsedMsecTime()
 *
 *****************************************************************************/

class Timer : public LogObject
{
public:
    Timer(phLogId_t l);
    void start();
    double getElapsedMsecTime();
private:
    struct timeval startTime;
};


/*****************************************************************************
 *
 * Status bit
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Status bit used by handler to indicate how handler is functioning and
 * any error conditions
 *
 *****************************************************************************/
class StatusBit
{
public:
    enum { No_position = -1 };
    StatusBit();
    StatusBit(bool v, int p, const char* n="");
    virtual ~StatusBit();
    void setStatusBit(bool v, int p, const char* n="");
    void setBit(bool v);
    int getBitAsInt() const;
    bool getBit() const;
    const char *getName() const;
    const char *getKey() const;
private:
    bool value;
    int position;
    const char* name;
    mutable char* key;
};

/*--- helper functions ------------------------------------------------------*/

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
const char *findHexadecimalDigit(const char *s);


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
int convertHexadecimalDigitIntoInt(char s);


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
bool convertStringIntoBool(const char *string);


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
void removeEoc(char *string);

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
void setStrlower(char *string);

/*****************************************************************************
 *
 * Replace all occurances of a particular character in a string with another
 * character.
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Go through each character in string and look for character fc and replace
 * it with character rc until null terminal is encountered.
 *
 *
 *****************************************************************************/
void replaceCharacterInString(char *string, char fc, char rc);

/*****************************************************************************
 *
 * Replace all white spaces in string with another character
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Go through each character in string and look for white spaces and replace
 * it with character rc until null terminal is encountered.
 *
 *
 *****************************************************************************/
void replaceWhiteSpaceInString(char *string, char rc);

/*****************************************************************************
 *
 * Get Debug Level from log Mode
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/
int getDebugFromLogMode(phLogId_t logId);


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
    int        debugLevel         /* new debug level    */);



/*****************************************************************************
 *
 * Get timestamp string
 *
 * Author: Michael Vogt
 *
 * Description:
 * Returns a pointer to a string in the format like
 *
 * 1998/01/15 13:45:12.345
 *
 * based on the current time and local time zone
 *
 *****************************************************************************/
const char *getTimeString();






#endif /* ! _SIMHELPER_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
