/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SXviewController.C
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : Synax View Controller Implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce , created
 *            11 Mar 2005, Ken Ward, copied from Seiko-Epson
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
#include <cstring>

/*--- module includes -------------------------------------------------------*/

#include "simHelper.h"
#include "SXviewController.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */




/*--- defines ---------------------------------------------------------------*/

const char *SXviewControllerDesc =  " \
    S:`name`:`%s` \
    {@v \
        S::``{@v \
            S::{@h  f:`f_req`:`Request`:    p:`p_req`:`Receive`[`Try to receive request`] } \
            S::{@h  f:`f_answ`:`Answer`:    p:`p_answ`:`Send`[`Try to send answer`] } \
            S::{@h  f:`f_srq`:`SRQ`:        p:`p_srq`:`Send`[`Try to send SRQ`] } \
            S::{@h  f:`f_status`:`Status`:  } \
            S::{@h  R::{`r_life_1`:`` `r_life_2`:``}:2:2 * } \
            S::{@h  p:`p_start`:`Start` } \
            S::{@h  p:`p_dump`:`Dump`[`Output all debug`] o:`o_debug_level`:  \
                     (`debug -1`        \
                      `debug 0`           \
                      `debug 1`           \
                      `debug 2`           \
                      `debug 3`           \
                      `debug 4`):%d[`Set debug level`]   }  \
            S::{@h  t:`t_stop`:`Stop` } \
            S::{@h  t:`t_alarm`:`Alarm` } \
            S::{@h  t:`t_errorTx`:`Error of transmission` } \
            S::{@h  t:`t_empty`:`Empty` } \
            S::{@h  *p:`p_quit`:`Quit`[`Quit program`] } \
        } \
    } \
";


/*--- typedefs --------------------------------------------------------------*/
/*--- class implementations -------------------------------------------------*/

/*--- ViewController --------------------------------------------------------*/

SXViewController::SXViewController(phLogId_t l, 
                                   SXHandlerController *pSXHandCont,
                                   SynaxStatus *pSXs,
                                   ViewControllerSetup *VCsetup) :
ViewController(l,pSXHandCont),
pSXHandlerController(pSXHandCont),
pSXstatus(pSXs)
{
  noGui=VCsetup->noGui;
  timeout_getGuiData=1;
  pSXstatus->registerObserver(this);

  createView();

  setGuiDataBool("t_stop",    pSXstatus->getStatusBit(SynaxStatus::stopped) );
  setGuiDataBool("t_alarm",   pSXstatus->getStatusBit(SynaxStatus::alarm) );
  setGuiDataBool("t_errorTx", pSXstatus->getStatusBit(SynaxStatus::errorTx) );
  setGuiDataBool("t_empty",   pSXstatus->getStatusBit(SynaxStatus::empty) );
}

SXViewController::~SXViewController()
{
  phLogSimMessage(logId, LOG_VERBOSE,
                  "SXViewController destructor called: destroy %P",
                  guiId);
}

void SXViewController::createView()
{
  ::std::string title;


  if( !guiId && !noGui ) {
    phGuiError_t phGuiError;
    char *descr;
    int debug;

    switch(  pSXHandlerController->getModelType() ) {
      case SXHandlerController::SX1101:
        title="Synax GPIB Handler SX1101";
        break;
      case SXHandlerController::SX1701:
        title="Synax GPIB Handler SX1701";
        break;
      case SXHandlerController::SX2400:
        title="Synax GPIB Handler SX2400";
        break;
    }

    descr = new char[strlen(SXviewControllerDesc) + 256];

    debug=getDebugFromLogMode(logId);

    sprintf(descr, SXviewControllerDesc, 
            title.c_str(),
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

void SXViewController::getGuiData()
{
  bool value;

  value=getGuiDataBool("t_stop");
  pSXstatus->setStatusBit(this, SynaxStatus::stopped, value);

  value=getGuiDataBool("t_alarm");
  pSXstatus->setStatusBit(this, SynaxStatus::alarm, value);

  value=getGuiDataBool("t_errorTx");
  pSXstatus->setStatusBit(this, SynaxStatus::errorTx, value);

  value=getGuiDataBool("t_empty");
  pSXstatus->setStatusBit(this, SynaxStatus::empty, value);

  ViewController::getGuiData();
}

void SXViewController::SXStatusChanged(SynaxStatus::eSXStatus ebit, bool b)
{
  switch(ebit) {
    case SynaxStatus::stopped:
      phLogSimMessage(logId, LOG_VERBOSE, "SXViewController stop detected new value %s",
                      (b) ? "True" : "False");
      setGuiDataBool("t_stop",b);
      break;
    case SynaxStatus::alarm:
      phLogSimMessage(logId, LOG_VERBOSE, "SXViewController alarm detected new value %s",
                      (b) ? "True" : "False");
      setGuiDataBool("t_alarm",b);
      break;
    case SynaxStatus::errorTx:
      phLogSimMessage(logId, LOG_VERBOSE, "SXViewController errorTx detected new value %s",
                      (b) ? "True" : "False");
      setGuiDataBool("t_errorTx",b);
      break;
    case SynaxStatus::empty:
      phLogSimMessage(logId, LOG_VERBOSE, "SXViewController empty detected new value %s",
                      (b) ? "True" : "False");
      setGuiDataBool("t_empty",b);
      break;
  }
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
