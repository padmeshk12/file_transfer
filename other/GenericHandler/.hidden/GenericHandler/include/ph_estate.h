/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_estate.h
 * CREATED  : 12 Nov 1999
 *
 * CONTENTS : Equipment Specific State Control
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 12 Nov 1999, Michael Vogt, created
 *            29 May 2000, Michael Vogt, adapted for handler framework
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

#ifndef _PH_ESTATE_H_
#define _PH_ESTATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"

/*--- defines ---------------------------------------------------------------*/

/* this is global to all handler driver modules.... */
#define PHESTATE_MAX_SITES 1024

/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

typedef struct phEstateStruct *phEstateId_t;

typedef enum {
    PHESTATE_ERR_OK = 0                 /* no error */,
    PHESTATE_ERR_NOT_INIT               /* the state controller was not
				  	   initialized successfully */,
    PHESTATE_ERR_INVALID_HDL            /* the passed ID is not valid */,
    PHESTATE_ERR_MEMORY                 /* couldn't get memory from heap
					   with malloc() */,
    PHESTATE_ERR_SITE                   /* the passed site definition
					   has errors or exceeds the maximum 
					   number of supported sites */
} phEstateError_t;

typedef enum {
    PHESTATE_SITE_POPULATED = 0         /* a die is probed at this site */,
    PHESTATE_SITE_EMPTY,                /* the site is active, but
					   currently no die is probed
					   (possible reason: outside
					   of defined wafer map) */
    PHESTATE_SITE_DEACTIVATED           /* the site is temporarily
					   deactivated and no die is
					   probed */,
    PHESTATE_SITE_POPDEACT              /* a die is probed at this site,
					   but the site will be set
					   DEACTIVATED as soon as the
					   die is binned */
} phEstateSiteUsage_t;


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
 * Equipment specific state controller initialization
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function initializaes the equipment specific state
 * controller. It must be called before any subsequent call to this
 * module. The ID of the state cotroller is returned in <estateID> and
 * must be passed to any other call into this module.
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateInit(
    phEstateId_t *estateID              /* the resulting equipment state ID */,
    phLogId_t loggerID                  /* logger to be used */
);

/*****************************************************************************
 *
 * Destroy an equipment specific state controller
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function destroys an equipment specific state controller. The
 * estateID will become invalid.
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateDestroy(
    phEstateId_t estateID               /* the state ID */
);

/*****************************************************************************
 *
 * Get site population information
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function returns an array, which describes the current site
 * population as reported by the handler according to the unified
 * handler model. The length of the array (the number of sites) depends
 * on the handler. It is also returned by this function.  The valid
 * index range of the population array ranges from 0 to
 * <entries>-1. The index positions are mapped to the handler site
 * naming conventions as defined in the configuration. The
 * configuration always contains an ordered list of handler site names.
 *
 * For user convenience, error messages and other messages sent to the
 * user should always map the site array index to the defined handler
 * site names. The array index will only be used within the driver
 * framework for efficient handler site to SmarTest site mapping.
 *
 * Note: This is function refers to a unified handler model
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateAGetSiteInfo(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateSiteUsage_t **populated     /* pointer to array of populated
					   sites */,
    int *entries                        /* size of the array
					   (max. number of sites) */
);

/*****************************************************************************
 *
 * Set site population information
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is used to store the current site population and to
 * keep it stored in a common place throughout the whole driver
 * framework. If the function phPFuncGetDie(3) from the driver plugin
 * is called successfully, the driver plugin will set the found driver
 * population using this function. In cases of error recovery,
 * timeouts, etc. event functions or hook functions may also use this
 * function to notify the driver framework over the valid site
 * population.
 *
 * Note: This is function refers to a unified handler model
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateASetSiteInfo(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateSiteUsage_t *populated      /* pointer to array of populated
					   sites */,
    int entries                         /* entries of the array
					   (max. number of sites) */
);

/*****************************************************************************
 *
 * Get handler pause information
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is used to retrieve the current handler pause state.
 *
 * Note: This is function refers to a unified handler model
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateAGetPauseInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int *paused                         /* set, if handler is paused */
);

/*****************************************************************************
 *
 * Set handler pause information
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is used to store the current handler pause state and
 * keep it stored in a common place throughout the whole driver
 * framework.
 *
 * Note: This is function refers to a unified handler model
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateASetPauseInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int paused                          /* set, if handler is paused */
);

/*****************************************************************************
 *
 * Get handler lot information
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is used to retrieve the current handler lot state.
 *
 * Note: This is function refers to a unified handler model
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateAGetLotInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int *lotStarted                     /* set, if lot is already started */
);

/*****************************************************************************
 *
 * Set handler lot information
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function is used to store the current handler lot state and
 * keep it stored in a common place throughout the whole driver
 * framework.
 *
 * Note: This is function refers to a unified handler model
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateASetLotInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int lotStarted                      /* set, if handler is lotd */
);

/*Begin of Huatek Modifications, Donnie Tu, 04/23/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free Equipment specific state controller 
 *
 * Authors: Donnie Tu
 *
 * Description:
 *
 * This function frees the equipment specific state
 * controller. 
 *
 * Returns: none
 *
 ***************************************************************************/

void phEstateFree(phEstateId_t *estateID);
/*End of Huatek Modifications*/

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_ESTATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
