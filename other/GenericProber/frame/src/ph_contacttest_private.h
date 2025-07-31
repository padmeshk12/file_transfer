/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_contacttest_private.h
 * CREATED  : 22 August 2006
 *
 * CONTENTS : contact test actions
 *
 * AUTHORS  : EDO, R&D Shangai
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 16 Oct 2005, 
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
#ifndef _PH_CONTACTTEST_PRIVATE_H_
#define _PH_CONTACTTEST_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

#define CONTACT_TEST_STEP                    "step"
#define Z_SEARCH_ALGORITHM                   "z_search_algorithm"
#define CONTACT_POINT_SELECTION              "contact_point_selection"
#define Z_LIMIT                              "z_limit"
#define SAFETY_WINDOW                        "safety_window"
#define OVERDRIVE                            "overdrive"
#define DIE_COORDINATES                      "die_coordinates"
#define SETTLING_TIME                        "settling_time"
#define DIGITAL_PIN_GROUP                    "digital_pin_group"
#define FORCE_CURRENT_OF_DIGITAL_PIN_GROUP   "force_current_of_digital_pin_group"
#define LIMIT_OF_DIGITAL_PIN_GROUP           "limit_of_digital_pin_group"
#define ANALOG_PIN_GROUP                     "analog_pin_group"
#define FORCE_CURRENT_OF_ANALOG_PIN_GROUP    "force_current_of_analog_pin_group"
#define LIMIT_OF_ANALOG_PIN_GROUP            "limit_of_analog_pin_group"
  
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
 * To fill the documentation with life, please fill  in the angle
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
 * perform contact test
 *
 * Authors: Fabrizio Arca & Kun Xiao
 *
 * Description:
 *
 * Initialize contact test setup structure, and perform 
 * contact test.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameContacttest(
                                     struct phFrameStruct *f             /* the framework data */,
                                     char *FileNameString                /* contact test configuration file path name */
                                     );

#endif /* _PH_CONTACTTEST_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
