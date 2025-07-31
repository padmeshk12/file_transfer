/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : DeltaHController.h
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : Interface header for Delta handler controller class
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 23 Feb 2001, Chris Joyce, created
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

#ifndef _DELTA_HANDLER_CONTROLLER_H_
#define _DELTA_HANDLER_CONTROLLER_H_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"

/*--- defines ---------------------------------------------------------------*/


// enum deltaModel { Castle, Matrix, Flex, Rfs, Summit, Orion, Eclipse };



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



/*****************************************************************************
 *
 * Delta Handler Controller Implementation
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Interface for handler controller object
 *
 *****************************************************************************/

struct DeltaHandlerControllerSetup;


class DeltaHandlerController : public HandlerController
{
public:
    enum status   { e_retestOrtestagain, 
                    e_outputFull, 
                    e_handlerEmpty, 
                    e_motorDiagEnabled,
                    e_dymContactorCalEnabled,
                    e_inputInhibited,
                    e_sortInhibited,
                    e_outOfGuardBands,
                    e_jammed,
                    e_stopped,
                    e_partsSoaking,
                    e_inputOrSortInhibitedOrDoorOpen };

    DeltaHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~DeltaHandlerController();
    virtual void switchOn(deltaModel model);
    virtual void statusChanged(const HandlerStatusBit&);
    virtual const char *getName() const = 0;
protected:
    virtual unsigned char getSrqByte();
    virtual void handleMessage(char *message);
    void implHandleMessage(char *message);
    virtual void service_identify(const char *msg) = 0;
    virtual void service_which(const char *msg) = 0;
    virtual void service_testpartsready(const char *msg);
    virtual void service_testresults(const char *msg);
    virtual void service_start(const char *msg);
    virtual void service_numtestsites(const char *msg);
    virtual void service_sitespacing(const char *msg);
    virtual void service_sites_disabled(const char *msg);
    virtual void service_killchuck(const char *msg);
    virtual void service_verifytestCommand(const char *msg);
    virtual void service_verifytestQuery(const char *msg);
    virtual void service_cmdReplyCommand(const char *msg);
    virtual void service_cmdReplyQuery(const char *msg);
    virtual void service_systemModeQuery(const char *msg);
    virtual void service_systemModeCommand(const char *msg);
    virtual void service_emulationModeQuery(const char *msg);
    virtual void service_emulationModeCommand(const char *msg);
    virtual void service_errorclear(const char *msg);
    virtual void service_srqmask(const char *msg);
    virtual void service_srqmaskQuery(const char *msg);
    virtual void service_general(const char *msg);
    virtual void service_status(const char *msg);
    virtual void service_srqbyte(const char *msg);
    virtual void service_querybins(const char *msg);
    virtual void service_releaseparts(const char *msg);
    virtual void service_stop(const char *msg);
    virtual void service_temp_command(const char *msg);
    virtual void service_reprobe(const char *msg);
    virtual void service_bin(const char *msg);
    virtual void service_stripmap(const char *msg);
    virtual void service_index(const char *msg);
    virtual void service_materialid(const char *msg);
    virtual void service_sitemapping(const char *msg);
    virtual int  getBinDataNum();
    virtual void serviceBinon(const char *msg);
    virtual void getBinNum(int site, int bin, const char *binFirstbit, const char *binSecondBit);
    virtual int  hextoInt(const char *binFirstbit, const char *binSecondBit);
    virtual void serviceEcho();
    virtual void serviceEchoNg(const char *msg);
    virtual void serviceEchoOk(const char *msg);
    virtual void serviceDl(const char *msg);
    virtual void serviceSrqKind(const char *msg);


    bool getHandlerIsReady() const;

    bool                      releasepartsWhenReady;
    bool                      commandReply;
    const char                *commandReplyMessage;
    const char                *softwareVersion;
    const char                *handlerNumber;
    int                       systemMode;
    int                       emulationMode;
    int                       errorBit;
    int                       dl;
    int                       retestTime;             //retest model
};

struct DeltaHandlerControllerSetup : public HandlerControllerSetup
{
    bool             releasepartsWhenReady;
    int              retestTime;             //retest model
};


class CastleHandlerController : public DeltaHandlerController
{
public:
    CastleHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~CastleHandlerController();
    virtual const char *getName() const;
protected:
    virtual void service_identify(const char *msg);
    virtual void service_which(const char *msg);
};


class MatrixHandlerController : public DeltaHandlerController
{
public:
    MatrixHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~MatrixHandlerController();
    virtual const char *getName() const;
protected:
    virtual void service_identify(const char *msg);
    virtual void service_which(const char *msg);
};


class FlexHandlerController : public DeltaHandlerController
{
public:
    FlexHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~FlexHandlerController();
    virtual const char *getName() const;
protected:
    virtual void handleMessage(char *message);
    void implHandleMessage(char *message);
    virtual void service_identify(const char *msg);
    virtual void service_which(const char *msg);
    virtual void service_retract(const char *msg);
    virtual void service_contact(const char *msg);
};


class RfsHandlerController : public DeltaHandlerController
{
public:
    RfsHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~RfsHandlerController();
    virtual const char *getName() const;
protected:
    virtual void handleMessage(char *message);
    void implHandleMessage(char *message);
    virtual void service_start(const char *msg);
    virtual void service_identify(const char *msg);
    virtual void service_which(const char *msg);
    virtual void service_site_disabled(const char *msg);
};


class SummitHandlerController : public DeltaHandlerController
{
public:
    SummitHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~SummitHandlerController();
    virtual const char *getName() const;
protected:
    virtual void handleMessage(char *message);
    void implHandleMessage(char *message);
    virtual unsigned char getSrqByte();
    virtual void service_identify(const char *msg);
    virtual void service_which(const char *msg);
    virtual void service_temp_command(const char *msg);
    virtual void service_reprobe(const char *msg);
    virtual void service_recipestart(const char *msg);
    virtual void service_recipeend(const char *msg);
private:
    enum { MAX_RECIPE_NAME_LENGTH = 64 };

};


class OrionHandlerController : public DeltaHandlerController
{
public:
    OrionHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~OrionHandlerController();
    virtual const char *getName() const;
protected:
    virtual void service_identify(const char *msg);
    virtual void service_which(const char *msg);
    virtual void service_testpartsready(const char *msg);
    virtual void service_stripmap(const char *msg);
    virtual void service_index(const char *msg);
    virtual void service_materialid(const char *msg);
    virtual unsigned char getSrqByte();
};

class EclipseHandlerController : public DeltaHandlerController
{
public:
    EclipseHandlerController(DeltaHandlerControllerSetup   *setup);
    virtual ~EclipseHandlerController();
    virtual const char *getName() const;
protected:
    virtual void service_identify(const char *msg);
    virtual void service_which(const char *msg);
    virtual void service_materialid(const char *msg);
};

#endif /* ! _DELTA_HANDLER_CONTROLLER_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
