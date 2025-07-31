/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SEviewController.C
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : Seiko-Epson View Controller Implementation
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
#include <cstring>

/*--- module includes -------------------------------------------------------*/

#include "simHelper.h"
#include "SEviewController.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */




/*--- defines ---------------------------------------------------------------*/

const char *SEviewControllerDesc =  " \
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

SEViewController::SEViewController(phLogId_t l, 
                                   SEHandlerController *pSEHandCont,
                                   SeikoEpsonStatus *pSEs,
                                   ViewControllerSetup *VCsetup) :
     ViewController(l,pSEHandCont),
     pSEHandlerController(pSEHandCont),
     pSEstatus(pSEs)
{
    noGui=VCsetup->noGui;
    timeout_getGuiData=1;
    pSEstatus->registerObserver(this);

    createView();

    setGuiDataBool("t_stop",    pSEstatus->getStatusBit(SeikoEpsonStatus::stopped) );
    setGuiDataBool("t_alarm",   pSEstatus->getStatusBit(SeikoEpsonStatus::alarm) );
    setGuiDataBool("t_errorTx", pSEstatus->getStatusBit(SeikoEpsonStatus::errorTx) );
    setGuiDataBool("t_empty",   pSEstatus->getStatusBit(SeikoEpsonStatus::empty) );
}

SEViewController::~SEViewController()
{
    phLogSimMessage(logId, LOG_VERBOSE,
                    "SEViewController destructor called: destroy %P",
                    guiId);  
}

void SEViewController::createView()
{
    ::std::string title;


    if ( !guiId && !noGui )
    {
        phGuiError_t phGuiError;
        char *descr;
        int debug;

        switch (  pSEHandlerController->getModelType() )
        {
          case SEHandlerController::NS5000:
            title="Seiko-Epson GPIB Handler NS5000";
            break;
          case SEHandlerController::NS6000:
            title="Seiko-Epson GPIB Handler NS6000";
            break;
          case SEHandlerController::NS7000:
            title="Seiko-Epson GPIB Handler NS7000";
            break;
          case SEHandlerController::HONTECH:
            title="HONTECH GPIB Handler";
            break;
          case SEHandlerController::NS6040:
            title="Seiko-Epson GPIB Handler NS6040";
            break;
          case SEHandlerController::NS8040:
            title="Seiko-Epson GPIB Handler NS8040";
            break;
          case SEHandlerController::CHROMA:
            title="Seiko-Epson GPIB Handler CHROMA";
            break;
        }

        descr = new char[strlen(SEviewControllerDesc) + 256];

        debug=getDebugFromLogMode(logId);

        sprintf(descr, SEviewControllerDesc, 
                       title.c_str(),
                       convertDebugToOptionMenuPosition(debug) );

        phGuiError=phGuiCreateUserDefDialog(&guiId, 
                                          logId, 
                                          PH_GUI_COMM_ASYNCHRON, 
                                          descr);
        delete[] descr;

        if ( phGuiError != PH_GUI_ERR_OK )
        {
            throw GuiErrorException(phGuiError);
        }
    }
}

void SEViewController::getGuiData()
{
    bool value;

    value=getGuiDataBool("t_stop");
    pSEstatus->setStatusBit(this, SeikoEpsonStatus::stopped, value);

    value=getGuiDataBool("t_alarm");
    pSEstatus->setStatusBit(this, SeikoEpsonStatus::alarm, value);

    value=getGuiDataBool("t_errorTx");
    pSEstatus->setStatusBit(this, SeikoEpsonStatus::errorTx, value);

    value=getGuiDataBool("t_empty");
    pSEstatus->setStatusBit(this, SeikoEpsonStatus::empty, value);

    ViewController::getGuiData();
}

void SEViewController::SEStatusChanged(SeikoEpsonStatus::eSEStatus ebit, bool b)
{
    switch (ebit)
    {
      case SeikoEpsonStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "SEViewController stop detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_stop",b);
        break;
      case SeikoEpsonStatus::alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "SEViewController alarm detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_alarm",b);
        break;
      case SeikoEpsonStatus::errorTx:
        phLogSimMessage(logId, LOG_VERBOSE, "SEViewController errorTx detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_errorTx",b);
        break;
      case SeikoEpsonStatus::empty:
        phLogSimMessage(logId, LOG_VERBOSE, "SEViewController empty detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_empty",b);
        break;
    }
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
