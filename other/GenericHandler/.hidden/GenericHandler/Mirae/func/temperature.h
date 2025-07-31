/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : temperature.h
 * CREATED  : 29 Jun 2000
 *
 * CONTENTS : Handling of temperature configurations
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Ken Ward, Bitsoft Systems, Inc, Mirae revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Jun 2000, Michael Vogt, created
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

#ifndef _TEMPERATURE_H_
#define _TEMPERATURE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"
#include "ph_conf.h"

/*--- defines ---------------------------------------------------------------*/

#define PHFUNC_TEMP_MAX_CHAMBERS 32

/*--- typedefs --------------------------------------------------------------*/

enum tempControl {
    PHFUNC_TEMP_STOP,          /* Mirae only - temp setup not changed by the driver */
    PHFUNC_TEMP_HOT,           /* Mirae only - temp setup activated by driver */
    PHFUNC_TEMP_AMBIENT,       /* set to ambient by driver */
    PHFUNC_TEMP_CCA            /* Mirae only - temp setup activated by driver */
                               /* CCA = Cold and Controlled Ambient */
};


struct tempSetup
{
    enum tempControl          mode;
    int                       chambers;   /* the number of controlled
                                             areas, this number
                                             defines the size of the
                                             following arrays */

    char                      *name[PHFUNC_TEMP_MAX_CHAMBERS];     
    /* the names of these areas */
    double                    setpoint[PHFUNC_TEMP_MAX_CHAMBERS];  
    /* set temp point */
    double                    soaktime[PHFUNC_TEMP_MAX_CHAMBERS];  
    /* soaktime in seconds, inactive if set to 0, -1 if not defined */
    double                    desoaktime[PHFUNC_TEMP_MAX_CHAMBERS];
    /* desoaktime in seconds, inactive if set to 0, -1 if not defined */
    double                    uguard[PHFUNC_TEMP_MAX_CHAMBERS];   
    /* upper temp guard, inactive if set to 0, -1 if not defined */
    double                    lguard[PHFUNC_TEMP_MAX_CHAMBERS];    
    /* lower temp guard, inactive if set to 0, -1 if not defined */

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
 * Get temperature defintions from configuration
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * Retrieve all temperature parameters from the given
 * configuration and fill up a data struct. Temperature parameters are
 * converted to kelvin by default.
 *
 * Returns: 1 if OK, 0 if the configuration was corrupt
 *
 ***************************************************************************/

int phFuncTempGetConf(
    phConfId_t conf           /* configuration to read values from */,
    phLogId_t logger          /* report messages here */,
    struct tempSetup *data    /* data struct to be filled by this function */
);

#endif /* ! _TEMPERATURE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
