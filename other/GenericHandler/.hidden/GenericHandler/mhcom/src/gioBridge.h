/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : gioBridge.h
 * CREATED  : 30 Aug 2000
 *
 * CONTENTS : GIO call layer
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 30 Aug 2000, Michael Vogt, created
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

#ifndef _GIOBRIDGE_H_
#define _GIOBRIDGE_H_

/*--- system includes -------------------------------------------------------*/
#include    <stdint.h>
#include    "ph_mhcom.h"
#include    "compile_compatible.h"

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/
typedef enum {
    PHCOM_IFC_GPIB = 0                  /* gpib interface type */,
    PHCOM_IFC_LAN                       /* LAN(UDP/TCP) interface type */,
    PHCOM_IFC_LAN_SERVER                /* LAN(TCP) server interface type */,
    PHCOM_IFC_RS232                     /* RS232 interface type */
} phComIfcType_t;

typedef struct phSiclInst *phSiclInst_t;
typedef void (*phComSRQHandler_t)(phSiclInst_t, void *);

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

/*****************************************************************************/
/* FUNCTION: phComIOpen()                                                    */
/* DESCRIPTION:                                                              */
/*            iopen gio/simulator                                           */
/* Note: Interface differs from original GIO function                       */
/*****************************************************************************/
phSiclInst_t phComIOpen(
    const char *addr          /* the name of the interface */,
    phComMode_t mode          /* on/off line, simulation, etc. */,
    phComIfcType_t intfc      /* GPIB */,
    phLogId_t loggerId        /* here we log messages and errors */
);

/*****************************************************************************/
/* FUNCTION: phComIClose()                                                   */
/* DESCRIPTION:                                                              */
/*            iclose gio/simulator                                          */
/*****************************************************************************/
phComError_t phComIClose(
    phSiclInst_t session      /* The session ID of an open session. */
);

/*****************************************************************************/
/* FUNCTION: phComIClear()                                                   */
/* DESCRIPTION:                                                              */
/*            iclear gio/simulator                                          */
/*****************************************************************************/
phComError_t phComIClear(
    phSiclInst_t session      /* The session ID of an open session. */
);

/*****************************************************************************/
/* FUNCTION: phComILock()                                                    */
/* DESCRIPTION:                                                              */
/*            ilock gio/simulator call                                      */
/*****************************************************************************/
phComError_t phComILock(
    phSiclInst_t session      /* The session ID of an open session. */
);

/*****************************************************************************/
/* FUNCTION: phComIUnlock()                                                  */
/* DESCRIPTION:                                                              */
/*            iunlock gio/simulator call                                    */
/*****************************************************************************/
phComError_t phComIUnlock(
    phSiclInst_t session      /* The session ID of an open session. */
);

/*****************************************************************************/
/* FUNCTION: phComIIntrOn()                                                  */
/* DESCRIPTION:                                                              */
/*            iintron gio/simulator call                                    */
/* Note: Interface differs from original GIO function                       */
/*****************************************************************************/
phComError_t phComIIntrOn(
    phComMode_t mode          /* the communication mode */
);

/*****************************************************************************/
/* FUNCTION: phComIIntrOff()                                                 */
/* DESCRIPTION:                                                              */
/*            iintroff gio/simulator call                                   */
/* Note: Interface differs from original GIO function                       */
/*****************************************************************************/
phComError_t phComIIntrOff(
    phComMode_t mode          /* the communication mode */
);

/*****************************************************************************/
/* FUNCTION: phComIOnSRQ()                                                   */
/* DESCRIPTION:                                                              */
/*            ionsrq gio/simulator call                                     */
/* Note: Interface differs from original GIO function                       */
/*****************************************************************************/
phComError_t phComIOnSRQ(
    phSiclInst_t session      /* The session ID of an open session. */,
    phComSRQHandler_t handler /* the SRQ handler function */,
    void *ptr                 /* customizable pointer to some
                                 arbitrary memory location, will be
                                 delivered during SRQ to the handler
                                 function as last argument */
);

/*****************************************************************************/
/* FUNCTION: phComITimeout()                                                 */
/* DESCRIPTION:                                                              */
/*            itimeout gio/simulator call                                   */
/*****************************************************************************/
phComError_t phComITimeout(
    phSiclInst_t session      /* The session ID of an open session. */,
    long tval                 /* timout in milli seconds */
);


/*****************************************************************************/
/* FUNCTION: phComIWaitHdlr()                                                */
/* DESCRIPTION:                                                              */
/*            iwaithdlr gio/simulator call                                  */
/* Note: Interface differs from original GIO function                       */
/*****************************************************************************/
phComError_t phComIWaitHdlr(
    phSiclInst_t session      /* The session ID of an open session. */,
    long tval                 /* timout in milli seconds */
);


/*****************************************************************************/
/* FUNCTION: phComITermChr()                                                 */
/* DESCRIPTION:                                                              */
/*            itermchr gio/simulator call                                   */
/*****************************************************************************/
phComError_t phComITermChr(
    phSiclInst_t session      /* The session ID of an open session. */,
    int tchr                  /* termination character */
);

/*****************************************************************************/
/* FUNCTION: phComIWrite()                                                   */
/* DESCRIPTION:                                                              */
/*            iwrite gio/simulator call                                     */
/*****************************************************************************/
phComError_t phComIWrite(
    phSiclInst_t session      /* The session ID of an open session. */,
    char *buf                 /* buffer to write from */,
    unsigned long datalen     /* number of bytes to write */,
    int endi                  /* send end indicator, if set */,
    unsigned long *actual     /* number of written bytes */
);

/*****************************************************************************/
/* FUNCTION: phComIRead()                                                    */
/* DESCRIPTION:                                                              */
/*            iread gio/simulator call                                      */
/*****************************************************************************/
phComError_t phComIRead(
    phSiclInst_t session      /* The session ID of an open session. */,
    char *buf                 /* buffer to read to */,
    unsigned long bufsize     /* size of buffer */,
    int *reason               /* returned reason for end of reading */,
    unsigned long *actual     /* returned number of read bytes */
);

/*****************************************************************************/
/* FUNCTION: phComISetStb()                                                  */
/* DESCRIPTION:                                                              */
/*            isetstb gio/simulator                                         */
/*****************************************************************************/
phComError_t phComISetStb(
    phSiclInst_t session        /* The session ID of an open GPIB session. */,
    unsigned char stb           /* status byte to set */
);

/*****************************************************************************/
/* FUNCTION: phComIReadStb()                                                 */
/* DESCRIPTION:                                                              */
/*            ireadstb gio/simulator                                        */
/*****************************************************************************/
phComError_t phComIReadStb(
    phSiclInst_t session        /* The session ID of an open GPIB session. */,
    unsigned char *stb          /* status byte to be returned */
);

/*****************************************************************************/
/* FUNCTION: phComIgpibAtnCtl()                                              */
/* DESCRIPTION:                                                              */
/*            igpibatnctl gio/simulator                                     */
/* Note: Interface differs from original GIO function                       */
/*****************************************************************************/
phComError_t phComIgpibAtnCtl(
    phSiclInst_t session        /* The session ID of an open GPIB session. */,
    int atnval                  /* assert ATN line, if set                 */
);

#endif /* ! _GIOBRIDGE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
