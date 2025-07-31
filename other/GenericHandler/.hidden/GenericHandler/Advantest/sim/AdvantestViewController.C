/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : AdvantestViewController.C
 * CREATED  : 29 Apri 2014
 *
 * CONTENTS : Advantest View Controller Implementation
 *
 * AUTHORS  : Magco Li, STS-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Apri 2014, Magco Li , created
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

/*--- module includes -------------------------------------------------------*/

#include "simHelper.h"
#include "AdvantestViewController.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */




/*--- defines ---------------------------------------------------------------*/

const char *AdvantestViewControllerDesc =  " \
    S:`name`:`%s` \
    {@v \
        S::``{@v \
            S::{@h  f:`f_req`:`Request`:    p:`p_req`:`Receive`[`Try to receive request`] } \
            S::{@h  f:`f_answ`:`Answer`:    p:`p_answ`:`Send`[`Try to send answer`] } \
            S::{@h  f:`f_srq`:`SRQ`:        p:`p_srq`:`Send`[`Try to send SRQ`] } \
            S::{@h  f:`f_status`:`Status`:  } \
            S::{@h  R::{`r_life_1`:`` `r_life_2`:``}:2:2 * } \
            S::{@h  p:`p_start`:`Start` } \
            S::{@h  p:`p_thermal_alarm`:`Thermal Alarm`[`Try to send Thermal Alarm`] } \
            S::{@h  p:`p_random`:`Random Alarm`[`Randomly send Thermal Alarm, click again to cancel`] } \
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

#include <iostream>
AdvantestViewController::AdvantestViewController(phLogId_t l, 
                                   AdvantestHandlerController *pAdvantestHandCont,
                                   AdvantestStatus *pAdvantestS,
                                   ViewControllerSetup *VCsetup) :
     ViewController(l,pAdvantestHandCont),
     pAdvantestHandlerController(pAdvantestHandCont),
     pAdvantestStatus(pAdvantestS)
{
    noGui=VCsetup->noGui;
    timeout_getGuiData=1;
    pAdvantestStatus->registerObserver(this);
    
    createView();

    setGuiDataBool("t_stop",    pAdvantestStatus->getStatusBit(AdvantestStatus::stopped) );
    setGuiDataBool("t_alarm",   pAdvantestStatus->getStatusBit(AdvantestStatus::alarm) );
    setGuiDataBool("t_errorTx", pAdvantestStatus->getStatusBit(AdvantestStatus::errorTx) );
    setGuiDataBool("t_empty",   pAdvantestStatus->getStatusBit(AdvantestStatus::empty) );
}

AdvantestViewController::~AdvantestViewController()
{
    phLogSimMessage(logId, LOG_VERBOSE,
                    "AdvantestViewController destructor called: destroy %P",
                    guiId);  
}

void AdvantestViewController::createView()
{
    ::std::string title;


    if ( !guiId && !noGui )
    {
        phGuiError_t phGuiError;
        char *descr;
        int debug;

        switch (  pAdvantestHandlerController->getModelType() )
        {
          case AdvantestHandlerController::GS1:
            title="Advantest GPIB Handler GS1";
            break;
          case AdvantestHandlerController::M45:
            title="Advantest GPIB Handler M45";
            break;
          case AdvantestHandlerController::M48:
            title="Advantest GPIB Handler M48";
            break;
          case AdvantestHandlerController::DLT: /* DLT enhance */
            title="Advantest GPIB Handler DLT";
            break;
            /*Begin CR92686, Adam Huang, 4 Feb 2015*/
          case AdvantestHandlerController::Yushan: /* Yushan enhance */
            title="Advantest GPIB Handler Yushan";
            break;
            /*End*/
        }

        descr = new char[strlen(AdvantestViewControllerDesc) + 256];

        debug=getDebugFromLogMode(logId);

        sprintf(descr, AdvantestViewControllerDesc, 
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

void AdvantestViewController::getGuiData()
{
    bool value;

    value=getGuiDataBool("t_stop");
    pAdvantestStatus->setStatusBit(this, AdvantestStatus::stopped, value);

    value=getGuiDataBool("t_alarm");
    pAdvantestStatus->setStatusBit(this, AdvantestStatus::alarm, value);

    value=getGuiDataBool("t_errorTx");
    pAdvantestStatus->setStatusBit(this, AdvantestStatus::errorTx, value);

    value=getGuiDataBool("t_empty");
    pAdvantestStatus->setStatusBit(this, AdvantestStatus::empty, value);

    ViewController::getGuiData();
}

void AdvantestViewController::AdvantestStatusChanged(AdvantestStatus::eAdvantestStatus ebit, bool b)
{
    switch (ebit)
    {
      case AdvantestStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestViewController stop detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_stop",b);
        break;
      case AdvantestStatus::alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestViewController alarm detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_alarm",b);
        break;
      case AdvantestStatus::errorTx:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestViewController errorTx detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_errorTx",b);
        break;
      case AdvantestStatus::empty:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestViewController empty detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_empty",b);
        break;
    }
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
