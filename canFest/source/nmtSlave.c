/*
  This file is part of CanFestival, a library implementing CanOpen
  Stack.

  Copyright (C): Edouard TISSERANT and Francis DUPIN
  modified by JDC Consulting. Master copy kept at
  https://jdc-consulting.unfuddle.com/svn/jdc-consulting_mastercodecanfestival/
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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
  USA
*/
/*!
** @file   nmtSlave.c
** @author Edouard TISSERANT and Francis DUPIN
** @date   Tue Jun  5 08:50:53 2007
**
** @brief
**
**
*/
#include "nmtSlave.h"
#include "states.h"
#include "canfestival.h"
#include "sysdep.h"


/*!
** put the slave in the state wanted by the master
**
** @param d
** @param m
**/

/* NMT Commands, sent by master to change a slave state 
------------------------------------------------------------- 

#define NMT_Start_Nodes               0x01    
#define NMT_Stop_Nodes                0x02
#define NMT_Enter_Patient_Operation   0x03
#define NMT_Enter_X_Manual            0x04
#define NMT_Enter_Y_Manual            0x05
#define NMT_Enter_Stop_Stim           0x06    
#define NMT_Enter_Wait_Mode           0x07    
#define NMT_Enter_Patient_Manual      0x08    
#define NMT_Enter_Produce_X_Manual    0x09
#define NMT_Do_Save_Cmd               0x0A
#define NMT_Do_Restore_Cmd            0x0B
#define NMT_Enter_Record_X            0x0C
#define NMT_Network_On                0x0D
#define NMT_Network_Off               0x0E
#define NMT_Return_Node_Table	      0x10
#define NMT_Return_Single_Node	      0x11
#define NMT_Reset_Node                0x81
#define NMT_Reset_Comunication        0x82
#define NMT_Enter_Bootloader          0x83
#define NMT_Start_Sync                0x84
#define NMT_Stop_Sync                 0x85
#define NMT_Reset_Watchdog            0x88
**************************************************************/

void processNMTstateChange(CO_Data* d, Message *m)
{
  
  // check to see if the system can handle external messages
  if( d->nodeState != Hibernate ||
      d->nodeState != BootCheckReset ) 
  {

    MSG_WAR(0x3400, "NMT received. for node :  ", (*m).data[1]);

    /* Check if this NMT-message is for this node */
    /* byte 1 = 0 : all the nodes are concerned (broadcast) */

    if( ( (*m).data[1] == 0 ) || ( (*m).data[1] == *d->bDeviceNodeId ) )
    {
      switch( (*m).data[0])
      { /* NMT command specifier (cs) in def.h*/
        // command interpreter
        case NMT_Start_Nodes:
          if ( d->nodeState == Hibernate) 
            setState(d,Waiting);
          else
            setState(d,Unknown_state);
          break;
  
        case NMT_Stop_Nodes:
            setState(d,Stopped);
          break;
  
        case NMT_Enter_Wait_Mode:  // no restrictions on entering Waiting
            setState(d,Waiting);
            StopWatchDog( d );
          break;
          
        case NMT_Enter_Patient_Operation:
          if (d->nodeState == Waiting && (*m).data[1] == 0)
            setState(d,Mode_Patient_Control);
          break;  
  
        case NMT_Enter_X_Manual:
          if (d->nodeState == Waiting && (*m).data[1] == 0)
          {
            setState(d,Mode_X_Manual);
            StartWatchDog(d, 10000);
          }
          break;
          
        case NMT_Enter_Y_Manual:
          if (d->nodeState == Waiting && (*m).data[1] == 0)
          {
            setState(d,Mode_Y_Manual);
            StartWatchDog(d, 10000);
          }
          break;
          
        case NMT_Enter_Stop_Stim:  // no restriction on entering Stopped
            setState(d,Stopped);
          break;
          
        case NMT_Reset_Watchdog:
          if ( (*m).data[1] == *d->bDeviceNodeId) // must be targeted to this node
              StartWatchDog(d, 10000);
          break;  
          
        case NMT_Reset_Node:
           if(d->NMT_Slave_Node_Reset_Callback != NULL)
              d->NMT_Slave_Node_Reset_Callback(d);
           if ((*m).data[1] == *d->bDeviceNodeId)
              setState(d,Waiting);
          break;
  
        case NMT_Reset_Comunication:
          {
             if ( (*m).data[1] == *d->bDeviceNodeId)
             {
              UNS8 currentNodeId = getNodeId(d);
             
                if(d->NMT_Slave_Communications_Reset_Callback != NULL)
                   d->NMT_Slave_Communications_Reset_Callback(d);
        #ifdef CO_ENABLE_LSS
                // LSS changes NodeId here in case lss_transfer.nodeID doesn't 
                // match current getNodeId()
                if(currentNodeId!=d->lss_transfer.nodeID)
                   currentNodeId = d->lss_transfer.nodeID;
        #endif
    
                // clear old NodeId to make SetNodeId reinitializing
                // SDO, EMCY and other COB Ids
                *d->bDeviceNodeId = 0xFF; 
             
                setNodeId(d, currentNodeId);
             }
          }
           break;

      }/* end switch */

    }/* end if( ( (*m).data[1] == 0 ) || ( (*m).data[1] ==
        bDeviceNodeId ) ) */
  }
}

/*!
**
**
** @param d
**
** @return
**/
UNS8 slaveSendBootUp(CO_Data* d)
{
  Message m;

#ifdef CO_ENABLE_LSS
  if(*d->bDeviceNodeId==0xFF)return 0;
#endif

  MSG_WAR(0x3407, "Send a Boot-Up msg ", 0);

  /* message configuration */
  {
	  UNS16 tmp = NODE_GUARD << 7 | *d->bDeviceNodeId; 
	  m.cob_id = UNS16_LE(tmp);
  }
  m.rtr = NOT_A_REQUEST;
  m.len = 1;
  m.data[0] = 0x00;

  return canSend(d->canHandle,&m);
}


//=====================
//	WATCH DOG ROUTINES
//=====================

static CO_Data *wdogCoData;


void StartWatchDog( CO_Data* d, UNS16 timebase )
{
  	//setAppTimeOut1( timebase );
        // need #define for correct module
	
	wdogCoData = d;
  
}

void StopWatchDog( CO_Data* d )
{
  	//setAppTimeOut1( 0 );
  
  	wdogCoData = d;
  
}

void setNodeStateToStopped( void )
{
	if( wdogCoData != NULL )
	{
		setState( wdogCoData, Stopped);
	}
	
}



