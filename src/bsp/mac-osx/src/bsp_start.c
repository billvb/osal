/******************************************************************************
** File:  bsp_start.c
**
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement. 
**
**      Copyright (c) 2004-2006, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. 
**
**
** Purpose:
**   OSAL BSP main entry point.
**
** History:
**   2005/07/26  A. Cudmore      | Initial version for OS X 
**
******************************************************************************/

/*
**  Include Files
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

/*
** OSAL includes 
*/
#include "common_types.h"
#include "osapi.h"

/*
** Types and prototypes for this module
*/

/*
**  External Declarations
*/
void OS_Application_Startup(void);
                                                                           
/*
** Global variables
*/
                                                                                                                                                               
                                                                                                                                                
/******************************************************************************
**  Function:  main()
**
**  Purpose:
**    BSP Application entry point.
**
**  Arguments:
**    (none)
**
**  Return:
**    (none)
*/

int main(void)
{
   /*
   ** Initialize the OS API data structures
   */
   OS_API_Init();
     
   /*
   ** Call application specific entry point.
   */
   OS_Application_Startup();
   
   /*
   ** Let the main thread sleep 
   */     
   for ( ;; )
   {
      sleep(1);
   }
                 
   return(0);
}

