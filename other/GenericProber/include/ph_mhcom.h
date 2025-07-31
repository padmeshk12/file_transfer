/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_mhcom.h
 * CREATED  : 19 May 1999
 *
 * CONTENTS : Interface header for material handler communication layer
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 19 May 1999, Michael Vogt, created
 *            11 Oct 1999, Michael Vogt, added proposal for GPIB 
 *                interface connection
 *          : 18 Jan 2000, Jeff Morton, STE/ADC, added new GPIB function definitions
 *                based on Michael's proposal 
 *          : 23 Jan 2000, Jeff Morton, STE/ADC, changes to Event data types
 *          : 06 Feb 2000, Jeff Morton, STE/ADC,                             
 *          : 07 Feb 2000, Jeff Morton, STE/ADC,                             
 *          : 08 Feb 2000, Jeff Morton, STE/ADC,                             
 *          : 10 Feb 2000, Jeff Morton, STE/ADC,  got rid of PHCOM_ERR_GPIB_TIMEOUT                           
 *          : 16 Dec 2014, Xiaofei Han, add function phComGpibClear
 *          : 13 May 2015, Magco Li, add functions phComRs232Open(), phComRs232Close()
 *                                                phComRs232Send(), phComRs232Receive()
 *                                   add struct phComRs232Struct for open rs232 interface
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

#ifndef _PH_MHCOM_H_
#define _PH_MHCOM_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"
#include <vector>

/*--- defines ---------------------------------------------------------------*/

//#define PHCOM_MAX_MESSAGE_LENGTH 1024
#define PHCOM_MAX_MESSAGE_LENGTH 10240
#define SYMBOLIC_NAME_LENGTH 1024
#define MAX_EOIS_NUMBER 1024
#define MAX_TIMESTRING_LENGTH 1024
#define PARITY_TYPE_LENGTH 256
#define FLOWCONTROL_TYPE_LENGTH 256

/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

/* Handle for open communication sessions. This handle must be passed
   to each of the inteface functions of this module. It is created
   through a number of session open functions. */
typedef struct phComStruct *phComId_t;

typedef void(*AsyncSRQCallback_t)(unsigned char, void*);

/* Interface type definition */
#if 0
typedef enum {
    PHCOM_IFC_GPIB = 0                     /* gpib interface type */,
    PHCOM_IFC_LAN                          /* LAN(UDP/TCP) interface type */,
    PHCOM_IFC_LAN_SERVER                   /* LAN(TCP) server interface type */,
    PHCOM_IFC_RS232                        /* RS232 interface type */
} phComIfcType_t;
#endif
/* Enumeration type for the kind of communication */
typedef enum {
    PHCOM_ONLINE          /* normal calls */,
    PHCOM_OFFLINE         /* stub out bus activity */,
    PHCOM_SIMULATION      /* normal bus activity with delays surrounding ireadstb() needed with simulator */
} phComMode_t;

/* Enumeration type for communication errors or warnings */
typedef enum {
    PHCOM_ERR_OK = 0               /* no error */,
    PHCOM_ERR_UNKNOWN_DEVICE       /* interface device is unknown */,
    PHCOM_ERR_TIMEOUT              /* timed out */,
    PHCOM_ERR_GPIB_ICLOSE_FAIL     /* iclose()  failed   */,
    PHCOM_ERR_GPIB_ILOCK_FAIL      /* ilock()  failed    */,
    PHCOM_ERR_GPIB_IUNLOCK_FAIL    /* iunlock()  failed  */,
    PHCOM_ERR_TIME_FAIL            /* time.h gettimeofday() failed   */,
    PHCOM_ERR_MEMORY               /* couldn't get memory from heap with malloc() */,

    PHCOM_ERR_SYNTAX               /* syntac error */,
    PHCOM_ERR_VERSION              /* version error */,
    PHCOM_ERR_NO_CONNECTION        /* connection error */,
    PHCOM_ERR_NO_GPIB_SESSION      /* passed session ID does not belong to a GPIB session */,
    PHCOM_ERR_GPIB_BAD_ADDRESS     /* port address invalid */,
    PHCOM_ERR_GPIB_INVALID_INST    /* invalid INST */,
    PHCOM_ERR_GPIB_ICLEAR_FAIL     /* gpib iclear() failed */,
    PHCOM_ERR_GPIB_IONSRQ_FAIL     /* gpib ionsrq() failed */,
    PHCOM_ERR_GPIB_IONINTR_FAIL    /* gpib ionintr() failed */,
    PHCOM_ERR_GPIB_ISETINTR_FAIL   /* gpib isetintr() failed */,
    PHCOM_ERR_GPIB_ITERMCHR_FAIL   /* gpib itermchr() failed */,
    PHCOM_ERR_GPIB_ITIMEOUT_FAIL   /* gpib itimeout()  failed */,
    PHCOM_ERR_GPIB_IGPIBATNCTL_FAIL /* gpib igpibatnctl() failed */,
    PHCOM_ERR_GPIB_INCOMPLETE_SEND /* a message send over GPIB was sent incomplete */,

    PHCOM_ERR_NO_RS232_SESSION     /* passed session ID does not belong to a RS232 session */,
    PHCOM_ERR_RS232_BAD_ADDRESS    /* port address invalid */,
    PHCOM_ERR_RS232_ICTRL_FAIL     /* call rs232ctrl() failed */,
    PHCOM_ERR_RS232_ITIMEOUT_FAIL   /* set timeout  failed */,

    PHCOM_ERR_UNKNOWN              /*  */,
    PHCOM_ERR_NUM
} phComError_t;

/* Enumeration type for different line start-up conditions */
typedef enum {
    PHCOM_DELAY_NOW           /* delay immediately, don't return until
				 delay time is over */,
    PHCOM_DELAY_DEFERRED      /* schedule delay for given session but
				 return immediately */
} phComDelayMode_t;

typedef enum {
    PHCOM_GPIB_ET_SRQ                   /* an SRQ event */,
} phComGpibEventType_t;

/* type definition for GPIB events */
typedef struct gpibEvent {
    phComGpibEventType_t type;          /* the type of the event */
    union {
       unsigned char srq_byte;        /* master mode only, SRQ status byte */
    } d;
} phComGpibEvent_t;

typedef enum {
    PHCOM_GPIB_EOMA_NONE                /* do not perform any special action,
					   (default) */,
    PHCOM_GPIB_EOMA_TOGGLEATN           /* assert and release ATN line
                                           after any message was send */,
    PHCOM_GPIB_EOMA_SPOLL               /* perform a single serial
                                           poll after sending any
                                           message */
} phComGpibEomAction_t;

struct phComRs232Struct
{
    char device[SYMBOLIC_NAME_LENGTH];     /* the path of the rs232 device*/

    unsigned long baudRate;                //rs232 baudRate
    unsigned long dataBits;                //rs232 dataBIts
    unsigned long stopBits;                //rs232 stopBits
    char parity[PARITY_TYPE_LENGTH];       //rs232 parity
    char flowControl[FLOWCONTROL_TYPE_LENGTH]; //rs232 flow control
    unsigned long eois[MAX_EOIS_NUMBER];   //a set of long values which are used to recognize the end of input data
    int eoisNumber;                        //total number of the EOIs

    int gSimulatorInUse;                   //simulator mode: value = 1, others mode: value = 0

    struct timeval gLastSentTime;          //last send message time
    struct timeval gLastRecvTime;          //last receive message time
};

/*--- internal variables ----------------------------------------------------*/
static const char* GPIB_ERROR_DESCRIPTION[PHCOM_ERR_NUM] = {
    "No error.",
    "Interface device is unknown.",
    "The GPIB communication times out.",
    "Failed to close the GPIB interface.",
    "Failed to lock the interface.",
    "Failed to unlock the interface.",
    "Failed to get the system time.",
    "Failed to allocate memory.",

    "Syntax error in GPIB address.",
    "Version error.", /* has been obsolete or only be used in simulator */
    "Faile to Connect to the prober/handler.",
    "The session ID does not belong to a GPIB session", /* has been obsolete or only be used in simulator */
    "Invalid port address.",
    "Invalid instrument.",
    "Failed to reset the interface.",
    "Failed to set SRQ callbacks.",
    "Failed to set interruption callbacks.",
    "Failed to enable the interrupt events.",
    "Failed to define the termination character.",
    "Failed to set the length of timing out.",
    "Failed to control the state of the ATN signal on the bus.",
    "Incomplete message.",

    "The session ID does not belong to a rs232 session.",
    "Invalid port address in rs232 communication mode.",
    "Failed to set communication parameters to rs232 interface.",
    "Failed to set timeout in rs232 mode.",
    "Unexpected GPIB error."
};

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
 * General functions
 *
 *****************************************************************************/

/*****************************************************************************
 *
 * Delay communication for a given number of usec
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs a delay for the given interface session. A
 * session ID is required, since this
 * function may allow a defered delay, i.e. the function returns
 * immediately, but this module ensures, that the next action for the
 * given session ID is not performed before the delay is not
 * over. 
 * 
 * Note: delay mode PHCOM_DELAY_DEFERED is not yet
 * implemented. PHCOM_DELAY_NOW is used instead
 *
 * Returns: error code
 *
 ***************************************************************************/
phComError_t phComDelay(
    phComId_t session           /* The session ID of an open session. */,
    phComDelayMode_t mode       /* the delay mode */,
    long delayTime              /* [usec], delay time */
);

/*****************************************************************************
 *
 * GPIB session specific functions
 *
 *****************************************************************************/

/*****************************************************************************
 *
 * Open a GPIB communication link
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: A GPIB communication session is opened based on the
 * given parameters. 
 * 
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibOpen(
    phComId_t *newSession       /* The new session ID (result value). Will
				   be set to NULL, if an error occured */,
    phComMode_t mode            /* Commun mode: online|offline|simulation  */,
    const char *device          /* Symbolic Name of GPIB card in workstation*/,
    int port                    /* GPIB port to use */,
    phLogId_t logger            /* The data logger to use for error and
				   message log. Use NULL if data log is not 
				   used.                                    */,
    phComGpibEomAction_t act    /* To ensure that the EOI line is
                                   released after a message has been
                                   send, a end of message action may
                                   be defined here. The EOI line can
                                   not be released directly, only as a
                                   side affect of a serial poll or a
                                   toggle of the ATN line. */
);

/*****************************************************************************
 *
 * Reopen a GPIB communication link
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: An already opened GPIB session may be reopened in
 * cases where communication lockups occur. The underlaying 
 * interface session is closed and opened again. Doing this the device
 * ID and the port number may be changed. This may be very usefull
 * during exception handling to allow change of the predefined GPIB
 * communication on the fly without restarting the complete driver
 * environment. The provided session ID remains valid and does not
 * need to be redistributed throughout the complete driver framework.

 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibReopen(
    phComId_t session           /* session ID of an open GPIB session */,
    char *device                /* Symbolic Name of GPIB card in
                                   workstation. If set to NULL or empty
                                   string, the previous device name is
                                   used */,
    int port                    /* GPIB port to use */
);

/*****************************************************************************
 *
 * Close a GPIB communication link
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: This function closes an open GPIB session.
 * 
 * Returns: error code
 *
 ***************************************************************************/
phComError_t phComGpibClose(
    phComId_t session        /* session ID of an open GPIB session */
);

/*****************************************************************************
 *
 * Send a message over the GPIB bus
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: 
 *
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibSend(
    phComId_t session           /* session ID of an open GPIB session. */,
    char *h_string              /* message to send, may contain '\0'
				   characters */,
    int length                  /* length of the message to be sent,
                                   must be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message must be
                                   sent */
);

/*****************************************************************************
 *
 * Receive a message over the GPIB bus
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: 
 *
 * Based on the role of the current session, this function receives a
 * message from some other GPIB device. The message transfer must
 * start before timeout is reached. In case of timeout, an error code
 * is returned.
 * 
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibReceive(
    phComId_t session        /* session ID of an open GPIB session. */,
    const char **message        /* received message, may contain '\0'
				   characters. The calling function is 
				   not allowed to modify the passed 
				   message buffer */,
    int *length                 /* length of the received message,
                                   will be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message 
				   must be received */
);

/*****************************************************************************
 *
 * Clear the GPIB interface
 *
 * Authors: Xiaofei Han
 *
 * History: 16 Dec 2014, Xiaofei Han , created
 *
 * Description:
 *
 * Based on the role of the current session, this function clears the
 * GPIB interface. In case of timeout, an error code
 * is returned.
 *
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibClear(
    phComId_t session        /* session ID of an open GPIB session. */
);

/*****************************************************************************
 *
 * Get Next Event
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: 
 *
 * It is possible that events (like SRQs, etc....) occur on the GPIB bus while the mhcom layer is
 * currently not active. In this case the event is captured inside of
 * this module and saved into an event queue for later processing.
 *
 * This function queries the event queue for pending events. It
 * retrieves a pointer to the next event to handle from an internal
 * FIFO buffer. If no event is pending, a NULL pointer is returned.
 * 
 * Important: Any access to the event FIFO must be protected by
 * synchronization mechanisms to avoid concurrent changes caused by
 * incoming SRQs. The best thing to do so is to disable and reenable
 * interrupts whenever the FIFO is accessed.
 *
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibGetEvent(
   phComId_t session        /* session ID of an open GPIB session. */,
   phComGpibEvent_t **event    /* current pending event or NULL */,
   int *pending                /* number of additional pending events in 
				  the FIFO buffer */,
   long timeout                /* if set to value > 0 and if the event 
				  FIFO is empty, this defines the time
				  in usec to wait for an event before
				  returning with a NULL event */
);


/*****************************************************************************
 *
 * Check if any SRQ in the input SRQ list can be found in the FIFO. This function
 * doesn't remove any SRQs from the FIFO.
 *
 * Authors: Xiaofei Han
 *
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibCheckSRQs(
   phComId_t session        /* session ID of an open GPIB session. */,
   const int srqs[]         /* the collection of all SRQs to be checked */,
   int srqCount            /* total number of events in the array */,
   int *foundSRQ           /* regular SRQ value if the SRQ is found in the FIFO;
                               -1 if none of the SRQ is found*/
);

/*****************************************************************************
 *
 * Get Next Expected Event
 *
 * Authors: Xiaofei Han
 *
 * Description:
 * This function is derived from phComGpibGetEvent(). The only difference is
 * that an expected SRQ list is required. If the size of the array is zero,
 * the current SRQ in the FIFO will be returned. Otherwise, this function will
 * keep waiting for the one of the expected SRQ arrives. *
 *
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibGetExpectedEvents(
   phComId_t session        /* session ID of an open GPIB session. */,
   phComGpibEvent_t &event    /* current pending event or NULL */,
   std::vector<int> expected,  /* the expected SRQ list */
   long timeout                /* if set to value > 0 and if the event
                                  FIFO is empty, this defines the time
                                  in usec to wait for an event before
                                  returning with a NULL event */
);





/*****************************************************************************
 *
 * Send Event
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description:
 *
 * Returns: error code
 *
 ***************************************************************************/

phComError_t phComGpibSendEvent(
    phComId_t session          /* session ID of an open GPIB session */,
    phComGpibEvent_t *event     /* current pending event or NULL */,
    long timeout                /* time in usec until the transmission
				   (not the reception) of the event
				   must be completed */
);



/****************************************************************************
 * Register call back function for a specific SRQ.
 *
 * Author: Xiaofei Han
 *
 * Description:
 *
 * Allow the user to register an callback function which will be triggered
 * asynchronously when a specific srq arrives. 
 *
 ****************************************************************************/
phComError_t phComGpibRegisterSRQCallback(
    phComId_t session           /* session ID of an open GPIB session */, 
    unsigned char statusByte             /* the status byte which needs to be monitored */,
    AsyncSRQCallback_t callback /* the call back function pointer */,
    int keepStausByte           /* keep the status byte in the FIFO or just drop it*/,
    void* helperData            /* the helper data which may be required by the callback function*/
       
);


/*****************************************************************************
 *
 * Establish a LAN (either TCP or UDP) connection
 *
 * Authors: Xiaofei Han
 *
 * Description: The format of the lan symbolic name in the configuration file
 * is "lan/[tcp,udp]/ip_address/port_number".
 * For example: lan/tcp/10.150.1.1/9999
 * For TCP type, a socket will be created and a connection will be established.
 * For UDP type, only socket will be created.
 *
 ***************************************************************************/

phComError_t phComLanOpen(
    phComId_t *newSession       /* The new session ID (result value). Will
                                   be set to NULL, if an error occured */,
    phComMode_t mode            /* Commun mode: online|offline|simulation  */,
    const char *lanAddress      /* Symbolic Name of LAN address */,
    phLogId_t logger            /* The data logger to use for error and
                                   message log. Use NULL if data log is not
                                   used.                                    */
);

/*****************************************************************************
 *
 * Close a LAN communication link
 *
 * Authors: Xiaofei Han
 *
 * Description: This function closes the socket.
 *
 ***************************************************************************/
phComError_t phComLanClose(
    phComId_t session        /* session ID of an open LAN session */
);

/*****************************************************************************
 *
 * Send a message over the LAN
 *
 * Authors: Xiaofei Han
 *
 * Description:Send a message over the LAN.
 *
 ***************************************************************************/

phComError_t phComLanSend(
    phComId_t session           /* session ID of an open LAN session. */,
    char *h_string              /* message to send, may contain '\0'
                                   characters */,
    int length                  /* length of the message to be sent,
                                   must be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message must be
                                   sent */
);

/*****************************************************************************
 *
 * Receive a message over the LAN
 *
 * Authors: Xiaofei Han
 *
 * Description:Receive a message over the LAN.
 *
 ***************************************************************************/

phComError_t phComLanReceive(
    phComId_t session        /* session ID of an open LAN session. */,
    const char **message              /* received message, may contain '\0'
                                   characters. The calling function is
                                   not allowed to modify the passed
                                   message buffer */,
    int *length                 /* length of the received message,
                                   will be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message
                                   must be received */
);

/*****************************************************************************
 *
 * Receive a message over the LAN for Pyramid
 *
 * Authors: Zoyi Yu
 *
 * Description:Receive a message over the LAN.
 *
 ***************************************************************************/

phComError_t phComLanReceiveForPyramid(
    phComId_t session           /* session ID of an open LAN session. */,
    char **message              /* received message, may contain '\0'
                                   characters. The calling function is
                                   not allowed to modify the passed
                                   message buffer */,
    int *length                 /* length of the received message,
                                   will be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message
                                   must be received */
);

/*****************************************************************************
 *
 * Establish a RS232 connection
 *
 * Authors: Magco Li
 *
 * Description: The format of the rs232 symbolic name in the configuration file
 * is "rs232/dev/rs232 device".
 * For example: rs232/dev/usb/ttyUSB0
 *
 ***************************************************************************/

phComError_t phComRs232Open(
    phComId_t *newSession       /* The new session ID (result value). Will
                                   be set to NULL, if an error occured */,
    phComMode_t mode            /* Commun mode: online|offline|simulation  */,
    struct phComRs232Struct rs232Config /* rs232 parameters */,
    phLogId_t logger            /* The data logger to use for error and
                                   message log. Use NULL if data log is not
                                   used.                                    */
);

/*****************************************************************************
 *
 * Close a RS232 communication link
 *
 * Authors: Magco Li
 *
 * Description: This function closes the socket.
 *
 ***************************************************************************/
phComError_t phComRs232Close(
    phComId_t session        /* session ID of an open RS232 session */
);

/*****************************************************************************
 *
 * Send a message over the RS232
 *
 * Authors: Magco Li
 *
 * Description: Send a message over the RS232.
 *
 ***************************************************************************/

phComError_t phComRs232Send(
    phComId_t session           /* session ID of an open LAN session. */,
    char *h_string              /* message to send, may contain '\0'
                                   characters */,
    int length                  /* length of the message to be sent,
                                   must be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message must be
                                   sent */
);

/*****************************************************************************
 *
 * Receive a message over the RS232
 *
 * Authors: Magco Li
 *
 * Description: Receive a message over the RS232.
 *
 ***************************************************************************/

phComError_t phComRs232Receive(
    phComId_t session           /* session ID of an open RS232 session. */,
    const char **message              /* received message, may contain '\0'
                                   characters. The calling function is
                                   not allowed to modify the passed
                                   message buffer */,
    int *length                 /* length of the received message,
                                   will be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message
                                   must be received */
);

/*****************************************************************************
 *
 * Establish a LAN server
 *
 * Authors: Adam Huang
 *
 * Description: The format of the lan symbolic name in the configuration file
 * is "lan/tcp/ip_address/port_number".
 * For example: lan/tcp/10.150.1.1/9999
 * A tcp server will be created and listen the connection
 *
 ***************************************************************************/

phComError_t phComLanServerCreateSocket(
    phComId_t *newSession       /* The new session ID (result value). Will
                                   be set to NULL, if an error occured */,
    phComMode_t mode            /* Commun mode: online|offline|simulation  */,
    int serverPort              /* LAN server port */,
    phLogId_t logger            /* The data logger to use for error and
                                   message log. Use NULL if data log is not
                                   used.                                    */
);

/*****************************************************************************
 *
 * Wating for client connect
 *
 * Authors: Adam Huang
 *
 * 
 *
 ***************************************************************************/
phComError_t phComLanServerWaitingForConnection(phComId_t pmhcom, phLogId_t logger);


//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_MHCOM_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/

