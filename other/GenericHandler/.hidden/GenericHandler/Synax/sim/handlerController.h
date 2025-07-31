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
#include "gpibComm.h"
#include "handler.h"

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



class ViewController;
struct HandlerControllerSetup;
struct ViewControllerSetup;
class EventPlayback;
class EventRecorder;

class HandlerController : public LogObject {
public:
  /* 
   * Handler controller setup with simplest GUI
   */
  HandlerController(
                   HandlerControllerSetup *setup             /* configurable setup values */,
                   ViewControllerSetup *vcSetup              /* simple view controller */);
  virtual ~HandlerController();
  virtual GpibCommunication::commError tryReceiveMsg();
  virtual GpibCommunication::commError tryReceiveMsg(int to);
  virtual GpibCommunication::commError trySendSRQ(unsigned char);
  virtual GpibCommunication::commError trySendMsg(const char*);
  virtual void switchOn();
  virtual void quit();
  virtual void dump();
  int getReceiveMessageCount() const;
  Handler          *pHandler;
protected:
  /* 
   * Constructor for use of derived classes of HandlerController
   * which must setup their own GUI
   */
  HandlerController(
                   HandlerControllerSetup *setup            /* configurable setup values */);

  virtual void handleMessage(char *);
  virtual void sendMessage(const char*);
  virtual void replyToQuery(const char*);
  virtual void sendSRQ(unsigned char);

  ViewController            *pViewController;
  GpibCommunication         *pGpibComm;
  EventRecorder             *pEventRecorder;
  EventPlayback             *pEventPlayback;
  int                       length;
  int                       timeout;
  const char                *defaultEoc;
  bool                      done;
private:
  void sendMessageEoc(const char *msg, const char *eoc);
  GpibCommunication::commError trySendMsg(const char *msg, const char *eoc);

  bool                      queryError;
  int                       receiveMessageCount; 
  int                       replyQueryCount;
};


struct HandlerControllerSetup {
  int                           debugLevel;
  int                           timeout;
  const char                    *eoc;
  bool                          queryError;
  GpibCommSetup                 gpibComm;
  HandlerSetup     handler;
};





#endif /* ! _HANDLER_CONT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
