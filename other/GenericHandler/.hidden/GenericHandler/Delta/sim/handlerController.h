/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : handlerController.h
 * CREATED  : 22 Jan 2001
 *
 * CONTENTS : Interface header for handler controller implementation class
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

#ifndef _HANDLER_CONT_H_
#define _HANDLER_CONT_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
#include "handler.h"
#include "gpibComm.h"
#include "status.h"

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

const int MAX_MSG_LENGTH = 256;

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
 * Authors: Chris Joyce
 *
 * Description:
 * Interface for handler controller object
 *
 *****************************************************************************/

struct HandlerControllerSetup;

class ViewController;
class HandlerEventRecorder;
class HandlerEventPlayback;
class HandlerStatus;

enum deltaModel { Castle, Matrix, Flex, Rfs, Summit, Orion, Eclipse };

class HandlerController : public LogObject, public StatusObserver
{
public:
    HandlerController(
        HandlerControllerSetup *setup            /* configurable setup values */);
    virtual ~HandlerController();
    virtual GpibCommunication::commError tryReceiveMsg();
    virtual GpibCommunication::commError tryReceiveMsg(int to);
    virtual GpibCommunication::commError trySendSRQ(unsigned char);
    virtual GpibCommunication::commError trySendMsg(const char*);
    virtual void switchOn(deltaModel model);
    virtual void quit();
    virtual void dump();
    virtual const char *getName() const;
    int getReceiveMessageCount() const;
    Handler                   *pHandler;
protected:
    void createView();
    void checkData();
    virtual void handleMessage(char *);
    void implHandleMessage(char *message);
    virtual void sendMessage(const char*);
    virtual void replyToQuery(const char*);
    virtual void sendSRQ(unsigned char);

    // Handler                   *pHandler;
    ViewController            *pViewController;
    GpibCommunication         *pGpibComm;
    HandlerStatus             *pHstatus;
    HandlerEventRecorder      *pHEventRecorder;
    HandlerEventPlayback      *pHEventPlayback;
    int                       srqMask;
    int                       length;
    int                       timeout;
    const char                *defaultEoc;
    bool                      verifyTest;
    bool                      multipleSrqs;
    bool                      operatorPause;
    bool                      done;
    bool                      corruptMessage;
    int                       replyQueryCount;
private:
    void sendMessageEoc(const char *msg, const char *eoc);
    GpibCommunication::commError trySendMsg(const char *msg, const char *eoc);

    bool                      queryError;
    int                       receiveMessageCount; 
    bool                      noGuiDisplay;
    const char                *recordFile;
    const char                *playbackFile;
};


struct HandlerControllerSetup
{
    HandlerControllerSetup();
    HandlerSetup                  handler;
    GpibCommSetup                 gpibComm;
    const char                    *eoc;
    int                           debugLevel;
    int                           timeout;
    bool                          queryError;
    bool                          verifyTest;
    bool                          multipleSrqs;
    const char                    *recordFile;
    const char                    *playbackFile;
    bool                          noGui;
    bool                          corruptMessage;
};



#endif /* ! _HANDLER_CONT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
