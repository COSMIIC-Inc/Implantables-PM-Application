/*
This file is part of CanFestival, a library implementing CanOpen Stack. 

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*!
** @file   states.c
** @author Edouard TISSERANT and Francis DUPIN
** @date   Tue Jun  5 09:32:32 2007
**
** @brief
**
**
*/

#include "data.h"
#include "sysdep.h"
#include "includes.h"
#include "RMBootloader.h"

/** Prototypes for internals functions */
/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param newCommunicationState                                                                    
**/     
void switchCommunicationState(CO_Data* d, 
	s_state_communication *newCommunicationState);
	
/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/



/*
*********************************************************************************************************
*                                          getState()
*
* Description : 
*
* Argument(s) : 
*
* Return(s)   : none.
*
*
*********************************************************************************************************
*/
e_nodeState getState(CO_Data* d)
{
  return d->nodeState;
}

/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param m                                                                                        
**/  
void canDispatch(CO_Data* d, Message *m)
{                 
        UNS16 cob_id = UNS16_LE(m->cob_id);
      
	 switch(cob_id >> 7)
	{
		case SYNC:		/* can be a SYNC or a EMCY message */
			if(cob_id == 0x080)	/* SYNC */
			{
				if(d->CurrentCommunicationState.csSYNC)
					processSYNC(d);
			} 
                        else 		/* EMCY */
				if(d->CurrentCommunicationState.csEmergency)
					proceedEMCY(d,m);
			break;
		case RMBOOT: 
                  {
                    ProcessRMBOOT(d,m);
                    break;
                  }
		case PDO1tx:
		case PDO1rx:
		case PDO2tx:
		case PDO2rx:
		case PDO3tx:
		case PDO3rx:
		case PDO4tx:
		case PDO4rx:
                case PDO6rx:  // custom PDO from BP2
			if (d->CurrentCommunicationState.csPDO || (Control_SystemControl & 0x40 == 0x40))
                        {
                            if( (cob_id & 0xFF0 ) ==  0x490 ) // test for HS PDO
                            {
                              processHSPDO(d,m);
                            }
                            else
                            {
                              processPDO(d,m);
                            }
                        }
			break;
		case SDOtx:
		case SDOrx:                     
			if (d->CurrentCommunicationState.csSDO)
                        {
				processSDO(d,m);
                                processCANGateway();
                        }
                                
			break;
		case NODE_GUARD:
			//if (d->CurrentCommunicationState.csHeartbeat || !(*(d->iam_a_slave))) ALWAYS do heartbeat	
                            processNODE_GUARD(d,m);
			break;
		case NMT: // NMT commands should not be processed coming in from the CAN link.
                        // cause a reset to enter bootloader
                        if ( ((m->data[1]== 0x00) || (m->data[1]== getNodeId(d))) &&
                             (m->data[0]== NMT_Enter_Bootloader) )
                        {  
                          
                          //while(1);
                        }    
                        // not called since PM is a master -> use "ProcessNMTLocalStateChange"
			if (*(d->iam_a_slave))  
			{
				//processNMTstateChange(d,m);
			}
#ifdef CO_ENABLE_LSS
		case LSS:
			if (!d->CurrentCommunicationState.csLSS)break;
			if ((*(d->iam_a_slave)) && cob_id==MLSS_ADRESS)
			{
				proceedLSS_Slave(d,m);
			}
			else if(!(*(d->iam_a_slave)) && cob_id==SLSS_ADRESS)
			{
				proceedLSS_Master(d,m);
			}
			break;
                        
                
#endif
	} // end CASE statement
        
     
}

#define StartOrStop(CommType, FuncStart, FuncStop) \
  if(newCommunicationState->CommType && d->CurrentCommunicationState.CommType == 0){\
		MSG_WAR(0x9999,#FuncStart, 9999);\
		d->CurrentCommunicationState.CommType = 1;\
		FuncStart;\
	}\
        else if(!newCommunicationState->CommType && d->CurrentCommunicationState.CommType == 1){\
		MSG_WAR(0x9999,#FuncStop, 9999);\
		d->CurrentCommunicationState.CommType = 0;\
		FuncStop;\
	}
#define None

/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param newCommunicationState                                                                    
**/  	
void switchCommunicationState(CO_Data* d, s_state_communication *newCommunicationState)
{
#ifdef CO_ENABLE_LSS
	StartOrStop(csLSS,	startLSS(d),	stopLSS(d))
#endif
	StartOrStop(csSDO,	None,		resetSDO(d))
	StartOrStop(csSYNC,	startSYNC(d),		stopSYNC(d))
	StartOrStop(csHeartbeat,	heartbeatInit(d),	heartbeatStop(d))
	StartOrStop(csEmergency,	emergencyInit(d),	emergencyStop(d)) 
	StartOrStop(csPDO,	PDOInit(d),	PDOStop(d))
	StartOrStop(csBoot_Up,	None,	slaveSendBootUp(d))
}

/*!                                                                                                
** @brief This function controls the Network managment state machine.                                                                                                
**                                                                                                 
** @param d                                                                                        
** @param newState                                                                                 
**                                                                                                 
** @return                                                                                         
**/  
/******************************
the following states are set for the NNP NMT Master. Not following this state pattern leads to unnecessary control traffic
and/or unpredicatable behavior.
Communication state structure 
        INTEGER8 csBoot_Up   - always CLEAR;
	INTEGER8 csSDO       - always SET;
	INTEGER8 csEmergency - always SET;
	INTEGER8 csSYNC      - set by Init function and can be overwritten by NMT command set;
	INTEGER8 csHeartbeat - always SET;
	INTEGER8 csPDO       - set by Init function and can be overwritten by NMT command set;;
	INTEGER8 csLSS       - always CLEAR;
********************************/

UNS8 setState(CO_Data* d, e_nodeState newState)
{
	if(newState != d->nodeState)
        {
		switch( newState )
                {
			case Mode_Patient_Control:
			{
				s_state_communication newCommunicationState = {0, 1, 1, 1, 1, 1, 0};
				d->nodeState = Mode_Patient_Control;
				switchCommunicationState(d, &newCommunicationState);
                                
				
							
			}
			break;				
			case Mode_X_Manual:
			{
				
                          s_state_communication newCommunicationState = {0, 1, 1, 1, 1, 1, 0}; //No PDO
				d->nodeState = Mode_X_Manual;
				switchCommunicationState(d, &newCommunicationState);
				
			}
                        break;
                        case Mode_Y_Manual:
			{
				
				s_state_communication newCommunicationState = {0, 1, 1, 1, 1, 1, 0}; //JML: enabled PDO to support PDO based stim updates
				d->nodeState = Mode_Y_Manual;
				switchCommunicationState(d, &newCommunicationState);
				
 
			}
			break;
								
			case Waiting:
			
			{
				s_state_communication newCommunicationState = {0, 1, 1, 0, 1, 0, 0}; //No Sync, No PDO
				d->nodeState = Waiting;
				switchCommunicationState(d, &newCommunicationState);
				(*d->waiting)(d);
                                
                                memset(StimY, 0, sizeof(StimY)); //clear StimY (3212.1-64)
			}
			break;
						
			case Stopped:
			{
				s_state_communication newCommunicationState = {0, 1, 1, 0, 1, 0, 0}; //No Sync, No PDO
				d->nodeState = Stopped;
				switchCommunicationState(d, &newCommunicationState);
				(*d->stopped)(d);
			}
			break;
                        
                        case Mode_Patient_Manual:
			{
				s_state_communication newCommunicationState = {0, 1, 1, 1, 1, 1, 0}; 
				d->nodeState = Mode_Patient_Manual;
				switchCommunicationState(d, &newCommunicationState);
                               
				
							
			}
			break;
                        case Mode_Produce_X_Manual:
			{
				s_state_communication newCommunicationState = {0, 1, 1, 1, 1, 1, 0};
				d->nodeState = Mode_Produce_X_Manual;
				switchCommunicationState(d, &newCommunicationState);
                               
				/* call user app init callback now. */
							
			}
			break;
                        case Mode_Record_X:
			{
                          s_state_communication newCommunicationState = {0, 1, 1, 1, 1, 1, 0};  //JML:enabled SYNC in Record_X to allow stim
				d->nodeState = Mode_Record_X;
				switchCommunicationState(d, &newCommunicationState);
                               
				/* call user app init callback now. */
							
			}
			break;
                        case Mode_Charging:
			{
				s_state_communication newCommunicationState = {0, 1, 1, 0, 1, 0, 0}; //No Sync
				d->nodeState = Mode_Charging ;
				switchCommunicationState(d, &newCommunicationState);
							
			}
			break;
			default:
				return 0xFF;

		}/* end switch case */
	
	}
	/* d->nodeState contains the final state */
	/* may not be the requested state */
    return d->nodeState;  
}

/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
**                                                                                                 
** @return                                                                                         
**/ 
UNS8 getNodeId(CO_Data* d)
{
  return *d->bDeviceNodeId;
}


/*!                                                                                                
**                                                                                                 
**                                                                                                 
** @param d                                                                                        
** @param nodeId                                                                                   
**/   
void setNodeId(CO_Data* d, UNS8 nodeId)
{
  UNS16 offset = d->firstIndex->SDO_SVR;
  
#ifdef CO_ENABLE_LSS
  d->lss_transfer.nodeID=nodeId;
  if(nodeId==0xFF)
    {
          *d->bDeviceNodeId = nodeId;
          return;
    }
    else
  #endif
    if(!(nodeId > 0 && nodeId <= 127))
    {
            MSG_WAR(0x2D01, "Invalid NodeID",nodeId);
            return;
    }

  if(offset){
    /* Adjust COB-ID Client->Server (rx) only id already set to default value or id not valid (id==0xFF)*/
    if((*(UNS32*)d->objdict[offset].pSubindex[1].pObject == 0x600 + *d->bDeviceNodeId)||(*d->bDeviceNodeId==0xFF)){
      /* cob_id_client = 0x600 + nodeId; */
      *(UNS32*)d->objdict[offset].pSubindex[1].pObject = 0x600 + nodeId;
    }
    /* Adjust COB-ID Server -> Client (tx) only id already set to default value or id not valid (id==0xFF)*/
    if((*(UNS32*)d->objdict[offset].pSubindex[2].pObject == 0x580 + *d->bDeviceNodeId)||(*d->bDeviceNodeId==0xFF)){
      /* cob_id_server = 0x580 + nodeId; */
      *(UNS32*)d->objdict[offset].pSubindex[2].pObject = 0x580 + nodeId;
    }
  }

  /* 
   	Initialize the server(s) SDO parameters
  	Remember that only one SDO server is allowed, defined at index 0x1200	
 		
  	Initialize the client(s) SDO parameters 	
  	Nothing to initialize (no default values required by the DS 401)	
  	Initialize the receive PDO communication parameters. Only for 0x1400 to 0x1403 
  */
  {
    UNS8 i = 0;
    UNS16 offset = d->firstIndex->PDO_RCV;
    UNS16 lastIndex = d->lastIndex->PDO_RCV;
    UNS32 cobID[] = {0x200, 0x300, 0x400, 0x500};
    if( offset ) while( (offset <= lastIndex) && (i < 4)) {
      if((*(UNS32*)d->objdict[offset].pSubindex[1].pObject == cobID[i] + *d->bDeviceNodeId)||(*d->bDeviceNodeId==0xFF))
	      *(UNS32*)d->objdict[offset].pSubindex[1].pObject = cobID[i] + nodeId;
      i ++;
      offset ++;
    }
  }
  /* ** Initialize the transmit PDO communication parameters. Only for 0x1800 to 0x1803 */
  {
    UNS8 i = 0;
    UNS16 offset = d->firstIndex->PDO_TRS;
    UNS16 lastIndex = d->lastIndex->PDO_TRS;
    UNS32 cobID[] = {0x180, 0x280, 0x380, 0x480};
    i = 0;
    if( offset ) while ((offset <= lastIndex) && (i < 4)) {
      if((*(UNS32*)d->objdict[offset].pSubindex[1].pObject == cobID[i] + *d->bDeviceNodeId)||(*d->bDeviceNodeId==0xFF))
	      *(UNS32*)d->objdict[offset].pSubindex[1].pObject = cobID[i] + nodeId;
      i ++;
      offset ++;
    }
  }

  /* Update EMCY COB-ID if already set to default*/
  if((*d->error_cobid == *d->bDeviceNodeId + 0x80)||(*d->bDeviceNodeId==0xFF))
    *d->error_cobid = nodeId + 0x80;

  /* bDeviceNodeId is defined in the object dictionary. */
  *d->bDeviceNodeId = nodeId;
}

void _mode_X_Manual(CO_Data* d){}
void _mode_Y_Manual(CO_Data* d){}
void _waiting(CO_Data* d){}
void _stopped(CO_Data* d){}
void _mode_Patient_Manual(CO_Data* d){}
void _mode_Patient_Control(CO_Data* d){}


