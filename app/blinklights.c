
// Doxygen
/*!
** @file   blinklights.c
** @author Coburn
** @date   2/14/2010
**
** @brief Blinks LED at P1_16 for Kiel MCB2100 board
** @ingroup iotasks
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "sys.h"
#include "blinklights.h"



static unsigned int n = 0;


void BlinkLightsInit(void)
{
     
     IOCLR1 = 0x01FF0000;
     IODIR1 = 0x01FF0000;             /* P1.16..24 defined as Outputs  */      
   
}


 /* Telltale blinks LED to show OS operation */

void BlinkLights(void)             
{
  
    if (n)
    {
                             
    //^^NO this is THRM cs pin   IOSET1 = 0x00010000;
       n = 0;
    }
    else
    {   
       //^^NO this is THRM cs pin   IOCLR1 = 0x00010000;
       n = 1;
    } 
    return;

}

  
  
