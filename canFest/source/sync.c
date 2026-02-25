/*
This file is part of CanFestival, a library implementing CanOpen Stack. 


Copyright (C): Edouard TISSERANT and Francis DUPIN
modified by JDCC

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


/*!
** @file   sync.c
** @author Edouard TISSERANT and Francis DUPIN
** @date   Tue Jun  5 09:32:32 2007
**
** @brief
**
**
*/

#include "includes.h"
#include "data.h"
#include "sync.h"
#include "canfestival.h"
#include "sysdep.h"
#include "sys.h" //JML debug only

/*******************************************************************************************************
*                                         Globals
********************************************************************************************************/
CPU_INT08U syncCounter = 0;
CPU_INT08U toggleSYNC = 0;

/* Prototypes for internals functions */

/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param id                                                                                       
**/  
void SyncAlarm(CO_Data* d, UNS32 id);

/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param id                                                                                       
**/   
void SyncAlarm(CO_Data* d, UNS32 id)
{
  if(ScriptDebug_Indication & BIT0)
    IO0SET = BIT0;  //JML DEBUG - See IOInit in app.c for debug usage
  
  sendSYNC(d) ;
  
  if(ScriptDebug_Indication & BIT0)
    IO0CLR = BIT0;  //JML DEBUG - See IOInit in app.c for debug usage	   
}



/*!                                                                                                
**   modified by JDCC -- changed start trigger for COB_ID to 0x0080ul                                                                                              
**   and changed time values to milliseconds from microseconds                                                                                              
** @param d                                                                                        
**/ 
void startSYNC(CO_Data* d)
{
	if(d->syncTimer != TIMER_NONE)
        {
		stopSYNC(d);
	}


	if(*d->COB_ID_Sync & 0x00000080ul && *d->Sync_Cycle_Period)
	{
		d->syncTimer = SetAlarm(
				d,
				0 /*No id needed*/,
				&SyncAlarm,
				MS_TO_TIMEVAL(*d->Sync_Cycle_Period), 
				MS_TO_TIMEVAL(*d->Sync_Cycle_Period));
                Status_modeSelect |= (1 << 5);
                syncCounter = 0;
	}
}

/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
**/   
void stopSYNC(CO_Data* d)
{
	d->syncTimer = DelAlarm(d->syncTimer);
        Status_modeSelect &= ~(1 << 5);
}


/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param cob_id                                                                                   
** modified by jdcc                                                                                                
** @return                                                                                         
**/  
UNS8 sendSYNCMessage(CO_Data* d)
{
  Message m;
 
  CAN_ClearOnSYNC = 0;
  // if in Produce X, check for limit
  if ((Status_NumberOfSyncs > 0) &&( ++syncCounter > Status_NumberOfSyncs) && (Mode_Produce_X_Manual == getState(d)))
  { 
//    OSTimeDlyHMSM(0, 0, 0, 100,
//        OS_OPT_TIME_HMSM_STRICT,
//        &err);
//    
//    unsigned char cmd[3];
//    cmd[0] = 7;
//    cmd[1] = 0;
//    cmd[2] = 0;
//    masterSendNMTstateChange(&ObjDict_Data, 0, cmd ); 
//       
//    setState(d,Waiting);
    return 0;
  }
  
  
  m.cob_id = UNS16_LE(*d->COB_ID_Sync);
  m.rtr = NOT_A_REQUEST;
  m.len = 8;
  
  m.data[0] = ControlOutput1;
  m.data[1] = ControlOutput2;
  m.data[2] = ControlOutput3;
  m.data[3] = ControlOutput4;
  m.data[4] = ControlOutput5;
  m.data[5] = ControlOutput6;
  m.data[6] = ControlOutput7;
  m.data[7] = ControlOutput8;
  
  return canSend(d->canHandle,&m);
  
}


/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param cob_id                                                                                   
**                                                                                                 
** @return                                                                                         
**/  
UNS8 sendSYNC(CO_Data* d)
{
  UNS8 res;
  res = sendSYNCMessage(d);
  processSYNC(d) ; 
  return res ;
}

/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param m                                                                                        
**                                                                                                 
** @return                                                                                         
**/ 
UNS8 processSYNC(CO_Data* d)
{
  UNS8 res;
  MSG_WAR(0x3002, "SYNC received. Proceed. ", 0);
  
  (*d->post_sync)(d);
  /* only operational state allows PDO transmission */
  if(! d->CurrentCommunicationState.csPDO) 
    return 0;

  res = _sendPDOevent(d, 1 /*isSyncEvent*/ );
  
  /*Call user app callback*/
  (*d->post_TPDO)(d);
  
   
  return res;
  
}


void _post_sync(CO_Data* d){}
void _post_TPDO(CO_Data* d){}
