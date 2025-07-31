/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : viewController.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : View Controller Implementation
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
#include <cstring>

/*--- module includes -------------------------------------------------------*/
#include "simHelper.h"
#include "viewController.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

const char *viewControllerDesc =  " \
    S:`name`:`GPIB Handler` \
    {@v \
        S::``{@v \
            S::{@h  f:`f_req`:`Request`:    p:`p_req`:`Receive`[`Try to receive request`] } \
            S::{@h  f:`f_answ`:`Answer`:    p:`p_answ`:`Send`[`Try to send answer`] } \
            S::{@h  f:`f_srq`:`SRQ`:        p:`p_srq`:`Send`[`Try to send SRQ`] } \
            S::{@h  f:`f_status`:`Status`:  } \
            S::{@h  R::{`r_life_1`:`` `r_life_2`:``}:2:2 * } \
            S::{@h  p:`p_dump`:`Dump`[`Output all debug`] o:`o_debug_level`:  \
                     (`debug -1`        \
                      `debug 0`           \
                      `debug 1`           \
                      `debug 2`           \
                      `debug 3`           \
                      `debug 4`):%d[`Set debug level`]   }  \
            S::{@h  *p:`p_quit`:`Quit`[`Quit program`] } \
        } \
    } \
";




/*--- typedefs --------------------------------------------------------------*/


/*--- class implementations -------------------------------------------------*/


/*--- GuiErrorException -----------------------------------------------------*/

GuiErrorException::GuiErrorException(phGuiError_t e) : phGuiError(e)
{
}

void GuiErrorException::printMessage()
{
  Std::cerr << "GUI: ";

  switch(phGuiError) {
    case PH_GUI_ERR_OK:
      Std::cerr << "no error returned from ph_GuiServer layer";
      break;
    case PH_GUI_ERR_OTHER:
      Std::cerr << "not specified from ph_GuiServer ";
      break;
    case PH_GUI_ERR_FORK:
      Std::cerr << "can't fork GUI-process";
      break;
    case PH_GUI_ERR_MEMORY:
      Std::cerr << "not enough memory";
      break;
    case PH_GUI_ERR_TIMEOUT:
      Std::cerr << "a timeout occured on accessing pipe";
      break;
    case PH_GUI_ERR_WRDATA:
      Std::cerr << "received wrong data";
      break;
    case PH_GUI_ERR_INVALIDE_HDL:
      Std::cerr << "invalide handle";
      break;
    case PH_GUI_ERR_NOHANDLE:
      Std::cerr << "no such handle exists";
      break;
    case PH_GUI_ERR_PARSEGUI:
      Std::cerr << "error in gui description";
      break;
    case PH_GUI_ERR_GUININIT:
      Std::cerr << "gui not initialized";
      break;
  }
  Std::cerr << "\n";
}




/*--- ViewControllerException -----------------------------------------------*/

ViewControllerException::ViewControllerException(eVCException e) : VCexp(e)
{
}

void ViewControllerException::printMessage()
{
  Std::cerr << "ViewController: ";

  switch(VCexp) {
    case formatError:
      Std::cerr << "format error ";
      break;
  }
}


/*--- ViewController -----------------------------------------------------*/
ViewController::ViewController(phLogId_t l,
                               HandlerController *pHandCont,
                               ViewControllerSetup *setup) :
LogObject(l), 
pHandlerController(pHandCont),
noGui(setup->noGui), 
guiId(0),
alivePulse(true)
{
  createView();
  timeout_getGuiData=1;
}


ViewController::ViewController(phLogId_t l, 
                               HandlerController *pHandCont) :
LogObject(l), 
pHandlerController(pHandCont),
noGui(false), 
guiId(0),
alivePulse(true)
{
  timeout_getGuiData=1;
}

ViewController::~ViewController()
{
  phLogSimMessage(logId, LOG_VERBOSE,
                  "ViewController destructor called: destroy %P",
                  guiId);
  if( guiId ) {
    phGuiDestroyDialog(guiId);
    guiId = 0;
  }
}

void ViewController::createView()
{
  phLogSimMessage(logId, LOG_DEBUG,"ViewController::createView() ");
  if( !guiId && !noGui ) {
    phGuiError_t phGuiError;
    char *descr;
    int debug;

    descr = new char[strlen(viewControllerDesc) + 64];

    debug=getDebugFromLogMode(logId);

    sprintf(descr, viewControllerDesc, 
            convertDebugToOptionMenuPosition(debug) );

    phGuiError=phGuiCreateUserDefDialog(&guiId, 
                                        logId, 
                                        PH_GUI_COMM_ASYNCHRON, 
                                        descr);
    delete[] descr;

    if( phGuiError != PH_GUI_ERR_OK ) {
      throw GuiErrorException(phGuiError);
    }
  }
}

void ViewController::setGpibStatus(eGpibStatus status)
{
  ::std::string pStatusMsg;

  switch( status ) {
    case okay:
      pStatusMsg="OK";
      break;
    case waitingMsg:
      pStatusMsg="Waiting to receive message ...";
      break;
    case tryingSendMsg:
      pStatusMsg="Trying to send message ....";
      break;
    case tryingSendSrq:
      pStatusMsg="Trying to send SRQ ...";
      break;
    case timedOut:
      pStatusMsg="TIMED OUT";
      break;
    case notUnderstood:
      pStatusMsg="not understood";
      break;
    case error:
      pStatusMsg="error, see log";
      break;
  }
  changeGuiValue("f_status",pStatusMsg.c_str());
}


void ViewController::setReceiveMsgField(const char *msg)
{
  changeGuiValue("f_req", msg);
}

void ViewController::setSendMsgField(const char *msg)
{
  changeGuiValue("f_answ", msg);
}

void ViewController::setSendSrqField(unsigned char x)
{
  char message[MAX_MSG_LENGTH];

  sprintf(message, "0x%02x", (int)x);
  changeGuiValue("f_srq", message);
}

void ViewController::setSendSrqField(const char *msg)
{
  changeGuiValue("f_srq", msg);
}

void ViewController::showView()
{
  phGuiError_t phGuiError;
  if( guiId ) {
    phGuiError=phGuiShowDialog(guiId, 0);

    if( phGuiError != PH_GUI_ERR_OK ) {
      throw GuiErrorException(phGuiError);
    }
  }
}

void ViewController::checkGuiError(phGuiError_t phGuiError)
{
  switch(phGuiError) {
    case PH_GUI_ERR_OK:
      break;
    case PH_GUI_ERR_TIMEOUT:
      phLogSimMessage(logId, LOG_VERBOSE,
                      "received timeout error from gui server");
      break;
    default:
      phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                      "received error code %d from gui server",
                      (int) phGuiError);
  }

  return ;
}

void ViewController::changeGuiValue(const char *componentName, const char *newValue)
{
  phGuiError_t phGuiError;
  char cmpName[MAX_MSG_LENGTH];
  char newVal[MAX_MSG_LENGTH];

  strcpy(cmpName,componentName);
  strcpy(newVal,newValue);
  if( guiId ) {
//        phLogSimMessage(logId, LOG_DEBUG,
//                        "phGuiChangeValue(P%P,\"%s\",\"%s\")",
//                        guiId, cmpName, newVal);

    phGuiError = phGuiChangeValue(guiId, cmpName, newVal);

    checkGuiError(phGuiError);

    if( phGuiError != PH_GUI_ERR_OK ) {
      throw GuiErrorException(phGuiError);
    }
  }

  return;
}

void ViewController::checkEvent(int timeout)
{
  phGuiError_t phGuiError;
  phGuiEvent_t guiEvent;

  if( guiId ) {
    beAlive();

    do {
      phGuiError = phGuiGetEvent(guiId, &guiEvent, timeout);

      timeout=0;

      checkGuiError(phGuiError);
      if(phGuiError == PH_GUI_ERR_OK) {
        handleEvent(guiEvent);
      }
    } while( phGuiError != PH_GUI_ERR_TIMEOUT );

    /* coverity eliminate
    if( phGuiError != PH_GUI_ERR_TIMEOUT ) {
      throw GuiErrorException(phGuiError);
    }
    */

    getGuiData();
  }

  return;
}

void ViewController::handleEvent(const phGuiEvent_t &guiEvent)
{
  char message[MAX_MSG_LENGTH];

  switch(guiEvent.type) {
    case PH_GUI_NO_EVENT:
      break;
    case PH_GUI_NO_ERROR:
      break;
    case PH_GUI_COMTEST_RECEIVED:
      break;
    case PH_GUI_IDENTIFY_EVENT:
      break;
    case PH_GUI_ERROR_EVENT:
      break;
    case PH_GUI_PUSHBUTTON_EVENT:
    case PH_GUI_EXITBUTTON_EVENT:

      /* now handle the button press event */
      if(strcmp(guiEvent.name, "p_quit") == 0) {
        pHandlerController->quit();
      }
      else if(strcmp(guiEvent.name, "p_start") == 0) 
      {
        pHandlerController->pHandler->start();
      }
      else if(strcmp(guiEvent.name, "p_dump") == 0) {
        pHandlerController->dump();
      } else if(strcmp(guiEvent.name, "p_req") == 0) {
        setGpibStatus(ViewController::waitingMsg);
        pHandlerController->tryReceiveMsg();
      } else if(strcmp(guiEvent.name, "p_answ") == 0) {
        setGpibStatus(ViewController::tryingSendMsg);
        getGuiDataString("f_answ", message);
        pHandlerController->trySendMsg(message);
      } else if(strcmp(guiEvent.name, "p_srq") == 0) {
        unsigned char x;

        setGpibStatus(ViewController::tryingSendSrq);
        getGuiDataString("f_srq", message);

        x = (unsigned char) strtol(message, NULL, 0);

        pHandlerController->trySendSRQ(x);
      }
      break;
  }

  return;
}

void ViewController::beAlive()
{
  if(alivePulse) {
    changeGuiValue("r_life_1","True");
    alivePulse=false;
  } else {
    changeGuiValue("r_life_2","True");
    alivePulse=true;
  }
}

void ViewController::getGuiData()
{
  int debug;

  debug=getGuiDataInt("o_debug_level","debug %d");
  setLogModeDebugLevel(logId,debug);
}


void ViewController::getGuiDataString(
                                     const char *componentName     /* name of the component */,
                                     char *msgBuffer               /* string to set */
                                     )
{
  phGuiError_t phGuiError;
  char cmptName[MAX_MSG_LENGTH];

  strcpy(cmptName,componentName);
  if( guiId ) {
    phGuiError = phGuiGetData(guiId, cmptName, msgBuffer, timeout_getGuiData);

    checkGuiError(phGuiError);

    if( phGuiError != PH_GUI_ERR_OK && phGuiError != PH_GUI_ERR_TIMEOUT ) {
      throw GuiErrorException(phGuiError);
    }
  }

  return;
}


int ViewController::getGuiDataInt(
                                 const char *componentName     /* name of the component */,
                                 const char *format            /* format of returned string */
                                 )
{
  phGuiError_t phGuiError;
  char cmptName[MAX_MSG_LENGTH];
  char response[MAX_MSG_LENGTH];
  int value=0;

  strcpy(cmptName,componentName);

  if( guiId ) {
    phGuiError = phGuiGetData(guiId, cmptName, response, timeout_getGuiData);
    checkGuiError(phGuiError);

    if( phGuiError == PH_GUI_ERR_OK ) {
      if( sscanf(response,format,&value) != 1 ) {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                        "unable to interpret gui server return value \"%s\" with sscanf \n"
                        "format expected %s ",
                        response,format);
        throw ViewControllerException(ViewControllerException::formatError);
      } else {
#if DEBUG
        phLogSimMessage(logId, LOG_VERBOSE,
                        "interpretted component %s returned value \"%s\" as %d \n",
                        componentName,response,value);
#endif
      }
    } else if( phGuiError != PH_GUI_ERR_TIMEOUT ) {
      throw GuiErrorException(phGuiError);
    }
  }
  return value;
}


bool ViewController::getGuiDataBool(
                                   const char *componentName     /* name of the component */
                                   )
{
  phGuiError_t phGuiError;
  char cmptName[MAX_MSG_LENGTH];
  char response[MAX_MSG_LENGTH];
  bool value=false;

  strcpy(cmptName,componentName);

  if( guiId ) {
    phGuiError = phGuiGetData(guiId, cmptName, response, timeout_getGuiData);
    checkGuiError(phGuiError);

    if( phGuiError == PH_GUI_ERR_OK ) {
      if(strcasecmp(response, "true") == 0) {
        value=true;
      } else if(strcasecmp(response, "false") == 0) {
        value=false;
      } else {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                        "unable to interpret gui server return value \"%s\" \n"
                        "format expected True | False ",
                        response);
        throw ViewControllerException(ViewControllerException::formatError);
      }
    } else if( phGuiError != PH_GUI_ERR_TIMEOUT ) {
      throw GuiErrorException(phGuiError);
    }
  }
  return value;
}

void ViewController::setGuiDataBool(
                                   const char *componentName     /* name of the component */,
                                   bool value                    /* value to set */
                                   )
{
  phGuiError_t phGuiError;
  char cmptName[MAX_MSG_LENGTH];

  strcpy(cmptName,componentName);

  if( guiId ) {
    if( value ) {
      phGuiError = phGuiChangeValue(guiId, cmptName, "True");
    } else {
      phGuiError = phGuiChangeValue(guiId, cmptName, "False");
    }
    checkGuiError(phGuiError);

    if( phGuiError != PH_GUI_ERR_OK && phGuiError != PH_GUI_ERR_TIMEOUT ) {
      throw GuiErrorException(phGuiError);
    }
  }
}

int ViewController::convertDebugToOptionMenuPosition(int debug)
{
  /* debug     position */
  /*   -1         1     */
  /*    0         2     */
  /*    1         3     */
  /*    2         4     */
  /*    3         5     */
  /*    4         6     */

  return debug+2;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
