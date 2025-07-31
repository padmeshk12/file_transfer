/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *****************************************************************************/

#ifndef _EQUIPMENT_CMD_H_
#define _EQUIPMENT_CMD_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

/*****************************************************************************
 *
 * Send one-way command to the equipment.
 *
 * Description:
 *    This function sends protocol command to the equipment without waiting for
 *    the response.
 *
 * Returns: CI_CALL_PASS on success
 *
 * Note: The function name is *Gpib* due to historical reason; actually this function
 * can send out command over GPIB, LAN and RS232 connections.
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameExecGpibCmd(
    struct phFrameStruct *f,
    char *commandString,
    char **responseString
);

/*****************************************************************************
 *
 * Send query command to the equipment.
 *
 * Description:
 *    This function sends protocol command to the equipment and then wait until
 *    a response string is received.
 *
 * Returns: CI_CALL_PASS on success
 *
 * Note: The function name is *Gpib* due to historical reason; actually this
 * function can send out command over GPIB, LAN and RS232 connections.
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameExecGpibQuery(
    struct phFrameStruct *f             /* the framework data */,
    char *commandString,
    char **responseString
);

/*****************************************************************************
 *
 * get expected SRQ status byte
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *    This fucntion is used to get a SRQ status byte
 *
 * Input/Output:
 *    Input parameter "commandString" is the expected SRQ list in which each
 *    SRQ value is delimited by a white space. When the list is empty, the
 *    current SRQ in the FIFO will be returned. If no SRQ is currently available
 *    in the FIFO or there is no matched SRQ in the FIFO, this function will
 *    keep waiting. For this case, this function can only be aborted  by ph_interrupt().
 *    The responseString output parameter contains the SRQ returned.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameGetSrqStatusByte(
    struct phFrameStruct *f             /* the framework data */,
    char *commandString,
    char **responseString
);

/*****************************************************************************
 *
 * receive reply from the equipment.
 *
 * Description:
 *    This function wait until a response string is received.
 *
 * Returns: CI_CALL_PASS on success CI_CALL_TIMEOUT on timeout
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameReceive(
    struct phFrameStruct *f             /* the framework data */,
    char *commandString,
    char **responseString
);

/*****************************************************************************
 *
 * Send command to the equipment.
 *
 * Description:
 *    This function sends protocol command to the equipment
 *
 * Returns: CI_CALL_PASS on success CI_CALL_TIMEOUT on timeout
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameSend(
                                    struct phFrameStruct *f,
                                    char *commandString,
                                    char **responseString
);
#endif /* ! _EQUIPMENT_CMD_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
