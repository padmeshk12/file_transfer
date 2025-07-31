/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : YACHandlerController.h
 * CREATED  : 29 Dec 2006
 *
 * CONTENTS : Interface header for handler controller implementation class
 *
 * AUTHORS  : Kun Xiao, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Dec 2006, Kun Xiao, created
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

#ifndef _YAC_HANDLER_CONT_H_
#define _YAC_HANDLER_CONT_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "YACStatus.h"
#include "handler.h"

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/




/*--- typedefs --------------------------------------------------------------*/


/*--- class definition  -----------------------------------------------------*/

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
 * Handler Controller Implementation
 *
 * Authors: Kun Xiao
 *
 * Description:
 * Interface for handler controller object
 *
 *****************************************************************************/


struct YACHandlerControllerSetup;

class eventRecorder;
class YACViewController;


class YACHandlerController : public HandlerController, public YACStatusObserver
{
public:
    enum eYACModel { HYAC8086 };

    YACHandlerController(
        YACHandlerControllerSetup   *setup       /* configurable setup values */,
        ViewControllerSetup      *VCsetup       /* view controller */);
    virtual ~YACHandlerController();
    virtual void switchOn();
    virtual void YACStatusChanged(YACStatus::eYACStatus, bool b);
    eYACModel getModelType() const;
protected:
    virtual void handleMessage(char *message);
    virtual unsigned char getSrqByte();
    void serviceStatus(const char *msg);
    void serviceFullsites(const char *msg);
    void serviceBinon(const char *msg);
    void serviceEcho();
    void serviceEchoOk(const char *msg);
    void serviceEchoNg(const char *msg);
    void serviceSrqmask(const char *msg);

    YACStatus *pYACStatus;
    YACViewController *pYACViewController; 
    eYACModel model;
    bool verifyTest;
    bool multipleSrqs;
private:
    int srqMask;
};


struct YACHandlerControllerSetup : public HandlerControllerSetup
{
    YACHandlerController::eYACModel model;
    bool verifyTest;
    bool multipleSrqs;
    const char *recordFile;
    const char *playbackFile;
};




#endif /* ! _YAC_HANDLER_CONT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
