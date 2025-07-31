/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_tools.c
 * CREATED  : 27 Jan 2000
 *
 * CONTENTS : General tools ro be used by each component
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, add "phBinSearchStrValueByStrKey"
 *            Garry Xie, R&D Shanghai, add "phReplaceCharsInStrWithACertainChar"
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 27 Jan 2000, Michael Vogt, created
 *            August 2005, Jiawei Lin, add "phBinSearchStrValueByStrKey"
 *            Feburary 2006, Garry Xie, add "phReplaceCharsInStrWithACertainChar"
 *            Oct 2008, Jiawei Lin, CR42750
 *              add the function "phToolsGetTimeStampString"
 *            20 Mar 2009, Roger Feng, add "phTrimLeadingTrailingDelimiter"
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
#include <regex.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Match a string
 *
 * Authors: Michael Vogt
 *
 * Description:
 * see ph_tools.h
 *
 ***************************************************************************/
int phToolsMatch(const char *string, const char *pattern)
{
    int i;
    regex_t re;
    regmatch_t pmatch[2];

    i = regcomp(&re, pattern, REG_EXTENDED);
    if (i) 
	return 0;                       /* report error */
    i = regexec(&re, string, 1, pmatch, 0);
    regfree(&re);
    if (i || pmatch[0].rm_so != 0 || pmatch[0].rm_eo != strlen(string)) 
	return 0;                       /* report error */

    return 1;
}

/*****************************************************************************
 *
 * Binary Search a string by keyword within an array
 *
 * Authors: Jiawei Lin
 *
 * Description:
 * see ph_tools.h
 *
 ***************************************************************************/
const phStringPair_t *phBinSearchStrValueByStrKey(
  const phStringPair_t *array,    /* must be sorted by "key" field */
  int len,                        /* the length of 'array' */
  const char *key                 /* the search key */
)
{
  const phStringPair_t *pLow = NULL;
  const phStringPair_t *pHigh = NULL;
  const phStringPair_t *pMiddle = NULL;


  /* 
  * Now do a binary search using plain strcasecmp() comparison.
  */
  pLow = &array[0];
  pHigh = END_OF_ARRAY(array, len) - 1;

  while (pLow <= pHigh)
  {
    pMiddle = pLow + (pHigh - pLow) / 2;

    int difference = strcasecmp(pMiddle->key, key);
    if ( difference == 0 ){
      return pMiddle;
    } else if ( difference < 0 ){
      pLow = pMiddle + 1;
    } else{
      pHigh = pMiddle - 1;
    }
  }

  return NULL;
}

/*****************************************************************************
 *
 * Search characters in a string , and then replace them with a certain.char
 *
 * Authors: Garry Xie
 *
 * Description:
 * see ph_tools.h
 ***************************************************************************/
 int phReplaceCharsInStrWithACertainChar(
   char *s,                          /* string to be searched */
   const char *sFind,                /* characters to be replaced in the searched string*/
   const char cReplace               /* a certain character to replace the find characters */
 )
 {
   char *p = NULL;

   if( (s == NULL) || (s[0] == '\0') ) {
     return FAIL;
   }

   if( (sFind == NULL) || (sFind[0] == '\0') ) {
     return FAIL;
   }
   
   p = strpbrk(s, sFind);
   if( p == NULL ){
     return FAIL;
   }

   while( p != NULL ) {
     *p = cReplace;
     p = strpbrk(s, sFind);
   }

   return SUCCEED;
 }

/*****************************************************************************
 *
 * get the Unix timestamp string
 *
 * Authors: Jiawei Lin
 *
 * Description:
 * see ph_tools.h
 ***************************************************************************/
const char *phToolsGetTimestampString()
{
  static char timeString[64] = "";
  struct tm *localTime = NULL;
  time_t timeInSeconds = 0;
  struct timeval unixTime;

  gettimeofday(&unixTime, NULL);
  timeInSeconds = (time_t) unixTime.tv_sec;
  localTime = localtime(&timeInSeconds);

  sprintf(timeString, "%4d/%02d/%02d %02d:%02d:%02d.%04d",
          localTime->tm_year+1900,
          localTime->tm_mon+1,
          localTime->tm_mday,
          localTime->tm_hour,
          localTime->tm_min,
          localTime->tm_sec,
          (int)(unixTime.tv_usec/100));

  /*sprintf(timeString, "%4d/%02d/%02d %02d:%02d:%02d.%06d",
          localTime->tm_year+1900,
          localTime->tm_mon+1,
          localTime->tm_mday,
          localTime->tm_hour,
          localTime->tm_min,
          localTime->tm_sec,
          (int)(unixTime.tv_usec));*/


  return timeString;
}


/*****************************************************************************
 * this function find any appearance of a substring and replace this substring
 * with another string.
 *
 * Author: Xiaofei Han, CR45000, 5/20/2009
 *
 * Outputs: sourceString - the replaced string
 *
 * Returns: -1 on any errors
 *          0 on success
 ***************************************************************************/
static int phSubStringFindAndReplace(
   char** sourceString,  /* a pointer to the source string which needs substring replacement */
   const char* subString, /* the substring which needs to be replaced by other string */
   const char* replaceSubString /* the string which is used to replace the substring */
 )
{
  char tempString[MAX_PH_SUBSTR_REPLACE_STRING_LEN+1];
  char *startOfSub = NULL, *remainingStr = NULL;
  void *p = NULL;
  int countLen = 0;


  /* check the input parameters */
  if( sourceString == NULL || *sourceString == NULL || subString == NULL || replaceSubString == NULL || strlen(*sourceString)>MAX_PH_SUBSTR_REPLACE_STRING_LEN)
  {
    return -1;
  }
 
  tempString[0] = '\0'; 

  /* start the replacement */
  startOfSub = strstr(*sourceString, subString);
  remainingStr = *sourceString;
  while(startOfSub != NULL)
  {
     /* make sure the string length doesn't exceed the max length */
     countLen = countLen + (startOfSub - remainingStr + strlen(replaceSubString));
     if(countLen>MAX_PH_SUBSTR_REPLACE_STRING_LEN)
     {
       return -1;
     }

     strncat(tempString, remainingStr, (size_t)(startOfSub-remainingStr));
     strcat(tempString, replaceSubString);
 
     /* bypass the substring and continue the search */
     remainingStr = startOfSub + strlen(subString);
     startOfSub = strstr(remainingStr, subString);
  }

  /* make sure the string length doesn't exceed the max length */
  if((countLen + strlen(remainingStr))>MAX_PH_SUBSTR_REPLACE_STRING_LEN)
  {
    return -1;
  }

  /* only modify the source string when there are replacement occurs  */
  if(remainingStr != *sourceString)
  {
    strcat(tempString, remainingStr);
    tempString[strlen(tempString)] = '\0';

    /* realloc memory for the sourceString */
    if((p = realloc(*sourceString, strlen(tempString)+1)) == NULL)
    {
       return -1;
    }
  
    /* copy the content of the temp string to sourceString */
    *sourceString = (char*)p;
    strcpy(*sourceString, tempString);
  }

  return 0;
}



/*****************************************************************************
 * this function is used to enclose source string with specified
 * string, for example, enclose "abc" with "'", the string becomes 'abc'.
 *
 * Author: Xiaofei Han, CR45000, 5/20/2009
 *
 * Outputs: sourceString - the final enclosed string
 *
 * Returns: -1 on any errors
 *          0 on success
 ***************************************************************************/
static int phEncloseStr(
   char** sourceString,  /* a pointer to the source string */
   const char* encloseStr /* the string with which the source string is enclosed  */
 )
{
  char *tempString = NULL;
  void *p = NULL;

  /* check the input parameters */
  if( sourceString == NULL || *sourceString == NULL || encloseStr == NULL)
  {
    return -1;
  }
 
  tempString = (char*)malloc(strlen(*sourceString)+2*strlen(encloseStr)+1);
  if(tempString == NULL)
  {
   return -1;
  }

  /* enclose the source string with the encloseStr */
  strcpy(tempString, encloseStr);
  strcat(tempString, *sourceString);
  strcat(tempString, encloseStr);

  if((p = realloc(*sourceString, strlen(tempString)+1)) == NULL)
  {
    free(tempString);
    return -1;
  }

  *sourceString = (char *)p;
  strcpy(*sourceString, tempString);
  free(tempString);
  return 0;
}


/*****************************************************************************
 *
 * validate and reformat a string for ZNTF firmware command 
 *
 * Authors: Xiaofei Han
 *
 * Description:
 * see ph_tools.h
 ***************************************************************************/
int phHVMConvertString(char** str, const int needQuotes)
{
  char* tempStr = NULL;
  unsigned int i = 0;
  int empty_string = 1;
  void* p = NULL;

  if(str == NULL || *str == NULL)
  {
    return -1;
  }

  /* determine if the string is empty */
  if((*str)[0] == '\0')
  {
    empty_string = 1;
  }
  else
  {
     for(i=0; i<strlen(*str); i++)
     {
       if((*str)[i] != ' ')
       {
         empty_string = 0;
         break;
       }
     }
  }

  if(empty_string == 1)
  {
    /* if it's an empty string, reformat the string to '' */
    if((p = realloc(*str, 3)) == NULL)
    {
      return -1;
    }
    *str = (char *)p;
    strcpy(*str, "\'\'");
    return 0;
  }
  else
  {
     tempStr = (char*)malloc(strlen(*str)+1);
     if(tempStr == NULL)
     {
       return -1;
     }

     strcpy(tempStr, *str);

    /* replace the special characters within the escape character
     *  \\ as a substitute for backslash (\)
     *  \nnn with n being an octal number:
     *  \043 as a substitute for hash (#)
     *  \012 as a substitute for cr (\n)
     *  \042 as a substitute for double quotes (")
     *  \073 as a substitute for semicolon (;)
     *  \' as a substitute for '
     */

    if(phSubStringFindAndReplace(&tempStr, "\\", "\\\\") != -1 &&
       phSubStringFindAndReplace(&tempStr, "\n", "\\012") != -1 &&
       phSubStringFindAndReplace(&tempStr, "#", "\\043") != -1 &&
       phSubStringFindAndReplace(&tempStr, "\"", "\\042") != -1 &&
       phSubStringFindAndReplace(&tempStr, ";", "\\073") != -1 &&
       phSubStringFindAndReplace(&tempStr, "\'", "\\\'") != -1)
    {
      if(needQuotes == 1)
      {
        /* enclose the string with '' */
        if(phEncloseStr(&tempStr, "\'") == -1)
        {
          free(tempStr);
          return -1;
        }
      }
    }
    else
    {
      free(tempStr);
      return -1;
    }
  
    /* copy the reformatted string to the source string */
    if((p=realloc(*str, strlen(tempStr)+1)) == NULL)
    {
      free(tempStr);
      return -1;
    }

    *str = (char *)p;
    strcpy(*str, tempStr);
    free(tempStr);
    return 0;
  }
}


/*****************************************************************************
 *
 * Issue the HVM alarm
 *
 * Authors: Xiaofei Han
 *
 * Description:
 * see ph_tools.h
 ***************************************************************************/
int phIssueAlarm(
    const char* aSeverity,
    const char* aCode,
    const char* aTextFormat,
    ...
)
{
    /* function return value */
    int phRtnValue = 0;

    if(aSeverity == NULL || aCode == NULL || aTextFormat == NULL)
    {
        phRtnValue = -1;
    }
    else
    {
    }

    return phRtnValue;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/


/*****************************************************************************
 *
 * Trim the leading and trailing delimeters in any of a specified set
 *
 * Authors: Roger Feng
 *
 * Description:
 * see ph_tools.h
 ***************************************************************************/
 int phTrimLeadingTrailingDelimiter(
   char * const str,   /* string to trim*/
   const char * delim  /* delimiter set of triming*/
 )
 {

   char * ptr = NULL;

   if((str == NULL) || (delim == NULL))
   {
     return FAIL;
   }

   if((str[0] == '\0') || (delim[0] == '\0'))
   {
     return SUCCEED;
   }

   ptr = str;
   while((ptr[0] != '\0' )&&(strchr(delim, ptr[0]) != NULL))
   {
     ptr++;
   }

   if(ptr != str)
   {
     strcpy(str, ptr);
   }


   while((str[0] != '\0') && (strchr(delim, str[strlen(str) - 1]) != NULL))
   {
     str[strlen(str) - 1] = '\0';
   }

   return SUCCEED;
 }

 //split a string into sub-string; see ph_tools.h
 int phSplitStringByDelimiter(
   const char * str,   
   char delim,   
   char **ppResultArray,
   int maxSize,
   int *sizeOfResultArray
 )
 {
   if( str==NULL || ppResultArray==NULL ) {
     return FAIL;
   }

   const char *p = str;
   char buf[129] = {0};
   int j = 0;
   int index = 0;

   

   while(*p) {
     char ch = *p;
          
     if( ch == delim ) {

       if( index == maxSize ) {
         return FAIL;
       }
       
       buf[j] = '\0';
       ppResultArray[index] = (char *)malloc(128);
       strcpy(ppResultArray[index], buf);
       
       buf[0] = 0;
       j=0;
       index++;
     } else {
       buf[j++] = ch;
     }
     
     p++;
   }

   if( index == maxSize ) {
     return FAIL;
   }

   //collect the last substring
   buf[j] = '\0';

   ppResultArray[index] = (char *)malloc(128);
   strcpy(ppResultArray[index], buf);

   
   *sizeOfResultArray = index+1;

   return SUCCEED;
 }
