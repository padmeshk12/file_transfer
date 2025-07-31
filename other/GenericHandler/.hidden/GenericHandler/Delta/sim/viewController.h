/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : viewController.h
 * CREATED  : 22 Jan 2001
 *
 * CONTENTS : Interface header for view controller implementation class
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

#ifndef _VIEW_CONTROLLER_H_
#define _VIEW_CONTROLLER_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "logObject.h"
#include "ph_GuiServer.h"


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
 * GUI class exceptions
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Exception caused by unexpected ph_GuiServer.h error.
 *
 *****************************************************************************/

class GuiErrorException : public Exception
{
public:
    GuiErrorException(phGuiError_t e);
    virtual void printMessage();
protected:

    phGuiError_t phGuiError;
};


/*****************************************************************************
 *
 * ViewController exceptions
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Exception caused by some unexpected error during ViewController 
 * function call.
 *
 *****************************************************************************/

class ViewControllerException : public Exception
{
public:
    enum eVCException { formatError };
    ViewControllerException(eVCException);
    virtual void printMessage();
protected:
    eVCException VCexp;
};


/*****************************************************************************
 *
 * View Controller
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Interface for handler controller object
 *
 *****************************************************************************/

class HandlerController;
class HandlerStatus;

class ViewController : public LogObject, public StatusObserver 
{
public:
    enum eGpibStatus { okay, waitingMsg, tryingSendMsg, tryingSendSrq, timedOut, notUnderstood, error  };
    ViewController(phLogId_t l,
                   HandlerController *pHandCont,
                   HandlerStatus *pHS,
                   bool noGui);
    virtual ~ViewController();
    virtual void showView();
    virtual void statusChanged(const HandlerStatusBit&);
    virtual void setGpibStatus(eGpibStatus);
    virtual void setReceiveMsgField(const char *msg);
    virtual void setSendMsgField(const char *msg);
    virtual void setSendSrqField(unsigned char x);
    virtual void setSendSrqField(const char *msg);
    virtual void checkEvent(int timeout);
    virtual void checkData(int timeout);
protected:
    void createView(bool noGui);
    void handleEvent(const phGuiEvent_t &guiEvent);
    void checkGuiError(phGuiError_t phGuiError);
    void changeGuiValue(const char *componentName, const char *newValue);
    void beAlive();
    void getGuiDataString(
        const char *componentName     /* name of the component */, 
        char *msgBuffer               /* string to set */,
        int timeout                   /* timeout secs */);
    int getGuiDataInt(
        const char *componentName     /* name of the component */,
        const char *format            /* format of returned string */,
        int timeout                   /* timeout secs */);
    bool getGuiDataBool(
        const char *componentName     /* name of the component */,
        int timeout                   /* timeout secs */);
    void setGuiDataBool(
        const char *componentName     /* name of the component */,
        bool value                    /* value to set */ );
    int convertDebugToOptionMenuPosition(int debug);

    HandlerController *pHandlerController;
    HandlerStatus *pHStatus;
    phGuiProcessId_t guiId;
private:
    bool alivePulse;
};



#endif /* ! _VIEW_CONTROLLER_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
