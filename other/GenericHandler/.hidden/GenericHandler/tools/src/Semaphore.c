/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : Semaphore.c
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
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*--- module includes -------------------------------------------------------*/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/*
#include "tpi_c.h"
#include "ci_types.h"
#include "libcicpi.h"
*/

#include "Semaphore.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define SEM_RESOURCE_MAX        1       /* Initial value of all semaphores */

/*--- typedefs --------------------------------------------------------------*/

typedef union {
	int val;
	struct semid_ds *buf;
	ushort *array;
} semun;

/*--- function prototypes ---------------------------------------------------*/

SemaphoreError_t getval(int sid, ushort member, int *semval);
void dispval(int sid, ushort member);

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

SemaphoreError_t sem_open(
	int *sid,
	key_t key
)
{
    /* Open the semaphore set - do not create! */
    
    if((*sid = semget(key, 0, 0666)) == -1) {
	if (errno >= SEMAPHORE_ERR_PERM && errno <= SEMAPHORE_ERR_ILSEQ)
	    return (SemaphoreError_t)errno;
	return SEMAPHORE_ERR_UNKNOWN;
    }
    return SEMAPHORE_ERR_OK;
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

SemaphoreError_t sem_create(
    int *sid,
    key_t key,
    ushort members
)
{
    int cntr;
    semun semopts;
    
#ifdef DEBUG
    printf("Attempting to create new semaphore set with %d members\n", members);
#endif

    if((*sid = semget(key, members, IPC_CREAT|IPC_EXCL|0666)) == -1) {
	if (errno >= SEMAPHORE_ERR_PERM && errno <= SEMAPHORE_ERR_ILSEQ)
	    return (SemaphoreError_t)errno;
	return SEMAPHORE_ERR_UNKNOWN;
    }
    
    semopts.val = SEM_RESOURCE_MAX;
    
    /* Initialize all members (could be done with SETALL) */        
    for (cntr=0; cntr<members; cntr++) {
	if (semctl(*sid, cntr, SETVAL, semopts) == -1) {
	    if (errno >= SEMAPHORE_ERR_PERM && errno <= SEMAPHORE_ERR_ILSEQ)
		return (SemaphoreError_t)errno;
	    return SEMAPHORE_ERR_UNKNOWN;
	}
    }

    return SEMAPHORE_ERR_OK;
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

SemaphoreError_t sem_lock(
    int sid,
    ushort member
)
{
    struct sembuf seml[1];
    int semval;
    SemaphoreError_t result;
    

    seml[0].sem_num = 0;
    seml[0].sem_op  = -1;
    seml[0].sem_flg = 0; /* = IPC_NOWAIT; */

    /* Attempt to lock the semaphore set */
    if((result = getval(sid, member, &semval)) != SEMAPHORE_ERR_OK) {

#ifdef DEBUG
	printf("Semaphore resources exhausted (no lock)!\n");
#endif

        return result;
    }
    
    seml[0].sem_num = member;
    
    if((semop(sid, seml, 1)) == -1) {
	if (errno >= SEMAPHORE_ERR_PERM && errno <= SEMAPHORE_ERR_ILSEQ) {
	    return (SemaphoreError_t)errno;
	}
	return SEMAPHORE_ERR_UNKNOWN;
    }
    else {

#ifdef DEBUG
	printf("Semaphore resources decremented by one (locked)\n");
#endif

    }
    
#ifdef DEBUG
    dispval(sid, member);
#endif
    
    return SEMAPHORE_ERR_OK;
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

SemaphoreError_t sem_unlock(
    int sid,
    ushort member
    )
{
    struct sembuf semul[1]; 
    int semval;
    SemaphoreError_t result = SEMAPHORE_ERR_OK;

    semul[0].sem_num = member;
    semul[0].sem_op  = 1;
    semul[0].sem_flg = 0; /*IPC_NOWAIT;*/
    
    /* Is the semaphore set locked? */
    if ((result = getval(sid, member, &semval)) != SEMAPHORE_ERR_OK) {
	return result;
    }
    
    if(semval == SEM_RESOURCE_MAX) {
#ifdef DEBUG
	printf("Semaphore not locked!\n");
#endif

	return SEMAPHORE_ERR_NLOCKED;
    }
    
    semul[0].sem_num = member;
    
    /* Attempt to lock the semaphore set */
    if((semop(sid, semul, 1)) == -1) {
	if (errno >= SEMAPHORE_ERR_PERM && errno <= SEMAPHORE_ERR_ILSEQ) {
	    return (SemaphoreError_t)errno;
	}
	return SEMAPHORE_ERR_UNKNOWN;
    }
    else {
#ifdef DEBUG
	printf("Semaphore resources incremented by one (unlocked)\n");
#endif

    }
    
#ifdef DEBUG
    dispval(sid, member);
#endif

    
    return SEMAPHORE_ERR_OK;    
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

SemaphoreError_t sem_remove(
    int sid
)
{
    if (semctl(sid, 0, IPC_RMID, 0) == -1) {
	if (errno >= SEMAPHORE_ERR_PERM && errno <= SEMAPHORE_ERR_ILSEQ)
	    return (SemaphoreError_t)errno;
		return SEMAPHORE_ERR_UNKNOWN;
    }
    
#ifdef DEBUG
    printf("Semaphore removed\n");
#endif
    
    return SEMAPHORE_ERR_OK;    
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

/* CR36857:
 * Removed the following function, because it has the same name with the system
 * standard semophore processing function sem_wait and this may lead to segmentation
 * violation unders certain situation.

SemaphoreError_t sem_wait(
	int id
)
{
	return sem_lock(id, 0);
}

*/

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

SemaphoreError_t sem_signal(
	int id
)
{
	return sem_unlock(id, 0);
}


/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

SemaphoreError_t getval(
	int sid,
	ushort member,
	int *semval
)
{
	if ((*semval = semctl(sid, member, GETVAL, 0)) == -1) {
		if (errno >= SEMAPHORE_ERR_PERM && errno <= SEMAPHORE_ERR_ILSEQ)
			return (SemaphoreError_t)errno;
		return SEMAPHORE_ERR_UNKNOWN;
	}
	return(SEMAPHORE_ERR_OK);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

void dispval(
	int sid,
	ushort member
)
{
	int semval;

	semval = semctl(sid, member, GETVAL, 0);
    printf("semval for member %d is %d\n", member, semval);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
