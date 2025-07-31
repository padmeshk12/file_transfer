/******************************************************************************
 *
 *       (c) Copyright Advantest
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_rs232_mhcom.h
 * CREATED  : Jun 2015
 *
 * CONTENTS : RS232 communication layer entry point
 *
 * AUTHORS  : Magco Li, Shanghai R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : Jun 2015, Magco Li, created
 *
 *
 *
 *
 *****************************************************************************/

#ifndef _PH_RS232_MHCOM_H_
#define _PH_RS232_MHCOM_H_

#ifdef __C2MAN__
#include "ph_mhcom.h"
#endif


/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/**
 * Error codes.
 *
 * Almost all functions return an error code of this type.
 * All of them, when they encounter an error
 *
 * @attention
 * Client applications should normally not check for specific error codes,
 * because not all access methods will return the same set of errors.
 * Exceptions to this rule are codes like @c RS232_ERR_NO_ERROR (i.e., success)
 * and @c RS232_ERR_IO_TIMEOUT.
 *
 * @ingroup errors
 */
typedef enum {

    /**
     * No error occurred; the operation was successfully completed.
     */
    RS232_ERR_NO_ERROR                       = 0,   /* VXI-11 B.5.2 */
    RS232_ERR_SYNTAX_ERROR                   = 1,   /* VXI-11 B.5.2 */
    RS232_ERR_INVALID_SYMBOLIC_NAME          = 2,
    RS232_ERR_DEVICE_NOT_ACCESSIBLE          = 3,   /* VXI-11 B.5.2 */
    RS232_ERR_INVALID_ID                     = 4,   /* VXI-11 B.5.2 */
    RS232_ERR_PARAMETER_ERROR                = 5,   /* VXI-11 B.5.2 */
    RS232_ERR_CHANNEL_NOT_ESTABLISHED        = 6,   /* VXI-11 B.5.2 */
    RS232_ERR_PERMISSION_DENIED              = 7,
    RS232_ERR_OPERATION_NOT_SUPPORTED        = 8,   /* VXI-11 B.5.2 */
    RS232_ERR_OUT_OF_RESOURCES               = 9,   /* VXI-11 B.5.2 */
    RS232_ERR_INTERFACE_INACTIVE             = 10,
    RS232_ERR_IO_TIMEOUT                     = 11,  /* VXI-11 B.5.2 */
    RS232_ERR_OVERFLOW                       = 16,
    RS232_ERR_IO_ERROR                       = 17,  /* VXI-11 B.5.2 */
    RS232_ERR_INTERRUPT                      = 129,
    RS232_ERR_UNKNOWN                        = 130,

}phComRs232Error_t;

/**
 * Serial interface control & end of input
 *
 * These values are used with @c phRs232Ctrl to change communication
 * parameters of an RS-232 interface
 *
 */
typedef enum {

    RS232_BAUD             = 1,
    RS232_PARITY           = 2,
    RS232_STOP_BITS        = 3,
    RS232_WIDTH            = 4,
    RS232_FLOW_CONTROL     = 5

} RS232_REQUEST;

/**
 * Reasons for returning from a read request.
 *
 * These values are returned by @c rs232Read() to indicate different termination
 * conditions.
 *
 */
typedef enum {

    RS232_REASON_REQCNT      = 1 << 0 /**< Requested number of bytes was read */,
    RS232_REASON_CHR         = 1 << 1 /**< Termination character encountered */,
    RS232_REASON_END         = 1 << 2 /**< END indicator encountered */

} RS232_REASON;

/**
 *  Wrapper for an RS-232 device
 *
 */
struct device
{
    /** Handle of device file. */
    int handle;
    /** Current setting of EOIS option for reading*/
    unsigned long readEOIs[MAX_EOIS_NUMBER];
    /** Current timeout. */
    unsigned long timeout;
    /** Identify interface type, offline or online */
    char card[64];
    /** selects the communication mode */
    phComMode_t mode;
    /** The data logger for errors/msgs */
    phLogId_t loggerId;
    /** total number of the EOIs*/
    int eoisNumber;
};

typedef struct device *rs232Device;

/*--- external variables ----------------------------------------------------*/

/*--- internal function -----------------------------------------------------*/



















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

#endif /* ! _PH_RS232_MHCOM_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/

