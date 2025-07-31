/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : Semaphore.h
 * CREATED  : 26 Oct 1999
 *
 * CONTENTS : System V Semaphores
 *
 * AUTHORS  : Ulrich Frank, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Oct 1999, Ulrich Frank, created
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

#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SEMAPHORE_ERR_PERM = 1       /* Not super-user				*/,
    SEMAPHORE_ERR_NOENT          /* No such file or directory	        */,
    SEMAPHORE_ERR_SRCH           /* No such process			*/,
    SEMAPHORE_ERR_INTR           /* interrupted system call		*/,
    SEMAPHORE_ERR_IO             /* I/O error				*/,
    SEMAPHORE_ERR_NXIO           /* No such device or address	        */,
    SEMAPHORE_ERR_2BIG           /* Arg list too long			*/,
    SEMAPHORE_ERR_NOEXEC         /* Exec format error			*/,
    SEMAPHORE_ERR_BADF           /* Bad file number			*/,
    SEMAPHORE_ERR_CHILD          /* No children				*/,
    SEMAPHORE_ERR_AGAIN          /* No more processes			*/,
    SEMAPHORE_ERR_NOMEM          /* Not enough core			*/,
    SEMAPHORE_ERR_ACCES          /* Permission denied			*/,
    SEMAPHORE_ERR_FAULT          /* Bad address				*/,
    SEMAPHORE_ERR_BUSY           /* Mount device busy			*/,
    SEMAPHORE_ERR_EXIST          /* File exists				*/,
    SEMAPHORE_ERR_XDEV           /* Cross-device link			*/,	
    SEMAPHORE_ERR_NODEV          /* No such device				*/,
    SEMAPHORE_ERR_NOTDIR         /* Not a directory			*/,
    SEMAPHORE_ERR_ISDIR          /* Is a directory				*/,
    SEMAPHORE_ERR_INVAL          /* Invalid argument			*/,
    SEMAPHORE_ERR_NFILE          /* File table overflow			*/,
    SEMAPHORE_ERR_MFILE          /* Too many open files			*/,
    SEMAPHORE_ERR_NOTTY          /* Not a typewriter			*/,
    SEMAPHORE_ERR_FBIG           /* File too large				*/,
    SEMAPHORE_ERR_NOSPC          /* No space left on device		*/,
    SEMAPHORE_ERR_SPIPE          /* Illegal seek				*/,
    SEMAPHORE_ERR_ROFS           /* Read only file system		        */,
    SEMAPHORE_ERR_MLINK          /* Too many links				*/,
    SEMAPHORE_ERR_PIPE           /* Broken pipe			      	*/,
    SEMAPHORE_ERR_DEADLK         /* A deadlock would occur		        */,
    SEMAPHORE_ERR_NOLCK          /* System record lock table was full      */,
    SEMAPHORE_ERR_ILSEQ          /* Illegal byte sequence		        */,
    SEMAPHORE_ERR_NOMSG = 35     /* No message of desired type             */,

    SEMAPHORE_ERR_NLOCKED        /* Semaphore not locked			*/,
    SEMAPHORE_ERR_VNALLOWED      /* Value is not allowed			*/,
    
    SEMAPHORE_ERR_OK             /* no error			      	*/,
    SEMAPHORE_ERR_UNKNOWN        /* unknown error				*/,

} SemaphoreError_t;

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
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * Returns: error code
 *
 ***************************************************************************/

SemaphoreError_t sem_open(
    int *sid,
    key_t key
    );

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * Returns: error code
 *
 ***************************************************************************/

SemaphoreError_t sem_create(
    int *sid,
    key_t key,
    ushort members
    );

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * Returns: error code
 *
 ***************************************************************************/

SemaphoreError_t sem_lock(
    int sid,
    ushort member
    );

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * Returns: error code
 *
 ***************************************************************************/

SemaphoreError_t sem_unlock(
    int sid,
    ushort member
    );

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * Returns: error code
 *
 ***************************************************************************/

SemaphoreError_t sem_remove(
    int sid
    );

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * Returns: error code
 *
 ***************************************************************************/

/* CR36857: 
 * Removed the following function, because it has the same name with the system
 * standard semophore processing function sem_wait and this may lead to segmentation 
 * violation under certain situation.

SemaphoreError_t sem_wait(
    int id
    );

*/

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * Returns: error code
 *
 ***************************************************************************/

SemaphoreError_t sem_signal(
    int id
    );

#ifdef __cplusplus
}
#endif

#endif /* ! _SEMAPHORE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
