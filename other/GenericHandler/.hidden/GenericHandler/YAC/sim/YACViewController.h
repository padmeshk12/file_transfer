/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : YACviewController.h
 * CREATED  : 29 Dec 2006
 *
 * CONTENTS : Interface header for YAC view controller implementation 
 *            class
 *
 * AUTHORS  : Kun Xiao, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Dec 2006, Kun Xiao, created
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

#ifndef _SE_VIEW_CONT_IMPL_H_
#define _SE_VIEW_CONT_IMPL_H_

/*--- system includes -------------------------------------------------------*/

#include <string>

/*--- module includes -------------------------------------------------------*/

#include "ph_GuiServer.h"
#include "viewController.h"
#include "YACHandlerController.h"
#include "YACStatus.h"
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



class YACViewController : public ViewController, public YACStatusObserver
{
public:
    YACViewController(phLogId_t l,
                     YACHandlerController *pYACHandCont,
                     YACStatus *pYACS,
                     ViewControllerSetup *VCsetup);
    ~YACViewController();
    virtual void createView();
    virtual void getGuiData();
    virtual void YACStatusChanged(YACStatus::eYACStatus, bool b);
protected:
    YACHandlerController *pYACHandlerController;
    YACStatus *pYACStatus;
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

}

/*--- HandlerViewController public member functions -------------------------*/

template<class T> void HandlerViewController<T>::createView()
{
}







/*--- HandlerViewController private member functions ------------------------*/


template<class T> const char *HandlerViewController<T>::createToggleKey(const char *name)
{
    Std::string keyName="t_";

    keyName+=name;

    return keyName.c_str();
}






#endif /* ! _SE_VIEW_CONT_IMPL_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
