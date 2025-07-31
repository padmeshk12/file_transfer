/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : MCTHandlerController.h
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : Interface header for handler controller implementation class
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

#ifndef _TESEC_HANDLER_CONT_H_
#define _TESEC_HANDLER_CONT_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "handler.h"
#include "status.h"
#include <string>

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
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 * Interface for handler controller object
 *
 *****************************************************************************/


struct MCTHandlerControllerSetup;

template<class T> class HandlerViewController;
template<class T> class EventRecorder;
template<class T> class EventPlayback;

enum eMctStatus { stopped, empty };


class MCTHandlerController : public HandlerController, public StatusObserver<eMctStatus>
{
public:
    enum eMctModel { FH1200 };

    MCTHandlerController(
        MCTHandlerControllerSetup   *setup       /* configurable setup values */,
        ViewControllerSetup           *VCsetup     /* view controller */);
    virtual ~MCTHandlerController();
    virtual void switchOn();
    virtual void StatusChanged(eMctStatus, bool b);
    eMctModel getModelType() const;
    const char *getModelName() const;
    void serviceStartSignal();
protected:
    virtual void handleMessage(char *message);
    virtual unsigned char getSrqByte();
    void serviceSortBinSignal(const char *msg);

    Status<eMctStatus> *pStatus;
    HandlerViewController<eMctStatus> *pHViewController;
    EventRecorder<eMctStatus> *pEventRecorder;
    EventPlayback<eMctStatus> *pEventPlayback;

    eMctModel model;
private:
    char newPanel[MAX_MSG_LENGTH];
    int testedDevice;
};


struct MCTHandlerControllerSetup : public HandlerControllerSetup
{
    MCTHandlerController::eMctModel model;
    const char *recordFile;
    const char *playbackFile;
};




#endif /* ! _TESEC_HANDLER_CONT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
