/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : TechWingviewController.C
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : TechWing View Controller Implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, TechWing revision(CR29015)
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce , created
 *            March 2006, Jiawei Lin, CR29015
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
#include "TechWingviewController.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif




/*--- defines ---------------------------------------------------------------*/

const char *TechWingviewControllerDesc =  " \
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
            S::{@h  *p:`p_quit`:`Quit`[`Quit program`] } \
        } \
    } \
";


/*--- typedefs --------------------------------------------------------------*/
/*--- class implementations -------------------------------------------------*/

/*--- ViewController --------------------------------------------------------*/

TechWingViewController::TechWingViewController(phLogId_t l, 
                                   TechWingHandlerController *pTechWingHandCont,
                                   TechWingStatus *pTechWings,
                                   ViewControllerSetup *VCsetup) :
     ViewController(l,pTechWingHandCont),
     pTechWingHandlerController(pTechWingHandCont),
     pTechWingstatus(pTechWings)
{
    noGui=VCsetup->noGui;
    timeout_getGuiData=1;
    pTechWingstatus->registerObserver(this);

    createView();

    setGuiDataBool("t_stop",    pTechWingstatus->getStatusBit(TechWingStatus::stopped) );
    setGuiDataBool("t_alarm",   pTechWingstatus->getStatusBit(TechWingStatus::high_alarm) );
//    setGuiDataBool("t_errorTx", pTechWingstatus->getStatusBit(TechWingStatus::errorTx) );
    setGuiDataBool("t_empty",   pTechWingstatus->getStatusBit(TechWingStatus::handler_empty) );
}

TechWingViewController::~TechWingViewController()
{
    phLogSimMessage(logId, LOG_VERBOSE,
                    "TechWingViewController destructor called: destroy %P",
                    guiId);
}

void TechWingViewController::createView()
{
    const char* title = NULL;


    if ( !guiId && !noGui )
    {
        phGuiError_t phGuiError;
        char *descr;
        int debug;

        switch (  pTechWingHandlerController->getModelType() )
        {
          case TechWingHandlerController::TW2XX:
            title="TechWing GPIB Handler TW2XX";
            break;
          case TechWingHandlerController::TW3XX:
            title="TechWing GPIB Handler TW3XX";
        }

        descr = new char[strlen(TechWingviewControllerDesc) + 256];

        debug=getDebugFromLogMode(logId);

        sprintf(descr, TechWingviewControllerDesc, 
                       title,
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

void TechWingViewController::getGuiData()
{
    bool value;

    value=getGuiDataBool("t_stop");
    pTechWingstatus->setStatusBit(this, TechWingStatus::stopped, value);

    value=getGuiDataBool("t_alarm");
    pTechWingstatus->setStatusBit(this, TechWingStatus::high_alarm, value);

//    value=getGuiDataBool("t_errorTx");
//    pTechWingstatus->setStatusBit(this, TechWingStatus::errorTx, value);

    value=getGuiDataBool("t_empty");
    pTechWingstatus->setStatusBit(this, TechWingStatus::handler_empty, value);

    ViewController::getGuiData();
}

void TechWingViewController::TechWingStatusChanged(TechWingStatus::eTechWingStatus ebit, bool b)
{
    switch (ebit)
    {
      case TechWingStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "TechWingViewController stop detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_stop",b);
        break;
      case TechWingStatus::high_alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "TechWingViewController alarm detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_alarm",b);
        break;
//      case TechWingStatus::errorTx:
//        phLogSimMessage(logId, LOG_VERBOSE, "TechWingViewController errorTx detected new value %s",
//                        (b) ? "True" : "False");
//        setGuiDataBool("t_errorTx",b);
        break;
      case TechWingStatus::handler_empty:
        phLogSimMessage(logId, LOG_VERBOSE, "TechWingViewController empty detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_empty",b);
        break;
      default:
        break;
    }
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
