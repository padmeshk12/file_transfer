/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_tools.h
 * CREATED  : 29 Jun 1999
 *
 * CONTENTS : Common base layer tools to be used by all components
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, add "phBinSearchStrValueByStrKey"
 *            Garry Xie, R&D Shanghai, add "phReplaceCharsInStrWithACertainChar"
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Jun 1999, Michael Vogt, created
 *            August 2005, Jiawei Lin, add "phBinSearchStrValueByStrKey"
 *              and "phStringPair_t" data structure
 *            Feburary 2006, Garry Xie, add "phReplaceCharsInStrWithACertainChar"
 *            December 2005, fabarca, #include "butil.h" to use BULinkedList
 *            Oct 2008, Jiawei Lin, CR42750
 *               add the function "phToolsGetTimeStampString"
 *
 *            20 Mar 2009, Roger Feng, add functoin "phTrimLeadingTrailingDelimiter".
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

#ifndef _PH_TOOLS_H_
#define _PH_TOOLS_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

/* allocate a new structure from heap memory, applying the correct type */
#define PhNew(x) (x *) malloc(sizeof(x))

/* allocate a new array of structures from heap memory, applying the
   correct type */
#define PhNNew(x, n) (x *) malloc((n) * sizeof(x))


#define SUCCEED 0
#define FAIL    1

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

#define ON      1
#define OFF     0
#define YES     1
#define NO      0
#define EOS     '\0'

#define BIN_CODE_LENGTH 8

/*--- typedefs --------------------------------------------------------------*/
typedef struct {
  const char *key;
  const char *value;
}phStringPair_t;

/*--- arithmetic ------------------------------------------------------------*/

/*
        MAX(a,b)        Evaluates maximum of <a> or <b>
        MIN(a,b)        Evaluates minimum of <a> or <b>
        ABS(a)          Evaluates <a> as absolute value

        Round(v,r)      Rounds the integer <v>alue to <r>esolution

*/

#                       ifndef  MAX
#define MAX(a,b)        ((a) > (b) ? (a) : (b))
#                       endif
#                       ifndef  MIN
#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#                       endif
#                       ifndef  ABS
#define ABS(a)          ((a) > 0 ? (a) : -(a))
#                       endif
#                       ifndef  Round
#define Round(v,r)      (((v)<0 ? ((v)-(r)/2) : ((v)+(r)/2)) / (r))
#                       endif

/* the 'array' must be real array, should not be pointer */
#ifndef LENGTH_OF_ARRAY
#define LENGTH_OF_ARRAY(array) (sizeof (array) / sizeof ((array)[0]))
#endif

#ifndef END_OF_ARRAY
#define END_OF_ARRAY(array, len) (&array[len])
#endif

#define MAX_PH_SUBSTR_REPLACE_STRING_LEN 5000

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
 * Match a string
 *
 * Authors: Michael Vogt
 *
 * Description: Match a string against an extended regular expression
 * (see man regexp(5))
 *
 * Examples:
 *
 * phToolsMatch("TEL P8",  "TEL *P-?8")  returns 1
 *
 * phToolsMatch("TEL P-8", "TEL *P-?8")  returns 1
 *
 * phToolsMatch("TELP8",   "TEL *P-?8")  returns 1
 *
 * phToolsMatch("TELP-8",  "TEL *P-?8")  returns 1
 *
 * phToolsMatch("P8",      "TEL *P-?8")  returns 0
 *
 * phToolsMatch("tel P8",  "TEL *P-?8")  returns 0
 *
 * phToolsMatch("Tel P8",  "[Tt][Ee][Ll] *P-?8")  returns 1
 *
 * Returns:
 * 1 if matched, 0 else
 *
 ***************************************************************************/
int phToolsMatch(
    const char *string              /* the string to be matched */,
    const char *pattern             /* the extended regular expression */
);

/*****************************************************************************
 *
 * Binary Search a string by keyword within an array
 *
 * Authors: Jiawei Lin
 *
 *
 * Examples:
 *
 *   Given the array:
 *            phStringPair_t myarray[] = {{"wafer_size","ur0001"},{"wafer_unit","ur0002"}};
 *   Then, the call:
 *            phFindStrValueByStrKey(myarray, "ur0001"}
 *   will return:
 *            {"wafer_size","ur0001"}
 *
 * NOtes:
 *   The input "array" must be sorted in ascendent by the field "key"
 *   Its entry is a structure of {string, string}
 *
 * Returns:
 * The found value if the key exists, otherwise NULL
 *
 ***************************************************************************/
const phStringPair_t *phBinSearchStrValueByStrKey(
  const phStringPair_t *array,      /* search in this {string,string} array */
  int len,                          /* the array length */
  const char *key                   /* the search key */
);
/*****************************************************************************
 *
 * Search characters in a string , and then replace them with a certain char
 *
 * Authors: Garry Xie
 *
 *
 * Examples:
 *     with s[50] = "abcdeeee" ;after call  
 *     strReplaceChar(s, "dc", 'k')
 *     s = "abkkeeee"
 *  
 * Returns:
 *    FAIL    -- the characters does not exist or the string is a NULL string.
 *    SUCCESS -- find the characters and replace successfully.
 ***************************************************************************/
 int phReplaceCharsInStrWithACertainChar(
   char *s,                          /* string to be searched */
   const char *sFind,                /* characters to be replaced in the searched string*/
   const char cReplace               /* character to replace the find characters */
 );   

/*****************************************************************************
 *
 * get the Unix timestamp string
 *
 * Authors: Jiawei Lin
 *
 *
 * Examples:
 *       char *timeStamp = phToolsGetTimeStampString()
 *       the value "timeStamp" is like "2008/10/21 21:21:59.6092"
 *           where the ".6092" means 609200 us
 *
 * Returns:
 *       the formatted timestamp string
 *
 ***************************************************************************/
const char *phToolsGetTimestampString(
);


/*****************************************************************************
 * 1) this function replaces the special characters with escape characters in 
 *    a string which will be set via the ZNTF firmware command:
 *    \\ as a substitute for backslash (\)
 *    \nnn with n being an octal number:
 *    \043 as a substitute for hash (#)
 *    \012 as a substitute for cr (\n)
 *    \042 as a substitute for double quotes (")
 *    \073 as a substitute for semicolon (;)
 *    \' as a substitute for '
 * 2) enclosed the string with '' in case there are other special characters 
 *    like spaces in the string
 *
 * Author: Xiaofei Han, CR45000, 5/20/2009
 *
 * Outputs: str - a pointer to the validated/replaced string 
 *
 * Returns: -1 on any errors
 *           0 on success
 ***************************************************************************/
 int phHVMConvertString(
   char** str,  /* a pointer to the string which needs to be validated */
   const int needQuotes /* if this parameter is set to 1, enclose the whole string with '' */
 );

/*****************************************************************************
 *
 * This function is used to issue a HVM alarm
 *
 * Authors: Xiaofei Han, CR45000, 5/20/2009
 *
 * Outputs: none
 * 
 * Returns: -1 on any errors
 *          0 on success
 ***************************************************************************/
 int phIssueAlarm(
   const char* aSeverity, /* the severity of the alarm */
   const char* aCode,     /* thet alarm code */
   const char* aTextFormat,     /* the alarm text format string */
   ...
);

/*****************************************************************************
 *
 * Trim the leading and trailing delimiter in any of a sepcified set
 *
 * Authors: Roger Feng
 *
 * Description:
 *
 * Input:
 *   str    -  string to trim
 *   delim  -  delimiter set of triming 
 *
 * Return
 *  SUCCESS - trim the leading and trailing delimeters specifed
 *  FAIL    - target trim string or specified delimiter set is invalid (null)
 *
 * Note
 *  The memory specified by str is maitained by user
 *
 * Examples
 *   To trim the leading and trailing "space/tabular/new line" character in str:
 *   phTrimLeadingTrailingDelimiter(str, " \t\n");
 *
 ***************************************************************************/
 int phTrimLeadingTrailingDelimiter(
   char * const str,   
   const char * delim  
 );


/*****************************************************************************
 *
 * split the string into several sub-string by a delimiter
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * Input:
 *   str    -  string to split
 *   delim  -  delimiter used to split
 
 *   ppResultArray - the splited result is stored in it. After call of this function:
 *              ppResultArray[0]: store the pointer to the first sub-string
 *              ppResultArray[1]: store the pointer to the second sub-string
 *              ...
 *   maxSize -  the size of ppResultArray, i.e., the maximal number of sub-string
 *   sizeOfResultArray - after call, it will keep the real number of sub-string
 * 
 * Return
 *  SUCCESS - 
 *  FAIL    - 
 *
 * Note
 *  this function will alloce the memory for the memeber of "ppResultArray", the user need to free it after use, like
 *          for (int i=0; i<sizeOfResultArray; i++ ) {
 *              free(ppResultArray[i])
 *          }
 *
 * Examples
 *     char sourceString[256] = "hello;1,2,3;4,5,6"
 *     char *array[16];
 *     int size = 0;
 * 
 *     phSplitStringByDelimiter(sourceString, ';', 16, array, &size);
 * 
 *     for (int i=0; i<size; i++ ) {
 *        printf("substring: %s\n", array[i]);
 *     }
 * 
 *
 ***************************************************************************/
 int phSplitStringByDelimiter(
   const char * str,   
   char delim,   
   char **ppResultArray,
   int maxSize,
   int *sizeOfResultArray
 );

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_TOOLS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
