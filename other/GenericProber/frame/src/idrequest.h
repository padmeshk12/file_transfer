/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : idrequest.h
 * CREATED  : 24 Jan 2000
 *
 * CONTENTS : Retrieve identifications from various places
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR 27092 & CR 25172
 *            Garry Xie,R&D Shanghai, CR28427
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 24 Jan 2000, Michael Vogt, created
 *            Aug 2005, CR27092 & CR 25172
 *              Implement the function "phFrameGetStatus".
 *              It could be used to request parameter/information
 *              from Prober. Thus more datalog are possible, like WCR(Wafer 
 *              Configuration Record),Wafer_ID, Wafer_Number, Cassette Status 
 *              and Chuck Temperature.
 *            February 2006, Garry Xie , CR28427
 *              Implement the function "phFrameSetStatus".
 *              It could be used to set parameter/information
 *              to Prober. like Alarm_buzzer.
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

#ifndef _IDREQUEST_H_
#define _IDREQUEST_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

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
 * retrieve a specific ID value
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameGetID(
    struct phFrameStruct *f             /* the framework data */,
    const char *idRequestString,
    const char **outputString
);

/*****************************************************************************
 *
 * retrieve specific status/parameter values from Prober.
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *    It is one of "getStatus" series functions and performs the necessary
 *    actions in Framework side.
 *
 * Input/Output:
 *    Input parameter "commandString" is the key name for information request.
 *    Output the result of request to "outputString".
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameGetStatus(
    struct phFrameStruct *f             /* the framework data */,
    char *commandString,
    char **responseString
);

/*****************************************************************************
 *
 * set specific status/parameter values from Prober.
 *
 * Authors: Garry xie
 *
 * Description:
 *    It is one of "setStatus" series functions and performs the necessary
 *    actions in Framework side.
 *
 * Input/Output:
 *    Input parameter "commandString" is the key name for information request.
 *    Output the result of request to "outputString".
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameSetStatus(
    struct phFrameStruct *f             /* the framework data */,
    char *commandString,
    char **responseString
);

#endif /* ! _IDREQUEST_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
