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

//#define DEBUG_WAR_CONSOLE_ON
//#define DEBUG_ERR_CONSOLE_ON

//^^REV: Ported to uCosII jayh dec2009; use the existing Micrium CANBus layer.


#include "can_bus.h"
#include "can_cpu.h"
#include "canfestival.h"
#include "drv_can_reg.h"
#include "includes.h"
#include "sys.h"



volatile unsigned char msg_received = 0;
extern const CANBUS_PARA CanCfg;

void canEnable(void)
{
  CPU_INT16U timeout_ticks;
  
  PCONP |= BIT13;  //enable CAN1 peripheral
  CanBusEnable((CANBUS_PARA *) &CanCfg); 
  
    //Network must be enabled before timeouts are set
  //JML >> timeout units are in ticks (1tick = 1ms)
  timeout_ticks = 10;  //maximum waiting time to transmit if transmitter is busy
  //this will block the task calling canSend, and should not be left infinte (0)
  CanBusIoCtl(0, CANBUS_SET_TX_TIMEOUT, &timeout_ticks); 
  //The RX timeout can stay the default of 0 (infinite), because the canReceive 
}

void canDisable( void )
{
  OS_ERR    err;
  
  if(CanBusDisable(0)==0) // Disable bus 0 (only active bus)
  {
    OSTimeDlyHMSM(0, 0, 0, 5, OS_OPT_TIME_HMSM_STRICT, &err); //delay to allow any remaining TX bits to complete
    //JML Note: It is unclear how long this delay needs to be, but without it disabling the CAN Perihperal below
    //to save power sometimes results in a PM crash (particularly if the CAN bus is having lots of errors)
    PCONP &= ~BIT13; 
  }
}

void canInit( void )
{
  CanBusInit(0L);
  PCONP &= ~( BIT13 | BIT14 ); //disable both CAN peripherals
}

void canReset( void )
{
  CanBusIoCtl(0, CANBUS_FLUSH_TX, NULL); // reset the can bus
  CanBusIoCtl(0, CANBUS_FLUSH_RX, NULL); //
  CanBusIoCtl(0, CANBUS_RESET, NULL); //
  
  CAN_Rx_ErrCounter = (LPC21XX_CAN_C1GSR >> 16) & 0x00FF; 
  CAN_Tx_ErrCounter = (LPC21XX_CAN_C1GSR >> 24) & 0x00FF;
}

unsigned char canSend(CAN_PORT notused, Message *m)
/******************************************************************************
The driver sends a CAN message passed from the CANopen stack
INPUT	CAN_PORT is not used (only 1 avaiable)
	Message *m pointer to message to send
OUTPUT	1 if  hardware -> CAN frame
******************************************************************************/
{
	CANFRM busFrame;
        CPU_INT16S err;
        
        if (!(PCONP & BIT13))  // check that CAN peripheral is active
          return 1;
        
	busFrame.Identifier = m->cob_id & 0x1fffffff;
	if( m->rtr != 0 )
	{
		/* set rtr bit */
		busFrame.Identifier |= 0x40000000 ;
	}
	memcpy( busFrame.Data, m->data, m->len );
	busFrame.DLC = m->len;

	err = CanBusWrite(0,                                /* write it back to can bus layer          */
         	 	(CPU_INT08U *)&busFrame,
          	  	sizeof(CANFRM));
        
        if(err < 0)  //can TX semaphore timed out or other TX error (e.g. bad network connection)
        {
          CanBusIoCtl(0, CANBUS_FLUSH_TX, NULL); // reset the can TX semaphore
          CanBusIoCtl(0, CANBUS_RESET, NULL);
          CAN_TX_ResetCounter++;
          return 3;
        }
	
    return 0;	//^^bug? 1;	// succesful
}

unsigned char canReceive(Message *m)
/******************************************************************************
The driver passes a received CAN message to the stack
INPUT	Message *m pointer to received CAN message
OUTPUT	1 if a message received
******************************************************************************/
{
        CANFRM 		busFrame;                                       /* Local: can frame                        */
	CPU_INT16S	len;
               
                                                            	        /* blocking mode                           */
        len = CanBusRead(0,                                 		/* read next can msg from can bus layer    */
               		(CPU_INT08U *)&busFrame, 
               		sizeof(CANFRM));            
	
	if( len > 0 )
	{
		m->cob_id 	= (UNS16) busFrame.Identifier ;
		m->rtr		= (busFrame.Identifier & 0x40000000)?  1 : 0 ;
		m->len		= busFrame.DLC ;
		memcpy( m->data, busFrame.Data, busFrame.DLC );
		return 1;
	}
	else //error
        {
              CanBusIoCtl(0, CANBUS_FLUSH_RX, NULL); // reset the can TX semaphore
              CanBusIoCtl(0, CANBUS_RESET, NULL);
              CAN_RX_ResetCounter++;
          
              return 0;
        }
}

