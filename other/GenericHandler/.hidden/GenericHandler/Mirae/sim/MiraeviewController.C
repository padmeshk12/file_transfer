/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : MiraeviewController.C
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : Mirae View Controller Implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Ken Ward, BitsoftSystems, Mirae revision
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
#include "MiraeviewController.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */




/*--- defines ---------------------------------------------------------------*/

const char *MiraeviewControllerDesc =  " \
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

MiraeViewController::MiraeViewController(phLogId_t l, 
                                   MiraeHandlerController *pMiraeHandCont,
                                   MiraeStatus *pMiraes,
                                   ViewControllerSetup *VCsetup) :
     ViewController(l,pMiraeHandCont),
     pMiraeHandlerController(pMiraeHandCont),
     pMiraestatus(pMiraes)
{
    noGui=VCsetup->noGui;
    timeout_getGuiData=1;
    pMiraestatus->registerObserver(this);

    createView();

    setGuiDataBool("t_stop",    pMiraestatus->getStatusBit(MiraeStatus::stopped) );
    setGuiDataBool("t_alarm",   pMiraestatus->getStatusBit(MiraeStatus::high_alarm) );
//    setGuiDataBool("t_errorTx", pMiraestatus->getStatusBit(MiraeStatus::errorTx) );
    setGuiDataBool("t_empty",   pMiraestatus->getStatusBit(MiraeStatus::handler_empty) );
}

MiraeViewController::~MiraeViewController()
{
    phLogSimMessage(logId, LOG_VERBOSE,
                    "MiraeViewController destructor called: destroy %P",
                    guiId);    
}

void MiraeViewController::createView()
{
    const char* title = NULL;


    if ( !guiId && !noGui )
    {
        phGuiError_t phGuiError;
        char *descr;
        int debug;

        switch (  pMiraeHandlerController->getModelType() )
        {
          case MiraeHandlerController::MR5800:
            title="Mirae GPIB Handler MR5800";
            break;
          case MiraeHandlerController::M660:
            title="Mirae GPIB Handler M660";
            break;
          case MiraeHandlerController::M330:
            title="Mirae GPIB Handler M330";
            break;
        }

        descr = new char[strlen(MiraeviewControllerDesc) + 256];

        debug=getDebugFromLogMode(logId);

        sprintf(descr, MiraeviewControllerDesc,
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

void MiraeViewController::getGuiData()
{
    bool value;

    value=getGuiDataBool("t_stop");
    pMiraestatus->setStatusBit(this, MiraeStatus::stopped, value);

    value=getGuiDataBool("t_alarm");
    pMiraestatus->setStatusBit(this, MiraeStatus::high_alarm, value);

//    value=getGuiDataBool("t_errorTx");
//    pMiraestatus->setStatusBit(this, MiraeStatus::errorTx, value);

    value=getGuiDataBool("t_empty");
    pMiraestatus->setStatusBit(this, MiraeStatus::handler_empty, value);

    ViewController::getGuiData();
}

void MiraeViewController::MiraeStatusChanged(MiraeStatus::eMiraeStatus ebit, bool b)
{
    switch (ebit)
    {
      case MiraeStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "MiraeViewController stop detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_stop",b);
        break;
      case MiraeStatus::high_alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "MiraeViewController alarm detected new value %s",
                        (b) ? "True" : "False");
        setGuiDataBool("t_alarm",b);
        break;
//      case MiraeStatus::errorTx:
//        phLogSimMessage(logId, LOG_VERBOSE, "MiraeViewController errorTx detected new value %s",
//                        (b) ? "True" : "False");
//        setGuiDataBool("t_errorTx",b);
        break;
      case MiraeStatus::handler_empty:
        phLogSimMessage(logId, LOG_VERBOSE, "MiraeViewController empty detected new value %s",
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
