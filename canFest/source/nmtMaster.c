/*
This file is part of CanFestival, a library implementing CanOpen
  Stack.

  Copyright (C): Edouard TISSERANT and Francis DUPIN
  Modified by JDCC.

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
** @file   nmtMaster.c
** @author Edouard TISSERANT and Francis DUPIN
** @date   Tue Jun  5 08:47:18 2007
**
** @brief
**
**
*/

#include "nmtMaster.h"
#include "canfestival.h"
#include "sysdep.h"
#include "bsp.h"
#include "scripts.h"
#include "SPI_Memory.h"
#include "sys.h"
#include "cc1101radio.h"
#include "pwrnet.h"
#include "can_cfg.h"
#include "drv_can_reg.h"
#include "can_bus.h"
#include "can_cpu.h"
#include "ScriptInterpreter.h"
#include "File_Operations.h"
#include "runcanserver.h"
#include "batPack.h"

CPU_INT08U networkOnState = 0;



/*!
**
**
** @param d
** @param Node_ID
** @param cs
**
** @return
**/
UNS8 masterSendNMTstateChange(CO_Data* d, UNS8 Node_ID, UNS8 cs[3])
{
  Message m;
  
  //Important, if the network is not active, do not send a CAN message, or the 
  //low level CAN HW will hang
  if( !IS_PWR_NETWORK_ENABLED() )
    return 0;
  
  /* message configuration -- allow for two parameters*/
  m.cob_id = 0x0000; /*(NMT) << 7*/
  m.rtr = NOT_A_REQUEST;
  m.len = 4;
  m.data[0] = cs[0];
  m.data[1] = Node_ID;
  m.data[2] = cs[1];
  m.data[3] = cs[2];

  return canSend(d->canHandle,&m);
}


/*!
**
**
** @param d
** @param nodeId
**
** @return
**/
UNS8 masterSendNMTnodeguard(CO_Data* d, UNS8 nodeId)
{
  Message m;

  /* message configuration */
  UNS16 tmp = nodeId | (NODE_GUARD << 7); 
  m.cob_id = UNS16_LE(tmp);
  m.rtr = REQUEST;
  m.len = 1;

  MSG_WAR(0x3503, "Send_NODE_GUARD to node : ", nodeId);

  return canSend(d->canHandle,&m);
}

/*!
**
**
** @param d
** @param nodeId
**/
void masterRequestNodeState(CO_Data* d, UNS8 nodeId)
{
  /* FIXME: should warn for bad toggle bit. */

  /* NMTable configuration to indicate that the master is waiting
    for a Node_Guard frame from the slave whose node_id is ID
  */
  d->NMTable[nodeId] = Unknown_state; /* A state that does not exist
                                       */

  if (nodeId == 0) { /* NMT broadcast */
    UNS8 i = 0;
    for (i = 0 ; i < NMT_MAX_NODE_ID ; i++) 
    {
      d->NMTable[i] = Unknown_state;
    }
  }
  masterSendNMTnodeguard(d,nodeId);
}
void MasterResetNodeTable(CO_Data* d )
{
  UNS8 i = 0;
  for (i = 0 ; i < ACTIVE_NODE_COUNT ; i++) 
    {
      if ( i == getNodeId(d))
        continue;
      
      d->NMTable[i] = Unknown_state;
    }
  
 return; 

}
void MasterRequestNodeTable(CO_Data* d, UNS8* temp)
{
  UNS8 i = 0;
  for (i = 0 ; i < ACTIVE_NODE_COUNT ; i++) 
    {
      temp[i] = (UNS8)d->NMTable[i];
    }
  
 return; 

}

/*
*********************************************************************************************************
*                                  ProcessNMTLocalStateChange(CO_Data* d, UNS8 Data[3])
*
* Description : Use the Vendor interface to the USB device stack.
*
* Argument(s) : d               Pointer to Object Dictionary Data structure
*
*               Data[]          Command, and Param1, Param2
*
* Return      : Void
* 
* Note(s)     : This function is the command interpreter for the system and accepts NMT messagess. 
*               It changes system modes and invokes various behaviors (functions).
*********************************************************************************************************
*/

void ProcessNMTLocalStateChange(CO_Data* d, UNS8 Data[3] )
{
  UNS8 Command = Data[0];
  UNS8 Param1   = Data[1];
  UNS8 Param2   = Data[2];
  OS_ERR err;
  
  /* Command formatting is slightly different than what exists in the remote modules.
  * The difference lies in where the NMT message originates from - the radio (or USB) link,
  * or the CAN bus. If the message reaches this point (ie this function), then it has been
  * filtered for the correct address. */
  
  switch( Command )
  { /* NMT command specifier (cs) in def.h*/
    
  case NMT_Start_Nodes:
    if ( d->nodeState == Hibernate) 
      setState(d,Waiting);
    else
      setState(d,Unknown_state);
    break;
    
  case NMT_Stop_Nodes:
    setState(d,Stopped);
    EnableTPDOs(0);
    break;
    
  case NMT_Enter_Wait_Mode:  // no restrictions on entering Waiting
    setState(d,Waiting);
    EnableTPDOs(0);
    StopWatchDog( d );
    if (networkOnState)
    {
      networkOnState = 0;
//      NetworkPowerControl |= (1 << 0); // turn on network
//      if( updatePowerNetwork() )
//        WakeRemoteModule( 0 );  // wakes the remote nodes from BL 
      
    }
    break;
    
  case NMT_Enter_Patient_Operation:
    if (d->nodeState == Waiting)
    {
      setState(d, Mode_Patient_Control);
      Control_CurrentGroup = Param1;
    }
    break;  
    
  case NMT_Enter_X_Manual:
    if (d->nodeState == Waiting )
    {
      setState(d,Mode_X_Manual);
      Control_CurrentGroup = Param1;
      StartWatchDog(d, 10000);
    }
    break;
    
  case NMT_Enter_Y_Manual:
    if (d->nodeState == Waiting)
    {
      EnableTPDOs(Param1);
      setState(d,Mode_Y_Manual); 
    }
    break;
    
  case NMT_Enter_Patient_Manual:
    if (d->nodeState == Waiting)
    {
      setState(d,Mode_Patient_Manual);
      Control_CurrentGroup = Param1;
    }
    break;
  case NMT_Enter_Produce_X_Manual:
    if (d->nodeState == Waiting)
    {
      EnableTPDOs(Param1);
      setState(d,Mode_Produce_X_Manual);
      Status_NumberOfSyncs = Param2;
    }
    break;
    
  case NMT_Enter_Stop_Stim:  // no restriction on entering Stopped
    setState(d,Stopped);
    break;
    
  case NMT_Enter_Record_X:
    if (d->nodeState == Waiting ) // going back to Waiting will turn off Recording
    {           
      TransferBuffer_Counter = 0;
      setState(d, Mode_Record_X);          
      RunHighSpeedPassThru(Param1);
      
    }
    break;  
  // enable coil task  
  case NMT_Enter_Charging_Mode:
    {           
      setState(d, Mode_Charging); 
      if ((NetworkPowerControl & 0x01) == 0x01)
      {
        NetworkPowerControl &= ~(1 << 0); // turn off network
        networkOnState = 1;
      }
    }
    break;  
    
  case NMT_Do_Save_Cmd:  
    {   
      SaveValues();       
      break;
    }
  case NMT_Do_Restore_Cmd:  
    if (d->nodeState == Waiting)
    {
      RestoreValues();
    }
    break;
    
  case NMT_Init_NV_Memory:
    if (d->nodeState == Waiting)
    {
      
    }
    break;
    
  case NMT_Network_On_Only:
    {
      if (d->nodeState == Waiting || d->nodeState == Stopped)
      {
        NetworkPowerControl |= 0x01; // turn on Bit 0
        updatePowerNetwork();
      }
    }
    break;
    
  case NMT_Network_On:
    if (d->nodeState == Waiting || d->nodeState == Stopped)
    {
      if (Param1 == 0x20)
      {
        Control_SystemControl |= 0x01; // set bit 0
        SaveValues();
      }
      
      NetworkPowerControl |= 0x01; // turn on Bit 0
      if( updatePowerNetwork() )
        WakeRemoteModule( 0 );  // wakes the remote nodes from BL 
      
    }
    break;
    
  case NMT_Network_Off:
    if (d->nodeState == Waiting || d->nodeState == Stopped )
    {
      if (Param1 == 0x20)
      {
        Control_SystemControl &= ~0x01;
        SaveValues();
      }
      NetworkPowerControl &= ~(0x01); // turn off Bit 0
      if( !updatePowerNetwork() )
        MasterResetNodeTable(d ); //resets all nodes to "Unknown State" in PM node table
    }
    break;
    
  case NMT_Init_CAN:
    {
      if (d->nodeState == Waiting || d->nodeState == Stopped )
      {        
        canReset();
      }
    }
    break;
    
  case NMT_Reset_Watchdog:
    {
      StartWatchDog(d, 10000);
      break;  
    }
  case NMT_Reset_Node:
    {
      if(d->NMT_Slave_Node_Reset_Callback != NULL)
        d->NMT_Slave_Node_Reset_Callback(d);
      setState(d,Waiting);
      break;
    }
  case NMT_Start_Sync:
    {
      startSYNC(d);
      break;
    }
  case NMT_Stop_Sync:
    {
      stopSYNC(d);
      break;
    }
  case NMT_Start_PDO:
    {
      Control_SystemControl |= (1 << 6); // manual override of PDO state
      Control_SystemControl |= (1 << 3); // compile bytes to byte array in OD
      PDOInit(d);
    break;
    }
  case NMT_Stop_PDO:
    {
      Control_SystemControl &= ~(1 << 6); 
      Control_SystemControl &= ~(1 << 3); 
      PDOStop(d);
      break;
    }
    
  case NMT_Set_RTC:
    {
      RTC_SetClock(); /* in bsp.c */
      //SaveValues(); //JML DOn't save values here.  
      //This takes extra time and is unnecessary because the clock will have lost its time anyway
      break;
    }
    
  case NMT_Halt_RTC:
    {
      RTC_HaltClock(); /* in bsp.c */
      break; 
    }
    
  case NMT_Set_Alarm:
    {
      RTC_SetAlarm(); /* in bsp.c */
      break;
    }
  case NMT_Turn_OFF_Alarms:
    {
      RTC_Turn_OFF_Alarms(); /* in bsp.c */
      break;
    }
  case NMT_Radio_WOR_ON:
    {
      RADIO_WakeInterval = (((UNS16)Param2)<<8) + Param1;
      enableRadio_WOR();
      break;
    }
    
  case NMT_Radio_WOR_OFF:
    {
      RADIO_WakeInterval = 0;
      disableRadio_WOR();
      break;
    }
  case NMT_Enable_Scripts:
    {
      Scripts_Enabled();
      //SaveValues();
      break;
    }
  case NMT_Disable_Scripts:
    {
      Scripts_Disabled();
      //SaveValues();
      break;
    }
  case NMT_Run_Script:
    {     
      Start_Script(Param1);
      //SaveValues();
      break;
    }
  case NMT_Stop_Script:
    {
      Stop_Script(Param1);
      //SaveValues();
      break;
    }
  case NMT_Run_Script_Once:
    {
      RunOnceScript(Param1, Param2);
      break;
    }
  case NMT_Abort_Scripts:
    {
      AbortAllScripts(TRUE);
      break;
    }
  case NMT_Delete_Script:
    {
      EraseScriptSegment(Param1);
      SaveValues(); //save cleared script pointer in restore list
      break;
    }
  case NMT_Set_Script_Delay_Time:
    {
      Control_ScriptDelayTime = Param1;
      //SaveValues();
      break;
    }  
  case NMT_Clear_Startup_Script:
    {
      Control_StartUp_ScriptPointer = 0;
      SaveValues(); //save cleared script pointer in restore list
      break;
    }
  case NMT_Clear_Shutdown_Script:
    {
      Control_ShutDown_ScriptPointer = 0;
      SaveValues(); //save cleared script pointer in restore list
      break;
    }
  case NMT_Radio_Address:  // note that the CT address and the PM address are 
    // reversed here.
    {                                  
      WriteRadioConfig();                    
      initRadioConfig();  
      break;    
    }
    
  case NMT_Set_Radio_Power:
    {
      RADIO_TXPower = Param1;
      initRadioConfig();
      
      if (Param2 == 0xFE) 
        WriteRadioConfig(); // commit to eeprom
      break;
    }
  case NMT_Wake_Remote_Modules:
    {
      if( updatePowerNetwork() )
        WakeRemoteModule( Param1 );  // wakes the remote nodes from BL 
      break;
    }
  case NMT_Clear_CAN_Errors:
    {
      CAN_TotalErrors = 0; 
      CAN_BitErrors = 0;
      CAN_StuffErrors = 0;
      CAN_FormErrors = 0;
      CAN_OtherErrors = 0;
      CAN_Receive_BEI = 0;
      CAN_Receive_Messages = 0;
      CAN_Transmit_Messages = 0;
      CAN_TX_ResetCounter = 0;
      CAN_RX_ResetCounter = 0;
      CAN_NS_ResetCounter = 0;           
      break;    
    }
  case NMT_Reset_Radio_Counters:
    {
      RADIO_Hard_Bad_CRC = 0x00;
      RADIO_TX_Sent = 0x00;
      RADIO_RX_Rec = 0x00;
      RADIO_RX_Rec_Partial = 0x00;
      
      break; 
    }
    
  case NMT_Reset_OD_Defaults: 
    {
      if (d->nodeState == Waiting || d->nodeState == Stopped )
        ResetToODDefault();
      break;
    }
  case NMT_Reset_Global_Vars:
    {     
      if(LoadGlobalVarTable( Param1 )) //Param1=0 loads all 
      { 
        ScriptDebug_statusByte = SCRIPT_ERR_RESET_GLOBALS;  
        Scripts_Disabled();
      }
      break;
    }
    
  case NMT_Start_Script_Debug:
    {
      StartScriptDebug( Param1, Param2 );
      break;
    }
  case NMT_Stop_Script_Debug:
    {
      StopScriptDebug();
      break;
    }
  case NMT_Single_Step_Debug:
    {
      SingleStepScriptDebug();
      break;
    }
  case NMT_Flush_Log_Files:
    {
      ClearLogfile(Param1);
      break;
    }
  case NMT_Init_File_System:
    {
      InitFiles(2);
      break;
    }
  case NMT_Enable_Encryption:
    {
      EnableEncryption(Param1);
      break;
    }
  case NMT_Erase_PM_Blocks:
    {
      if (Status_RemoteFlashBlock == 0)
        Status_RemoteFlashBlock = Param1 + (Param2 * 256);
      break;
    }
  case NMT_BlankCheck_PM_Flash_Mem:
    {
      BlankCheckRemoteFlash();
      break;
    }
  case NMT_Change_Task_Priority:
    {
     
      break;
    }
  case NMT_Change_Watchdog_Time:
    {
      //CPU_INT32U tempTime = Param1 * 1000000; // unit in seconds
      
      break;
    }
  case NMT_Reset_Module: 
    {      
      Reset_Module();
      break;
    }
    
  case NMT_Enter_Low_Power:
    {
      if(Param1 == 1) //not recommended but useful for power testing
      {
        Enter_Low_Power(1); //power down
      }
      else
      {
        if (d->nodeState == Waiting || d->nodeState == Stopped || d->nodeState == Mode_Charging)
        {
          if(!(BatteryControl_LowPowerStatus & BIT7)) //if low power is not enabled
          {
            BatteryControl_LowPowerStatus |= BIT7; //enable low power mode
            
            if(BatteryControl_LowPowerStatus & BIT4) //if radio is already pending (i.e. if NMT called from scripts)
            {
              if(BatteryControl_LowPowerStatus & BIT3) //is sleep task is pending?
              {
                BatteryControl_LowPowerStatus |= BIT1; //set source as script
                OSSemPost(&SleepSem, OS_OPT_POST_1, &err);
              }
            }
            //otherwise, radio gateway will put to sleep
          }
        }    
      }
    }
    break;
  case NMT_Exit_Low_Power:
    {
      BatteryControl_LowPowerStatus &= ~BIT7; //don't reenter Low Power mode from any source          
    }
    break;
  case NMT_Set_VSYS:
    {
        setBattVSYS(Param1);     
    }
    break;
  case NMT_Read_Memory_Now:
    if (d->nodeState == Waiting)
    {
      ReadMemoryWithIncrement(Param1);
    }
    break; 
  case NMT_Read_Script_Lengths:
    for(UNS8 i=0; i<MAX_NUMBER_SCRIPTS; i++)
    {
      Script_Management[i] = readScriptLength(i+1);
    }
    break;
  case NMT_Read_Script_IDs:
    for(UNS8 i=0; i<MAX_NUMBER_SCRIPTS; i++)
    {
      Script_Management[i] = (UNS16) readScriptID(i+1);
    }
    break;
  case NMT_Read_Script_Revs:
    for(UNS8 i=0; i<=MAX_NUMBER_SCRIPTS; i++)
    {
      Script_Management[i] = readScriptRev(i+1);
    }
    break;
  case NMT_Read_Script_CRCs:
    for(UNS8 i=0; i<MAX_NUMBER_SCRIPTS; i++)
    {
      Script_Management[i] = readScriptCRC(i+1);
    }
    break;  
  case NMT_Calculate_Script_CRCs:
    for(UNS8 i=0; i<MAX_NUMBER_SCRIPTS; i++)
    {
      Script_Management[i] = calculateScriptCRC16(i+1);
    }
    break;
 
    //       case NMT_Reset_Comunication:
    //          {            
    //              UNS8 currentNodeId = getNodeId(d);
    //             
    //                if(d->NMT_Slave_Communications_Reset_Callback != NULL)
    //                   d->NMT_Slave_Communications_Reset_Callback(d);
    //        #ifdef CO_ENABLE_LSS
    //                // LSS changes NodeId here in case lss_transfer.nodeID doesn't 
    //                // match current getNodeId()
    //                if(currentNodeId!=d->lss_transfer.nodeID)
    //                   currentNodeId = d->lss_transfer.nodeID;
    //        #endif
    //    
    //                // clear old NodeId to make SetNodeId reinitializing
    //                // SDO, EMCY and other COB Ids
    //                *d->bDeviceNodeId = 0xFF; 
    //             
    //                setNodeId(d, currentNodeId);
    //             
    //          }
    //            d->NMTable[getNodeId(d)] = getState(d);
    //break;
  //case NMT_StartChannelLoop:
      //enableRadio_ChannelLoop();
  //    break;
    
  //case NMT_StopChannelLoop: 
      //disableRadio_ChannelLoop();
   //   break;
      
  case NMT_Set_nBOOT_HighOut:
    IO0DIR |= BIT14;  //nBOOT as output 
    IO0SET  = BIT14; //nBOOT high
    break;
    
  case NMT_Set_nBOOT_LowOut:
    IO0DIR |= BIT14;  //nBOOT as output 
    IO0CLR  = BIT14; //nBOOT low
    break;
    
  case NMT_Set_nBOOT_In:
    IO0DIR &= ~BIT14;  //nBOOT as output 
    break;
    
  case NMT_Enable_Gyro:
    enableGyro();
    break;
    
  case NMT_Disable_Gyro:
    disableGyro();
    break;
    
  }/* end switch */
}




