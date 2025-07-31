/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : MiraeviewController.h
 * CREATED  : 22 Jan 2001
 *
 * CONTENTS : Interface header for Mirae view controller implementation 
 *            class
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Ken Ward, BitsoftSystems, Mirae revision
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

#ifndef _MIRAE_VIEW_CONT_IMPL_H_
#define _MIRAE_VIEW_CONT_IMPL_H_

/*--- system includes -------------------------------------------------------*/

#include <string>

/*--- module includes -------------------------------------------------------*/

#include "ph_GuiServer.h"
#include "viewController.h"
#include "MiraehandlerController.h"
#include "Miraestatus.h"
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



class MiraeViewController : public ViewController, public MiraeStatusObserver
{
public:
    MiraeViewController(phLogId_t l,
                     MiraeHandlerController *pMiraeHandCont,
                     MiraeStatus *pMiraes,
                     ViewControllerSetup *VCsetup);
    ~MiraeViewController();
    virtual void createView();
    virtual void getGuiData();
    virtual void MiraeStatusChanged(MiraeStatus::eMiraeStatus, bool b);
protected:
    MiraeHandlerController *pMiraeHandlerController;
    MiraeStatus *pMiraestatus;
};



template<class T> class HandlerViewController : public ViewController, public StatusObserver<T>
{
public:
    HandlerViewController(phLogId_t l,
                          HandlerController *pHandCont,
                          Status<T> *pS,
                          ViewControllerSetup *VCsetup);
    ~HandlerViewController();
    virtual void createView();
    virtual void getGuiData();
    virtual void StatusChanged(T, bool b);
protected:
    HandlerController *pHandlerController;
    Status<T> *pStatus;
private:
    const char *createToggleKey(const char *name);
};



/*--- defines ---------------------------------------------------------------*/
/* Begin of Huatek Modification, Donnie Tu, 12/05/2001 */
/* Issue Number: 319 */
/* const char *handlerViewControllerDesc =  " \
     S:`name`:`%s` \
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
*/
/* End of Huatek Modification */
//
//
//
/* Begin of Huatek Modification, Donnie Tu, 12/05/2001  */
/* Issue Number: 319 */
/*             S::{@h  t:`t_stop`:`Stop` } \
             S::{@h  t:`t_alarm`:`Alarm` } \
             S::{@h  t:`t_errorTx`:`Error of transmission` } \
             S::{@h  t:`t_empty`:`Empty` } \
            S::{@h  *p:`p_quit`:`Quit`[`Quit program`] } \
         } \
     } \
 ";
*/
/* End of Huatek Modification */


/*--- class implementations -------------------------------------------------*/

/*--- ViewController --------------------------------------------------------*/
/*--- HandlerViewController constructor -------------------------------------*/

template<class T> HandlerViewController<T>::HandlerViewController(phLogId_t l,
                          HandlerController *pHandCont,
                          Status<T> *pS,
                          ViewControllerSetup *VCsetup) :
     ViewController(l,pHandCont),
     pHandlerController(pHandCont),
     pStatus(pS)
{
    noGui=VCsetup->noGui;
    timeout_getGuiData=1;
    pStatus->registerObserver(this);

    createView();

//    setGuiDataBool("t_stop",    pMiraestatus->getStatusBit(MiraeStatus::stopped) );
//    setGuiDataBool("t_alarm",   pMiraestatus->getStatusBit(MiraeStatus::alarm) );
//    setGuiDataBool("t_errorTx", pMiraestatus->getStatusBit(MiraeStatus::errorTx) );
//    setGuiDataBool("t_empty",   pMiraestatus->getStatusBit(MiraeStatus::empty) );
}

/*--- HandlerViewController public member functions -------------------------*/

template<class T> void HandlerViewController<T>::createView()
{
//    const char* title;
//
//    if ( !guiId && !noGui )
//    {
//        phGuiError_t phGuiError;
//        char *descr;
//        int debug;
//
//        switch (  pMiraeHandlerController->getModelType() )
//        {
//          case MiraeHandlerController::MR5800:
//            title="Mirae GPIB Handler MR5800";
//            break;
//        }
//
//        descr = new char[strlen(MiraeviewControllerDesc) + 256];
//
//
/* Begin of Huatek Modification, Donnie Tu, 12/05/2001 */
/* Issue Number: 319*/
/*             S::{@h  t:`t_alarm`:`Alarm` } \ */
/* End of Huatek Modification */
//
//
//        debug=getDebugFromLogMode(logId);
//
//        sprintf(descr, MiraeviewControllerDesc,
//                       title,
//                       convertDebugToOptionMenuPosition(debug) );
//
//        phGuiError=phGuiCreateUserDefDialog(&guiId,
//                                          logId,
//                                          PH_GUI_COMM_ASYNCHRON,
//                                          descr);
//        delete[] descr;
//
//        if ( phGuiError != PH_GUI_ERR_OK )
//        {
//            throw GuiErrorException(phGuiError);
//        }
//    }
}







/*--- HandlerViewController private member functions ------------------------*/


template<class T> const char *HandlerViewController<T>::createToggleKey(const char *name)
{
    Std::string keyName="t_";

    keyName+=name;

    return keyName.c_str();
}






#endif /* ! _MIRAE_VIEW_CONT_IMPL_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
