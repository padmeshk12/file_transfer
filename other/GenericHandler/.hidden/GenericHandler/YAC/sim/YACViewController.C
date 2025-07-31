/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SEviewController.C
 * CREATED  : 29 Dec 2006
 *
 * CONTENTS : YAC View Controller Implementation
 *
 * AUTHORS  : Kun Xiao, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Dec 2006, Kun Xiao , created
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
#include "YACViewController.h"

/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif




/*--- defines ---------------------------------------------------------------*/

const char *YACViewControllerDesc =  " \
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

YACViewController::YACViewController(phLogId_t l, 
                                   YACHandlerController *pYACHandCont,
                                   YACStatus *pYACS,
                                   ViewControllerSetup *VCsetup) :
     ViewController(l,pYACHandCont),
     pYACHandlerController(pYACHandCont),
     pYACStatus(pYACS)
{
    noGui=VCsetup->noGui;
    timeout_getGuiData=1;
    pYACStatus->registerObserver(this);

    createView();

    setGuiDataBool("t_stop",    pYACStatus->getStatusBit(YACStatus::stopped) );
    setGuiDataBool("t_alarm",   pYACStatus->getStatusBit(YACStatus::alarm) );
    setGuiDataBool("t_errorTx", pYACStatus->getStatusBit(YACStatus::errorTx) );
    setGuiDataBool("t_empty",   pYACStatus->getStatusBit(YACStatus::empty) );
}

YACViewController::~YACViewController()
{
    phLogSimMessage(logId, LOG_VERBOSE,
                    "YACViewController destructor called: destroy %P",
                    guiId);
}

void YACViewController::createView()
{
    const char* title;


    if ( !guiId && !noGui )
    {
        phGuiError_t phGuiError;
        char *descr;
        int debug;

        switch (  pYACHandlerController->getModelType() )
        {
          case YACHandlerController::HYAC8086:
            title="YAC GPIB Handler HYAC8086";
            break;
          default:
            break;
        }

        descr = new char[strlen(YACViewControllerDesc) + 256];

        debug=getDebugFromLogMode(logId);

        sprintf(descr, YACViewControllerDesc, 
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

void YACViewController::getGuiData()
{
    bool value;

    value=getGuiDataBool("t_stop");
    pYACStatus->setStatusBit(this, YACStatus::stopped, value);

    value=getGuiDataBool("t_alarm");
    pYACStatus->setStatusBit(this, YACStatus::alarm, value);

    value=getGuiDataBool("t_errorTx");
    pYACStatus->setStatusBit(this, YACStatus::errorTx, value);

    value=getGuiDataBool("t_empty");
    pYACStatus->setStatusBit(this, YACStatus::empty, value);

    ViewController::getGuiData();
}

void YACViewController::YACStatusChanged(YACStatus::eYACStatus ebit, bool b)
{
    switch (ebit)
    {
      case YACStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "YACViewController stop detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_stop",b);
        break;
      case YACStatus::alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "YACViewController alarm detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_alarm",b);
        break;
      case YACStatus::errorTx:
        phLogSimMessage(logId, LOG_VERBOSE, "YACViewController errorTx detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_errorTx",b);
        break;
      case YACStatus::empty:
        phLogSimMessage(logId, LOG_VERBOSE, "YACViewController empty detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_empty",b);
        break;
    }
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
