/*%Z%************************************************************************
 *%Z%
 *%Z%                       MODULE  : pg_templ.c
 *%Z%
 *%Z%-----------------------------------------------------------------------
 *%Z%
 *%Z%        (c) Copyright Agilent Technologies GmbH, Boeblingen 1993
 *%Z%
 *%Z%-----------------------------------------------------------------------
 *%Z%
 *%Z%  ericfurm. , SSTD-R&D
 *%Z%
 *%Z%  DATE    : 30.06.93
 *%Z%
 *%Z%-----------------------------------------------------------------------
 *%Z%
 *%Z%  CONTENTS: Template for user written programs.
 *%Z%
 *%Z%
 *%Z%  Instructions:
 *%Z%
 *%Z%  1) Copy this template to as many .c files as you require,
 *%Z%
 *%Z%  2) Use the command 'make depend' to make visible the new
 *%Z%     source files to the makefile utility
 *%Z%
 *%Z%  3) Use the command 'make' to generate executables
 *%Z%
 *%Z%  DESCRIPTION:
 *%Z%
 *%Z%  This program is an example of how a command-set call as specified in
 *%Z%  the application model file can be realized. The program itself is a
 *%Z%  trivial example of string manipulation, but it does demontrate usage
 *%Z%  of the input and output medium provided for programs
 *%Z%  ---------------------------------------------------------------------
 *%Z%
 *%Z%  Valid Exit Codes:
 *%Z%
 *%Z%  CI_CALL_PASS   = 0  program completed successfully
 *%Z%  CI_TEST_PASS   = 0  test passed
 *%Z%  CI_TEST_FAIL   = 1  test failed
 *%Z%  CI_CALL_ERROR  = 2  program failure
 *%Z%  CI_CALL_BREAKD = 3  tpi break
 *%Z%
 *%Z%  ---------------------------------------------------------------------
 *%Z%  Reverses the input parameter string and prints the result to stdout
 *%Z%  as the value to be assigned to the application model file variable
 *%Z%
 *%Z%  Program Call:
 *%Z%
 *%Z%  *val = EXEC_INP(reversi abc);
 *%Z%
 *%Z%  Result: *val is assigned the value "cba")
 *%Z%
 *%Z%  ----------------------------------------------------------------------
 *%Z%  INPUTS:  char *argv[]            input parameter array
 *%Z%           int  argc               No. of parameters
 *%Z%
 *%Z%  OUTPUTS: stdout                  output/result buffer
 *%Z%           stderr                  directed to report window
 *%Z%           exit() code             success state of the program call
 *%Z%
 *%Z%=======================================================================
 ***************************************************************************/

/*
 *-- system includes ------------------------------------------------------
 */

#include <stdio.h>

/*
 *-- module include -------------------------------------------------------
 */

#include "tpi_c.h"
#include "adiUserI.h"
#include "ci_types.h"
#include "libcicpi.h"

/*
 *-- external functions ---------------------------------------------------
 */

/*
 *-- external variables ---------------------------------------------------
 */

/*
 *-- defines --------------------------------------------------------------
 */

#define MINIMUM(a,b)                                ((a) < (b) ? (a) : (b))

/*
 *-- typedefs -------------------------------------------------------------
 */

/*
 *-- globals variables ----------------------------------------------------
 */

/*
 *-- functions ------------------------------------------------------------
 */

/***************************************************************************/
/***************************************************************************/
/* begin 12/10/2001 301 */
int main(int argc, char* argv[])
/* end */
{
    /* local variables */
    int k;

/* begin 03/10/2001 79 */
/* There are no defination of the function _main()   on Linux  */
#if !defined(OS_LINUX) && !defined(__cplusplus)
    _main();              /* mandatory statement in a program source file */
#endif
/* end */
    HpInit();

    if (argc == 1)
    {
        printf("no parameters");
    }
    else
    {
        /* The maximum length of all parameters is 78 bytes, so it is safe to
         * make a direct printf without checking if we exceed
         * CI_MAX_COMMENT_LEN bytes
         */

        for(k = (argc - 1); k > 0; k--)    /* step back through arguments */
        {
            printf("%s", argv[k]);
            printf(" ");
        }
    }

    fprintf(stderr, "PROGRAM: report window msg\n");
    
    HpTerm();

    exit(CI_CALL_PASS);                /* program completed without error */
}
