/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : HandlerViewController.h
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : Interface header for Handler View Controller template
 *            implementation class
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 5 Jan 2015, Magco Li, created
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

#ifndef _HANDLER_VIEW_CONT_IMPL_H_
#define _HANDLER_VIEW_CONT_IMPL_H_

/*--- system includes -------------------------------------------------------*/

#include <string>

/*--- module includes -------------------------------------------------------*/

#include "ph_GuiServer.h"


/*--- module includes -------------------------------------------------------*/

#include "viewController.h"
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

template<class T> class HandlerViewController : public ViewController, public StatusObserver<T>
{
public:
    HandlerViewController(phLogId_t l,
                          HandlerController *pHandCont,
                          Status<T> *pS,
                          ViewControllerSetup *VCsetup);
    ~HandlerViewController();
    virtual void getGuiData();
    virtual void StatusChanged(T, bool b);
protected:
    virtual void createView();
    HandlerController *pHandlerController;
    Status<T> *pStatus;
private:
    const char *createToggleKey(const char *name);
};



/*--- defines ---------------------------------------------------------------*/

/*--- class implementations -------------------------------------------------*/

/*--- ViewController --------------------------------------------------------*/
/*--- HandlerViewController constructor -------------------------------------*/


/*--- defines ---------------------------------------------------------------*/

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
}

template<class T> HandlerViewController<T>::~HandlerViewController()
{
    phLogSimMessage(logId, LOG_VERBOSE,
                    "HandlerViewController destructor called: destroy %P",
                    guiId);
}



/*--- HandlerViewController public member functions -------------------------*/

template<class T> void HandlerViewController<T>::createView()
{
    if ( !guiId && !noGui )
    {
        const char* title;
        const char *toggleButtonTemplate="S::{@h  t:`%s`:`%s` }";
        char tmpStatusField[128];
        phGuiError_t phGuiError;
        char *descr;
        char *statusDescr;
        int debug;

        /* title */
        title=pHandlerController->getModelName();
        statusDescr = new char[ 128 * pStatus->getNumStatusLevels() ];

        strcpy(statusDescr,"");
        StatusPtr<T> sp=pStatus->getStatusPtr();
        phLogSimMessage(logId, LOG_VERBOSE,"HandlerViewController:: createView ");
        for ( sp.begin() ; !(sp.atEnd()) ; ++sp )
        {
            if ( pStatus->getStatusVisible(*sp) )
            {
                sprintf(tmpStatusField, toggleButtonTemplate,
                                        createToggleKey(pStatus->getStatusName(*sp)),
                                        pStatus->getStatusName(*sp) );
                strcat(statusDescr,tmpStatusField);
                phLogSimMessage(logId, LOG_VERBOSE,"statusDescr is %s ",statusDescr);
            }
        }
        debug=getDebugFromLogMode(logId);

        descr = new char[strlen(viewControllerDesc) + strlen(title) + strlen(statusDescr) + 128];
        sprintf(descr, viewControllerDesc,
                       title,
                       convertDebugToOptionMenuPosition(debug),
                       statusDescr );

        phGuiError=phGuiCreateUserDefDialog(&guiId,
                                            logId,
                                            PH_GUI_COMM_ASYNCHRON,
                                            descr);
        delete[] statusDescr;
        delete[] descr;

        if ( phGuiError != PH_GUI_ERR_OK )
        {
            throw GuiErrorException(phGuiError);
        }

        for ( sp.begin() ; !sp.atEnd() ; ++sp )
        {
            if ( pStatus->getStatusVisible(*sp) )
            {
                setGuiDataBool( createToggleKey( pStatus->getStatusName(*sp)),
                                pStatus->getStatusBit(*sp) );
            }
        }
    }
}

template<class T> void HandlerViewController<T>::getGuiData()
{
    bool value;

    StatusPtr<T> sp=pStatus->getStatusPtr();
    for ( sp.begin() ; !sp.atEnd() ; ++sp )
    {
        if ( pStatus->getStatusVisible(*sp) )
        {
            value=getGuiDataBool(createToggleKey(pStatus->getStatusName(*sp)));
            pStatus->setStatusBit(this, *sp, value);
        }
    }
}

template<class T> void HandlerViewController<T>::StatusChanged(T ebit, bool b)
{
    phLogSimMessage(logId, LOG_VERBOSE, "HandlerViewController %s detected new value %s",
                    pStatus->getStatusName(ebit),
                    (b) ? "True" : "False");
    setGuiDataBool(createToggleKey(pStatus->getStatusName(ebit)),b);
}

/*--- HandlerViewController private member functions ------------------------*/


template<class T> const char *HandlerViewController<T>::createToggleKey(const char *name)
{
    static char buffer[256] = "";
    Std::string keyName="t_";

    keyName+=name;
    strncpy(buffer, keyName.c_str(), sizeof(buffer));
    buffer[sizeof(buffer)-1] = 0;

    return buffer;
}



#endif /* ! _HANDLER_VIEW_CONT_IMPL_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
