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

#ifndef __CAN_CPU__
#define __CAN_CPU__

#include "config.h"

// Canfestivals includes
#include "can.h"



/************************* To be called by user app ***************************/
void canEnable(void);
void canDisable(void);
void canInit( void );
void canReset(void);
//unsigned char canInit(unsigned int bitrate);
unsigned char canSend(CAN_PORT notused, Message *m);
unsigned char canReceive(Message *m);

#endif
