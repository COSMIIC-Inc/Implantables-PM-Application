/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN
modified by JDC Consulting, Jay Hardway

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// AVR implementation of the  CANopen timer driver, uses Timer 3 (16 bit)

//^^REV: Ported to uCosII jayh dec2009; theory: although sized for 32bit args, these
//		functions really need a 16bit timer roll over for 534ms max latency on run-away.
//		so, force a rollover for timer1 in the bsp.c routines.
//      ^^JML 20141223, the timer does not need to roll over at 16bit.  Rollovers must be handled
//        to support long alarms (anything longer than 524 ms with 16-bit).  With 32-bit, rollovers 
//        happen much less frequently (~every 9.5 hours).  This means the timer interrupt is used almost
//        exclusively for handling alarms rather than unnecessarily handling rollovers.     


// Includes for the Canfestival driver

/*! 
** @file timer_ucCan.c
** @author Hardway
** @brief Ported to uCosII jayh dec2009
** Ported to uCosII jayh dec2009;
** @ingroup networkmanagement
*/



#include "timer.h"
#include "bsp.h"
#include "RunCANServer.h"

void incrTimer1MatchReg0( UNS32 value );
UNS32 GetCANTimer1Count( void );  // called from BSP.c

 


/************************** Module variables **********************************/
// Store the last timer value to calculate the elapsed time
static TIMEVAL last_time_set = TIMEVAL_MAX;     



void setTimer(TIMEVAL value)
/******************************************************************************
Set the timer for the next alarm.
INPUT	value TIMEVAL (unsigned long)
OUTPUT	void
******************************************************************************/
{
    incrTimer1MatchReg0( value );

}

TIMEVAL getElapsedTime(void)
/******************************************************************************
Return the elapsed time to tell the Stack how much time is spent since last call.
INPUT	void
OUTPUT	value TIMEVAL (unsigned long) the elapsed time
******************************************************************************/
{
  UNS32 timer = GetTimer1Count();            // Copy the value of the running timer
  
  return timer - last_time_set ;		//^^
 
}


void canOpenAlarm_interrupt(void)
/******************************************************************************
Interruptserviceroutine 
******************************************************************************/
{
  OS_ERR err;
  
  last_time_set = GetTimer1Count();
  OSSemPost(&CanTimerSem,  OS_OPT_POST_1, &err);
  //TimeDispatch();                               // Call the time handler of the stack to adapt the elapsed time
}



