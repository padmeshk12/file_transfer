/* 
 * To make the PH driver compilable without dependency on SmarTest,
 * this file is provided. 
 * Note: This file may eventually be removed. Don't add new contents.
 *
 */
#ifndef PH_COMPILE_COMPATIBLE 
#define PH_COMPILE_COMPATIBLE

#define PINNAME_S_LENGTH                16
#define TF_MAX_NR_MEASUREMENTS         2
#define TF_PIN_RESULTS_NOT_AVAILABLE       0
#define TF_MAX_NR_RSLTS                4
#define TF_PIN_RESULTS_AND_VAL_AVAILABLE   2
#define TF_PIN_RESULTS_AVAILABLE           1
#define TF_PIN_RESULTS_NOT_AVAILABLE       0
#define TF_PIN_RESULTS_ARRAY_TOO_SMALL    -1

typedef char           CHAR;
typedef char           INT8;
typedef double         FLOAT64;
typedef int            INT32;
typedef unsigned int   BOOLEAN;
typedef unsigned int   UINT32;


typedef struct
{
  INT8    result;
  FLOAT64 value;
} TF_MEASUREMENT;

typedef struct
{
  CHAR           pinname[PINNAME_S_LENGTH + 1];
  TF_MEASUREMENT meas[TF_MAX_NR_MEASUREMENTS];
} TF_PIN_RESULT;

#endif


