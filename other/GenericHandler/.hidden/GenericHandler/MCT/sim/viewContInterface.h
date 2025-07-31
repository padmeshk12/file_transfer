/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : viewContInterface.h
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : Interface header for view controller interface class
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 5 Jan 2015, Magco Li, created
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

#ifndef _VIEW_CONTROLLER_INT_H_
#define _VIEW_CONTROLLER_INT_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/



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
 * View Controller Interface class
 *
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 * Interface for handler controller object
 *
 *****************************************************************************/

struct ViewControllerSetup;

class ViewControllerInterface
{
public:
    virtual ~ViewControllerInterface() {};
    enum eGpibStatus { okay, waitingMsg, tryingSendMsg, tryingSendSrq, timedOut, notUnderstood, error  };
    virtual void setGpibStatus(eGpibStatus)=0;
    virtual void setReceiveMsgField(const char*)=0;
    virtual void setSendMsgField(const char*)=0;
    virtual void setSendSrqField(unsigned char)=0;
    virtual void setSendSrqField(const char*)=0;
    virtual void checkEvent(int timeout)=0;
    virtual void showView()=0;
};

class IntValueInterface
{
public:
    virtual void setValue(int)=0;
    virtual int getValue()=0;
    virtual bool hasChanged()=0;
};



#endif /* ! _VIEW_CONTROLLER_INT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
