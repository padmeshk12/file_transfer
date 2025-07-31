/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SXstatus.h
 * CREATED  : 18 Jan 2001
 *
 * CONTENTS : Synax status
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce, created
 *            11 Mar 2005, Ken Ward, copied from Seiko-Epson
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

#ifndef _SX_STATUS_H_
#define _SX_STATUS_H_

/*--- system includes -------------------------------------------------------*/
#include <map>
#include <list>

/*--- module includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
#include "simHelper.h"
#include "status.h"

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
 * Status type Conversion Exception 
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Base class from which all simulator exceptions may be derived.
 *
 *****************************************************************************/

class StatusStringToEnumConversionException : public Exception {
public:
  StatusStringToEnumConversionException(const char *errStr);
  virtual void printMessage();
private:
  Std::string convertStringError;
};



/*****************************************************************************
 *
 * Synax status
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/

class EventRecorder;
class SynaxStatusObserver;


class SynaxStatus : public LogObject {
public:
  enum eSXStatus {
    stopped, alarm, errorTx, empty
  };

  // public member functions
  SynaxStatus(phLogId_t l);
  bool setStatusBit(SynaxStatusObserver*, eSXStatus, bool b);
  bool getStatusBit(eSXStatus);
  int getStatusAsInt();
  bool getHandlerIsReady();
  void logStatus();
  void registerObserver(SynaxStatusObserver*);
  void registerRecorder(EventRecorder*, SynaxStatusObserver*);
  eSXStatus getEnumStatus(const char* statusName);
private:
  Std::list<SynaxStatusObserver*> observerList;
  Std::map<eSXStatus,StatusBit> StatusBitArray;
  EventRecorder                *pEventRecorder;
  SynaxStatusObserver     *pRecordObserver;
};


enum eYStatus {
  y_stopped, y_alarm, y_errorTx, y_empty
};

// class YokogawaStatus : public Status<eYStatus>
// {
// public:
//     YokogawaStatus(phLogId_t l);
// };
// 
// template<class T> class Yokogawa : public Status<T>
// {
// public:
//     YokogawaStatus(phLogId_t l);
// };
// 
// 
// 
// template<class T> Yokogawa::Yokogawa<T>(phLogId_t l) : Status<T>(l)
// {
//     //
//     // NOTE: Status bits defined must follow enumeration
//     // enum eYStatus defined in  SEstatus.h
//     //
// 
// //    StatusBitArray[y_stopped]    =  StatusBit("stopped", false, 1);
// //    StatusBitArray[y_alarm]      =  StatusBit("alarm"  , false, 2);
// //    StatusBitArray[y_errorTx]    =  StatusBit("errorTx", false, 3);
// //    StatusBitArray[y_empty]      =  StatusBit("empty"  , false, 0);
// }



/*****************************************************************************
 *
 * Synax status observer
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/


class SynaxStatusObserver {
public:
  virtual void SXStatusChanged(SynaxStatus::eSXStatus, bool b)=0;
};


#endif /* ! _SX_STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
