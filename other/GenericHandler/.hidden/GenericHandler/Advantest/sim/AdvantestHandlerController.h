/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : AdvantestHandlerController.h
 * CREATED  : 29 Apri 2014
 *
 * CONTENTS : Interface header for handler controller implementation class
 *
 * AUTHORS  : Magco Li, STS-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Apri 2014, Magco Li, created
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

#ifndef _ADVANTEST_HANDLER_CONT_H_
#define _ADVANTEST_HANDLER_CONT_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "AdvantestStatus.h"
#include "handler.h"

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/




/*--- typedefs --------------------------------------------------------------*/


/*--- class definition  -----------------------------------------------------*/

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



/*****************************************************************************
 *
 * Handler Controller Implementation
 *
 * Authors: Magco Li
 *
 * Description:
 * Interface for handler controller object
 *
 *****************************************************************************/


struct AdvantestHandlerControllerSetup;

class eventRecorder;
class AdvantestViewController;


class AdvantestHandlerController : public HandlerController, public AdvantestStatusObserver
{
public:
    /*Begin CR92686, Adam Huang, 4 Feb 2015*/
    enum eAdvantestModel { GS1, M45, M48, DLT, Yushan }; /* add DLT for DLT enhance*/
    /*End*/

    AdvantestHandlerController(
        AdvantestHandlerControllerSetup   *setup       /* configurable setup values */,
        ViewControllerSetup      *VCsetup       /* view controller */);
    virtual ~AdvantestHandlerController();
    virtual void switchOn();
    virtual void AdvantestStatusChanged(AdvantestStatus::eAdvantestStatus, bool b);
    eAdvantestModel getModelType() const;
protected:
    virtual void handleMessage(char *message);
    virtual unsigned char getSrqByte();
    void serviceStatus(const char *msg);
    void serviceFullsites(const char *msg);
    void serviceBinon(const char *msg);
    void serviceEcho();
    void serviceEchoOk(const char *msg);
    void serviceEchoNg(const char *msg);
    void serviceSrqmask(const char *msg);
    void serviceDl(const char *msg);
    void serviceSrqKind(const char *msg);
    void serviceSrq(const char *msg);
    void serviceBinoff(const char *msg);
    void serviceHandlerNum(const char *msg);
    void serviceHdlStatus(const char *msg);
    void serviceMeasTemp(const char *msg);
    void serviceSetTemp(const char *msg);
    void serviceStart(const char *msg);
    void serviceStop(const char *msg);
    void serviceVersion(const char *msg);
    void serviceLdPocket(const char *msg);  /* LDPOCKET command reply func to enhance DLT */
    void serviceLdTray(const char *msg);    /* LDTRAY command reply func to enhance DLT*/
    /*Begin CR92686, Adam Huang, 1 Feb 2015*/
    void serviceDvid(const char *msg);
    void serviceTloop(const char *msg);
    void serviceTsset(const char *msg);
    /*End*/
    void serviceOvertemp(const char *msg);
    int getBinDataNum();
    void getBinNum(int site, int bin, const char *binFirstbit, const char *binSecondBit);
    int hextoInt(const char *binFirstbit, const char *binSecondBit);


    AdvantestStatus *pAdvantestStatus;
   
    AdvantestViewController *pAdvantestViewController; 
    eAdvantestModel model;
    bool verifyTest;
    int retestTime;             //retest model
   
private:
    int srqMask;
    int dl;
    int srq;
};


struct AdvantestHandlerControllerSetup : public HandlerControllerSetup
{
    AdvantestHandlerController::eAdvantestModel model;
    bool verifyTest; 
    int retestTime;             //retest model
    const char *recordFile;
    const char *playbackFile;
};




#endif /* ! _ADVANTEST_HANDLER_CONT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
