/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : MiraehandlerController.h
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : Interface header for handler controller implementation class
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

#ifndef _MIRAE_HANDLER_CONT_H_
#define _MIRAE_HANDLER_CONT_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "Miraestatus.h"
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
 * Authors: Chris Joyce
 *
 * Description:
 * Interface for handler controller object
 *
 *****************************************************************************/


struct MiraeHandlerControllerSetup;

class eventRecorder;
class MiraeViewController;


class MiraeHandlerController : public HandlerController, public MiraeStatusObserver
{
public:
    enum eMiraeModel { MR5800, M660, M330 };

    MiraeHandlerController(
        MiraeHandlerControllerSetup   *setup       /* configurable setup values */,
        ViewControllerSetup      *VCsetup       /* view controller */);
    virtual ~MiraeHandlerController();
    virtual void switchOn();
    virtual void MiraeStatusChanged(MiraeStatus::eMiraeStatus, bool b);
    eMiraeModel getModelType() const;
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
    void serviceTestset(const char *msg);
    void serviceRunmode(const char *msg);
    void serviceSettemp(const char *msg);
    void serviceSetband(const char *msg);
    void serviceSetsoak(const char *msg);
    void serviceTempctrl(const char *msg);
    void serviceSitesel(const char *msg);
    void serviceSetname(const char *msg);
    void serviceSitemap(const char *msg);
    void serviceSetbin(const char *msg);
    void serviceLoader(const char *msg);
    void serviceStart(const char *msg);
    void serviceStop(const char *msg);
    void servicePlunger(const char *msg);
    void serviceLotClear(const char *msg);
    void serviceAccClear(const char *msg);

    void serviceVersionQuery(const char *msg);
    void serviceNameQuery(const char *msg);
    void serviceTestsetQuery(const char *msg);
    void serviceSetTempQuery(const char *msg);
    void serviceMeasTempQuery(const char *msg);
    void serviceStatusQuery(const char *msg);
    void serviceJamQuery(const char *msg);
    void serviceJamcodeQuery(const char *msg);
    void serviceJamqueQuery(const char *msg);
    void serviceJamCount(const char *msg);
    void serviceSetlampQuery(const char *msg);

    void serviceTrayIDQuery(const char *msg);
    void serviceLotErrCodeQuery(const char *msg);
    void serviceLotAlmCodeQuery(const char *msg);
    void serviceLotDataQuery(const char *msg);
    void serviceLotTotal2Query(const char *msg);
    void serviceAccSortDvcQuery(const char *msg);
    void serviceAccJamCodeQuery(const char *msg);
    void serviceAccErrCodeQuery(const char *msg);
    void serviceAccDataQuery(const char *msg);
    void serviceHdlStatusQuery(const char *msg);
    void serviceTempModeQuery(const char *msg);

    MiraeStatus *pMiraestatus;
    MiraeViewController *pMiraeViewController; 
    eMiraeModel model;
    bool verifyTest;
    bool multipleSrqs;
private:
    int srqMask;
};


struct MiraeHandlerControllerSetup : public HandlerControllerSetup
{
    MiraeHandlerController::eMiraeModel model;
    bool verifyTest;
    bool multipleSrqs;
    const char *recordFile;
    const char *playbackFile;
};




#endif /* ! _MIRAE_HANDLER_CONT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
