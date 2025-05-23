/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN
AVR Port: Andreas GLAUSER and Peter CHRISTEN

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

#ifndef __TIMERSCFG_H__
#define __TIMERSCFG_H__

// Whatever your microcontroller, the timer wont work if
// TIMEVAL is not at least on 32 bits
#define TIMEVAL UNS32

// needs to be checked for LPC 2129


// The timer of the AVR counts from 0000 to 0xFFFF in CTC mode (it can be
// shortened setting OCR3A eg. to get 2ms instead of 2.048ms)
#define TIMEVAL_MAX 	0xFFFFFFFF //MS_TO_TIMEVAL(100)	//^^0xFFFF

// The timer is incrementing every 4 us.
//#define MS_TO_TIMEVAL(ms) (ms * 250)
//#define US_TO_TIMEVAL(us) (us>>2)

//^^ The timer is incrementing every 100 us.
//#define MS_TO_TIMEVAL(ms) ((ms) * 10)
//#define US_TO_TIMEVAL(us) ((us) / 100)

// The timer is incrementing every 8 us. 
//If you change this, you also need to change US_TO_TIMEVAL_FACTOR in config.h
#define MS_TO_TIMEVAL(ms) ((ms) * 125)
#define US_TO_TIMEVAL(us) ((us)>>3)

#endif
