// Doxygen
/*!
** @file   runCANserver.c
** @author Coburn
** @date   2/14/2010
**
** @brief Task is set up to be an IO scanner. It is part of the OS While loop
** in app.c -- App_TaskStart(). It also initializes the CAN node for the Power
** Module.
** @ingroup canscanner
**
*/      

#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "bsp.h"
#include "sys.h"
#include "canfestival.h"
#include "can_cfg.h"
#include "can_cpu.h"
#include "objdict.h"
#include "RunCANServer.h"
#include "accel.h"
#include "caseTherm.h"
#include "batPack.h"
#include "pwrnet.h"
#include "SPI_Memory.h"
#include "gateway.h"
#include "drv_can_reg.h"
#include "i2c.h"
#include "File_Operations.h"







// -------- DEFINITIONS ----------
#define MAX_CASE_TEMPERATURE 400 //in 0.1 deg C


// --------  Static DATA   ------------
// canOpen
static Message m = Message_Initializer;


/******************************************/
/*           Data                  */
/******************************************/

//^^sometimes cast to UINT32; compiler seemed to do this, but making sure
#pragma data_alignment=4		

unsigned char nodeID= 7;
UNS16 n = 0;

OS_SEM CanTimerSem;
OS_SEM SleepSem;


/******************************************/
/*           Prototypes                   */
/******************************************/
void checkCaseTemperature( void );

/******************************************/
/*           Tasks                   */
/******************************************/

/**
@brief Initialize task that handles incoming CAN messages 
*/
void InitCANServerTask( void )
{
	
        /*^^ canOpen initialization */
	setNodeId (&ObjDict_Data, nodeID);
        
        
        /* Used to set the CAN server into operational mode; 
         * must run through sequence                        */
        
	
        setState(&ObjDict_Data, Stopped);
        setState(&ObjDict_Data, Waiting); 	
        
}


/**@brief Task for handling incoming CAN messages 
*/
void RunCANServerTask(void)
{
     
    /* task loop */
     while(DEF_TRUE)
    {
      /*Note: canReceive may be called while the CAN peripheral is disabled, as long as 
      canInit() has been called before this task is created.  
      The current task will only continue once CAN is enabled and a CAN message is received*/

      if (canReceive( &m )) 	 	
      {
			             
        canDispatch( &ObjDict_Data, &m );	     
      }
      
      
    }
    
}

void RunCANTimerTask(void)
{
    CPU_TS ts;
    OS_ERR err;
    
    /* task loop */
     while(DEF_TRUE)
    {
        OSSemPend(&CanTimerSem, 0, OS_OPT_PEND_BLOCKING, &ts, &err); 
        TimeDispatch();
    }
}
 
void SleepTask( void )
{
  CPU_TS ts;
  OS_ERR err;
    
  UINT8 lowBatCounter = 0;
  UINT16 loopCounter = 0;
  
  UINT8  sec = SEC;

  while(DEF_TRUE)
  {
    
    //The PM won't pend on the SleepSem until it wakes up, at which point the RTC or radio will have already posted
    BatteryControl_LowPowerStatus |= BIT3; //BatteryControl_LowPowerStatus BIT3 is also set just before idling MCU
    OSSemPend(&SleepSem, 0, OS_OPT_PEND_BLOCKING, &ts, &err); 
    BatteryControl_LowPowerStatus &= ~BIT3;
      
    if(!(BatteryControl_LowPowerStatus & BIT7)) 
    {
      continue;
    }
    
    if( sec != SEC ) //do background IOscan if not yet done this second
    {
      if (LowSystemVoltage())
      {
        if (++lowBatCounter >= 10) //requires 10 consecutive samples below threshold 
        {
          Reset_Module(); //shutdown        
        } 
      }
      else 
         lowBatCounter = 0;

       LowPowerBatteryThermScan();
    }
    sec = SEC;

    //IO0SET = BIT0; //JML DEBUG - See IOInit in app.c for debug usage    
    //wait until RadioGatewayTask, ScriptTask, and IOScanTask are pending
    loopCounter = 0;
    do{
      OSTimeDlyHMSM(0, 0, 0, 1,  OS_OPT_TIME_HMSM_STRICT,  &err); 
      
      if(loopCounter++ > 100)
      {
        BatteryControl_LowPowerStatus &= ~BIT7;
        break;  
      }
          
    }while( ( BatteryControl_LowPowerStatus & (BIT4|BIT5|BIT6) ) != (BIT4|BIT5|BIT6)  );
    //IO0CLR = BIT0; //JML DEBUG - See IOInit in app.c for debug usage
    
    //Put PM back to sleep
    if(BatteryControl_LowPowerStatus & BIT7)
      Enter_Low_Power(0);
       
  }//end while(DEF_TRUE)
}
  

/**
@brief initiaizes the hardware used in the IO scan task
*/
void InitIOScanTask(void)
{
  /* Setup A/D: 10-bit AIN0 @ 3MHz */
   ADCR   = 0x002E0401;     
   
   initTwiHdwr(); //init I2C
   
   initBatteryPack();
      
   initAccelerometer();
   
   initCaseThermometer();
   
}

/**
@brief This task runs in the background and spaces out communication to fuel gauges and thermistor.
During low power, the condensed LowPowerBatteryThermScan runs instead
*/
void RunIOScanTask(void)
{
  OS_ERR err;
  static UNS8 taskCounter = 0;
  static UNS8 lowBatCounter = 0;
  static CPU_BOOLEAN eraseAllFlag = FALSE;
  static CPU_INT08U flashEraseStopValue = 0;
  UINT32 address;
  
  while(DEF_TRUE)
  {
    BatteryControl_LowPowerStatus |= BIT6; //IOScanTask is pending
    OSTimeDlyHMSM(0, 0, 0, 100,  OS_OPT_TIME_HMSM_STRICT,  &err);  
    BatteryControl_LowPowerStatus &= ~BIT6; //IOScanTask is not pending
  
    if(BatteryControl_LowPowerStatus & BIT7)  
    {
        //implement the condensed IOScan instead 
        continue;
    }

    //If the indication isn't used for something else, set or clear TX/RX based on bits 4:7
    //BIT0 SYNC on TX, Script on RX
    //BIT1-3 reserved for future
    //BIT4 set TX
    //BIT5 set RX
    //BIT6 clear TX
    //BIT7 clear RX
    //TX = BIT0, and RX = BIT1 for IO0SET/CLR
    if( ScriptDebug_Indication!=0 && !(ScriptDebug_Indication & (BIT0|BIT1|BIT2|BIT3)))
    {
      if(ScriptDebug_Indication & (BIT4|BIT5))
        IO0SET = (ScriptDebug_Indication & (BIT4|BIT5))>>4;
      
      for(UNS16 delay = 0; delay<1000; delay++);  //add short delay here
        asm("nop");
        
      if(ScriptDebug_Indication & (BIT6|BIT7))
        IO0CLR = (ScriptDebug_Indication & (BIT6|BIT7))>>6;
    }
      
    if (taskCounter == 0)
    {
      HBT_LED = 1;
    }
    else if (taskCounter == 5)
       HBT_LED = 0;
    

    //Background task Schedule, each entry is 100ms, so full loop is 1s):
    //0: fuelgauge1 
    //1: fuelgauge2
    //2: fuelgauge3
    //3: turn on voltage divider for thermistor ADC
    //4: start thermistor ADC conversion
    //5: read thermistor (must be at least 33ms after start of ADC conversion)
    //6-8: do nothing
    //9: update Network
    //every cycle the Accelerometer (when network is off) and BatteryCharger is updated
       
    MaintainBatteryCharger();
    UpdateFuelGauges( taskCounter ); 
    UpdateBatteryODValues();
    
    updateCaseThermometer( taskCounter - 3 );        // output => Temperature (OD 2003)
    if (taskCounter ==5) //just finished reading thermistor
      checkCaseTemperature();
   
    //update for next time through task
    if( ++taskCounter > 9 )
    {
      updatePowerNetwork();  //effective 500ms rate
      taskCounter = 0;
    }

    //If network is ON, don't read Accelerometer, because readings are worthless
    //if(NetworkPowerControl & 0x01)
    //  memset( &AccelerometersPM[0], 0xFF, 4 ); //set error values
    //else
      updateAccelerometer();
    
    
    //JML: See note in SPI_Memory.c
    //ManageRemoteFlashRefresh(); // this routine refreshes all flash every 19,500 writes - backgrounc
    
    // clear status
    if (memorySelect > 0 )
      statusByteMemory = 0;
    
    // run memory reads (eeprom = 4, 8) - not implemented on PM
    if (memorySelect == 1)
      statusByteMemory = ReadLocalFlashMemory();
    else if (memorySelect == 2)
      statusByteMemory = ReadRemoteFlashMemory();
    else if (memorySelect == 3)
      statusByteMemory = ReadRemoteRAMMemory();
    else if (memorySelect == 5)
      statusByteMemory = WriteLocalFlashMemory();
    else if (memorySelect == 6)
      statusByteMemory = WriteRemoteFlashMemory();
    else if (memorySelect == 7)
      statusByteMemory = WriteRemoteRAMMemory();
    else if (memorySelect == 9)
      statusByteMemory = ReadLocalRAMMemory();
    else if (memorySelect != 0)
      statusByteMemory = 4; // no memory to process
    
    if ( memorySelect > 0 )
    {        
      memorySelect = 0;
    }

    // check for error conditions on CAN1       
    CAN_EnabledInterrupts = (CPU_INT16U)(LPC21XX_CAN_C1IER & 0xFFFF);      
    
    CPU_INT08U tempVar = Status_modeSelect & 0xFF;
    Status_modeSelect = 0;
    Status_modeSelect |= (CPU_INT32U)(tempVar & 0xFF);
    Status_modeSelect |= getState(&ObjDict_Data) << 8;
    
    if (BatteryControl_VSYS < BatteryControl_MinVSYS)
    {
      if (++lowBatCounter > 9) //requires 10 consecutive samples below threshold 
      {
        Reset_Module(); //shutdown        
      }
    }
    else
       lowBatCounter = 0;
    
    // erase remote flash - one block per scan
   
    if (Status_RemoteFlashBlock == 0xFFFF || Status_RemoteFlashBlock == 0xFFFE || eraseAllFlag)
    {     
      if (eraseAllFlag == FALSE)
      {
        if (Status_RemoteFlashBlock == 0xFFFF) // erase everything
           flashEraseStopValue = 0;
        else if(Status_RemoteFlashBlock == 0xFFFE) //erase all but block 0 (restore params)
          flashEraseStopValue = 1;

        if(blankCheckRemoteFlash(&address))
          Status_RemoteFlashBlock = (address >> 12) + 1; //ignore all pages that are already blank
        
        if(Status_RemoteFlashBlock > NV_NUM_BLOCKS) //make sure starting block is in range
          Status_RemoteFlashBlock = NV_NUM_BLOCKS;
        
        if(Status_RemoteFlashBlock > flashEraseStopValue)
        {
          eraseAllFlag = TRUE; 
          Scripts_Disabled();
          Status_NV_Flash_Status &= ~(1 << 8);
        }
      }
      
      if (eraseAllFlag && EraseRemoteFlashBlock(--Status_RemoteFlashBlock))
      {
        Status_NV_Flash_Status |= (1 << 8);
      }
      
      OSTimeDlyHMSM(0, 0, 0, 2,  OS_OPT_TIME_HMSM_STRICT,  &err); 
       
      if (Status_RemoteFlashBlock == flashEraseStopValue)
      {
        eraseAllFlag = FALSE;
        //InitFiles(0); //reinitialize file pointer
        Scripts_Enabled();
      }
    }
    else if (Status_RemoteFlashBlock > 0)  // single block 
    {
      EraseRemoteFlashBlock(Status_RemoteFlashBlock);
      Status_RemoteFlashBlock = 0;
    }
    else
    {
      eraseAllFlag = FALSE;
      Status_RemoteFlashBlock = 0;
    }

  }
  
}

void UpdateBatteryODValues()
{
    UINT32 temp32;
    INT16 AvgCurrent;
    //INT16 LMD, NAC, Capacity;
    static UINT16 BatteryUpdateCounter=0;
    
  
    BatteryUpdateCounter++;    
    
    // pack data for polling during charging
    ChargingData[0] = (CPU_INT08U)BatteryControl_VREC;
    ChargingData[1] = (CPU_INT08U)(BatteryControl_VREC >> 8);
    ChargingData[2] = (CPU_INT08U)Temperature;
    ChargingData[3] = (CPU_INT08U)(Temperature >> 8); 
    ChargingData[4] = (CPU_INT08U)BatteryUpdateCounter;
    ChargingData[5] = (CPU_INT08U)(BatteryUpdateCounter >> 8);
    ChargingData[6] = Battery1Charge_Status[6]; // reported voltage of bat
    ChargingData[7] = Battery1Charge_Status[7];
    ChargingData[8] = Battery2Charge_Status[6];
    ChargingData[9] = Battery2Charge_Status[7];
    ChargingData[10] = Battery3Charge_Status[6];
    ChargingData[11] = Battery3Charge_Status[7];
    ChargingData[12] = (CPU_INT08U)BatteryControl_ChargeNFLinkNoChargeLimit;
    ChargingData[13] = (CPU_INT08U)(BatteryControl_ChargeNFLinkNoChargeLimit >> 8);
    ChargingData[20] = BatteryControl_CommandedChargingCurrent;
    
    //Current can range from 0-1500mA (1A fuse)
    //We will report current to 0.1mA: range -15000 to 15000   (Note: (x*457)>>8 is equivalent to (x*3.57)/2.0)
    
    if(Battery1Charge_Status[4] == 0xFF && Battery1Charge_Status[5] == 0xFF)
    {
      AvgCurrent = 0xFFFF; //set error value
    }
    else
    {
      temp32 = ( ( (UINT32)Battery1Charge_Status[4] + ((UINT32)Battery1Charge_Status[5]<<8) )*457  )>>8;  
      //saturate value to what can be contained by INT16
      if(temp32 > 0x7FFF)   
        temp32 = 0x7FFF;
      //set sign based on charging/discharging bit
      if(Battery1Charge_Status[12] & BIT7) 
        AvgCurrent = (INT16)temp32;
      else
        AvgCurrent = -(INT16)temp32;
    }
    ChargingData[15] = (UINT8) (AvgCurrent>>8); //HB
    ChargingData[14] = (UINT8) AvgCurrent;      //LB
    
    if(Battery2Charge_Status[4] == 0xFF && Battery2Charge_Status[5] == 0xFF)
    {
      AvgCurrent = 0xFFFF; //set error value
    }
    else
    {
      temp32 = (  ( (UINT32)Battery2Charge_Status[4] + ((UINT32)Battery2Charge_Status[5]<<8)  )*457  )>>8;
      //saturate value to what can be contained by INT16
      if(temp32 > 0x7FFF)   
        temp32 = 0x7FFF;
      //set sign based on charging/discharging bit
      if(Battery2Charge_Status[12] & BIT7) 
        AvgCurrent = (INT16)temp32;
      else
        AvgCurrent = -(INT16)temp32;
    }
    
    ChargingData[17] = (UINT8) (AvgCurrent>>8); //HB
    ChargingData[16] = (UINT8) AvgCurrent;      //LB
    
    if(Battery3Charge_Status[4] == 0xFF && Battery3Charge_Status[5] == 0xFF)
    {
      AvgCurrent = 0xFFFF; //set error value
    }
    else
    {
      temp32 = (  ( (UINT32)Battery3Charge_Status[4] + ((UINT32)Battery3Charge_Status[5]<<8)  )*457  )>>8;  
      //saturate value to what can be contained by INT16
      if(temp32 > 0x7FFF)   
        temp32 = 0x7FFF;
      //set sign based on charging/discharging bit
      if(Battery3Charge_Status[12] & BIT7) 
        AvgCurrent = (INT16)temp32;
      else
        AvgCurrent = -(INT16)temp32;
    }
    
    ChargingData[19] = (UINT8) (AvgCurrent>>8); //HB
    ChargingData[18] = (UINT8) AvgCurrent;      //LB
   
//    //Maximum NAC and LMD is 2048mAh = 20,480.  INT16 is sufficient
//    //NAC and LMD are in 0.1 mAh.  Capacity is in %.
//    NAC = (  ( (UINT32)Battery3Charge_Status[ 8] + ((UINT32)Battery3Charge_Status[ 9]<<8)  )*457  )>>8;  
//    LMD = (  ( (UINT32)Battery3Charge_Status[13] + ((UINT32)Battery3Charge_Status[14]<<8)  )*457  )>>8; 
//    
//    if( LMD > 4000) //LMD has not yet learned actual battery capacity, but true remaining capacity can 
//                    //still be calculated, assuming 200mAh battery
//    {
//      if(LMD - NAC < 1875)  //Battery has been fully charged since passing EDV1 or has never passed EDV1 
//        Capacity = (NAC + 2000 - LMD)/20;      //(At full charge, NAC = LMD)
//      else                  //Battery has not been fully charged since passing EDV1 
//        Capacity = (NAC + 125 - (LMD>>4))/20;  //(At EDV1, NAC = LMD/16)
//    }
//    else if (LMD > 2000) //LMD has not yet learned actual battery capacity, and true remaining capacity  
//                         //is not certain 
//    {
//      //Assume lower capacity estimate (assume fully charged since passing EDV1).  
//      Capacity = (NAC + 2000 - LMD)/20;
//      //Only if out of range use higher capacity estimate
//      if (Capacity < 0 || Capacity > 100) 
//         Capacity = (NAC + 125 - (LMD>>4))/20;
//    }
//    else //LMD has learned down to actual battery capacity
//    {
//      //NAC and LMD now have a maximum value of 2000
//      Capacity = (NAC*100)/LMD;
//    }
//    
//    
//     
} 

/**
This scan takes ~15ms to execute and condenses the most items from the IOscantask
CAN errors and memory management stuff is not handled

NOTE:The Thermistor reading must be made at least 5ms after the conversion is initiated.  
This time is currently ~13ms but is dependent on I2C timing and the code placed between
updateCaseThermometer( 1 ) and updateCaseThermometer( 2 ).  Please verify Thermistor accuracy if changing this code
*/
void LowPowerBatteryThermScan( void )
{   
     HBT_LED =~ HBT_LED;
     
     updateCaseThermometer( 0 ); //turn on ADC for thermistor
     updateCaseThermometer( 1 ); //start ADC conversion for thermistor
     
     MaintainBatteryCharger(); 
     UpdateFuelGauges( 0 ); 
     UpdateFuelGauges( 1 );
     UpdateFuelGauges( 2 );
     
     updateAccelerometer();

     updateCaseThermometer( 2 ); //read ADC and turn off. MUST be AT LEAST 5ms (1/240Hz) after updateCaseThermometer(1);
     
     UpdateBatteryODValues();
}

/**
 * Only to be called during non-lowpower IO scan task
*/
void checkCaseTemperature( void )
{
  static CPU_INT08U high_temp_counter = 0;
  CPU_INT08U data[3] = {0,0,0};
  
  if (Temperature >= MAX_CASE_TEMPERATURE)
  {
    if (++high_temp_counter >= 3) //require 3 consecutive reads above temp
    {
      high_temp_counter=0;
      
      //PM is too hot: enter waiting mode, turn off Network, (but don't enter Low Power)
      data[0] = NMT_Enter_Wait_Mode;
      ProcessNMTLocalStateChange(&ObjDict_Data, data);
      
      data[0] = NMT_Network_Off;
      ProcessNMTLocalStateChange(&ObjDict_Data, data);

      //data[0] = NMT_Enter_Low_Power;
      //ProcessNMTLocalStateChange(&ObjDict_Data, data);
    }  
  }
  else
  {
    high_temp_counter=0;
  }
    
}
     