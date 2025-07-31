/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : binner.h
 * CREATED  : 23 Jul 1999
 *
 * CONTENTS : Bin Management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 23 Jul 1999, Michael Vogt, created
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

#ifndef _BINNER_H_
#define _BINNER_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

typedef struct binningMap *phBinMapId_t;
typedef struct binningActMap *phBinActMapId_t;

typedef enum {
    PHBIN_MODE_DEFAULT,
    PHBIN_MODE_HARDMAP,
    PHBIN_MODE_SOFTMAP, 
    PHBIN_MODE_DEFAULT_SOFTBIN
} phBinMode_t;

typedef enum {
    PHBIN_ACTMODE_HARDBINS = 0,
    PHBIN_ACTMODE_SOFTBINS
} phBinActMode_t;


/* list of special actions to take place for certain bin codes. This
   list may be expanded in the future. Currently it is prepared to
   hold flags for automatic probe needle cleaning and automatic probe
   to pad optimization. Besides the regular bin map an independent map
   is used to store these actions. The entries are binary bit fields. */

typedef enum {
    PHBIN_ACT_NONE      = 0x00000000    /* empty */,
    PHBIN_ACT_CLEAN     = 0x00000001    /* needle cleaning */,
    PHBIN_ACT_PPOPT     = 0x00000002    /* probe to pad optimization */,
    
    PHBIN_ACT_LAST      = 0x00000004    /* last entry, value must
                                             be adapted, if new
                                             actions are added */
} phBinActType_t;

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
 * Reset/Initialize the bin mapping feature
 *
 * Authors: Michael Vogt
 *
 * Description: Prepares entering of a new smartest to handler
 * mapping, frees an existing mapping
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int resetBinMapping(phBinMapId_t *handle, int numberOfHandlerBins);

/*****************************************************************************
 *
 * Reset/Initialize the bin mapping feature
 *
 * Authors: Michael Vogt
 *
 * Description: Prepares entering of a new smartest bin to action
 * mapping, frees an existing mapping
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int resetBinActMapping(phBinActMapId_t *handle);

/*****************************************************************************
 *
 * Enter one entry into the map
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * If the <stCode> has not already got an entry, a new binMapEntry is
 * allocated and a new stBinSorter entry is allocated, based on the
 * <numberOf HandlerBins> given during initialization. The new
 * stBinSorter is initialized to { 0 , 0 , ... }. The new binMapEntry
 * is initialized to { <stCode>, ..., ... } and put to the top of the
 * internal list.
 *
 * If the entry exists, <handlerBinIndex> is entered at the <used>
 * position and <used> is incremented, checking that used doesn't hit
 * the <numberOf HandlerBins> border before doing this.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int enterBinMap(phBinMapId_t handle, long long stCode, long handlerBinIndex);

/*****************************************************************************
 *
 * Enter one entry into the map
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * The given <action> is added to the given bin Code <stCode>
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int enterBinActMap(phBinActMapId_t handle, long long stCode, phBinActType_t action);

/*****************************************************************************
 *
 * Map one SmarTest bin to one handler bin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The <stCode> is searched in the binMapEntry List and put to tht top
 * of the list. In the associated stBinSorter, the entry at <nextUse>
 * is retrieved and returned in <handlerBinIndex>. <nextUse> is
 * incremented or reset to 0.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int mapBin(phBinMapId_t handle, long long stCode, long *handlerBinIndex);

/*****************************************************************************
 *
 * Map one SmarTest bin to an action combination
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The <stCode> is searched in the bin action list and returned.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int mapAct(phBinActMapId_t handle, long long stCode, phBinActType_t *action);

/*Begin of Huatek Modifications, Donnie Tu, 04/23/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free the bin mapping feature
 *
 * Authors: Donnie Tu
 *
 * Description: frees an existing mapping
 *
 * Returns: none
 *
 ***************************************************************************/
void freeBinMapping( phBinMapId_t *handle);
/*End of Huatek Modifications*/


/*Begin of kaw Modifications 11/15/2004*/
/*Issue Number: 15686*/
/*****************************************************************************
 *
 * Free the bin Act mapping feature
 *
 * Authors: Ken Ward
 *
 * Description: frees act map 
 *
 * Returns: none
 *
 ***************************************************************************/
 
void freeActBinMapping( phBinActMapId_t *handle);
/*End of kaw Modifications*/

#endif /* ! _BINNER_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
