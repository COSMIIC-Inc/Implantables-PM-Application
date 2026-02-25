/*
*********************************************************************************************************
*                                              uC/OS-III
*                                        The Real-Time Kernel
*
*                    Copyright 2009-2022 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
* MODIFIED 
*********************************************************************************************************
*/
/*
*********************************************************************************************************
*
*                                         BOARD SUPPORT PACKAGE
*
*                                              NXP LPC2129 --> 
//^^mods by jayh for use with 2129
*
* Filename      : bsp.c
* Version       : V1.00
* Programmer(s) : FT/JDCC
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#define  BSP_MODULE
#include "sys.h"
#include <bsp.h>
#include "includes.h"
#include "ObjDict.h"
#include "scripts.h"
#include "canfestival.h"
#include "cc1101radio.h"
#include "gateway.h"
#include "cpuFlash.h"
#include "pwrnet.h"
#include "caseTherm.h"
#include "accel.h"
#include "RMBootloader.h"
#include "runcanserver.h"
#include "batpack.h"

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/



/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

CPU_INT32U  VIC_SpuriousInt;
CPU_INT08U scriptAlarmTable[4];
CPU_INT32U alarmValueTable[4];

volatile CPU_INT16U scriptTimeCount = 0;
volatile CPU_INT16U logTimeCount = 0;
volatile CPU_BOOLEAN runSchedulerFlag = 0;

//BatteryControl_LowPowerStatus
//Bits 0-3 are set, but not currently used by the application and could be repurposed if necessary
//bit0: source = RTC
//bit1: source = radio (or radio wasn't yet pending during call to NMT_Enter_Low_Power)
//bit2: source = NMT_Enter_Low_Power (script or overheat)
//bit3: sleep task is pending
//bit4: radio gateway is pending
//bit5: script task is pending
//bit6: IO Scan Task is pending
//bit7: low power enabled


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  BSP_Tmr0_TickInit     (void);
static  void  BSP_Tmr1_Init 	    (void);
static  void  BSP_Tmr1_ISR_Handler (void);
static  void  RTC_Alarm_ISR        (void);
void canOpenAlarm_interrupt(void);
void UpdateRTCOD(void);

/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
**                                         GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

#define APBDIV_BY_4		0
/*********************************************************************************************************
*                                             BSP_Init()
**********************************************************************************************************/
/**
* @brief  Initialize the Board Support Package (BSP).
*       Note(s)     : (1) This function SHOULD be called before any other BSP function is called.
*
* @param none
*
* @return none
*
* 
*/
void  BSP_Init (CPU_INT08U pll_val)
{
    CPU_SR  cpu_sr = 0;                                             /* used by CPU_CRITICAL_ENTER _ EXIT */
    CPU_INT08U msel, psel;
      
    if(pll_val > 6) pll_val=0; //if PLL value out of range, disable it
    
    if(pll_val > 1)
    {
     
      //choose divider 
      if(pll_val < 4) 
        psel = 2;
      else 
        psel = 1;
    
      msel = pll_val-1;
      
      CPU_CRITICAL_ENTER();
        PLLCFG = (psel << 5) | msel;      //configure PLL
        //write to PLL feed register
        PLLFEED  = 0xAA;                                            
        PLLFEED  = 0x55;
        
        PLLCON = BIT0;                    //enable PLL
        //write to PLL feed register
        PLLFEED  = 0xAA;                                            
        PLLFEED  = 0x55;
      CPU_CRITICAL_EXIT();
      
      while(!(PLLSTAT & BIT10)) //wait until lock (PLOCK = BIT10)
        asm("NOP"); 
      
      CPU_CRITICAL_ENTER();
        PLLCON |= BIT1;                   //connect PLL
        //write to PLL feed register
        PLLFEED  = 0xAA;                                            
        PLLFEED  = 0x55;
      CPU_CRITICAL_EXIT();
      
      while( !(PLLSTAT & BIT8) || !(PLLSTAT & BIT9) ) //wait until connected (PLLE = BIT8, PLLC = BIT9)
        asm("NOP"); 
      
    } 
    else
    {
      //disable PLL
      CPU_CRITICAL_ENTER();
      PLLCFG = 0;					                
      PLLCON = 0;
      
      //write to PLL feed register
      PLLFEED  = 0xAA;                                            
      PLLFEED  = 0x55;
      CPU_CRITICAL_EXIT();
    }
    

    //keep default peripheral clock divider
    APBDIV = APBDIV_BY_4;		                        
    
    //enable Memory Accelerator for flash access  (see LPC2129 User Manual) On reset, MAMTIM=7     
    MAMCR = 0x00; //disable MAM
    switch(pll_val) //set MAMTIM based on PLL (assumes 10MHz clock)
    {
      case 2:
      case 3: 
        MAMTIM = 0x02; 
        break;
      case 4:
      case 5:
        MAMTIM = 0x03; 
        break;
      case 6: 
        MAMTIM = 0x04; 
        break;
      default: 
        MAMTIM = 0x01; 
        break;
    }
    MAMCR = 0x02; //enable full MAM
    
    BSP_Tmr0_TickInit(); 
	
    BSP_Tmr1_Init();
    
    
    HBT_LED = 1;
   
    
}



/*********************************************************************************************************
*                                            BSP_CPU_ClkFreq()
**********************************************************************************************************/
/**
* @brief Get the CPU clock frequency.
* @param none
* @return The CPU clock frequency, in Hz.
*/
CPU_INT32U  BSP_CPU_ClkFreq (void)
{ 
  return (BSP_BOARD_CCLK_FREQ);
}


/**********************************************************************************************************
*                                            BSP_CPU_PclkFreq()
**********************************************************************************************************/
/**
* @brief Get the peripheral clock frequency.
* @param none
* @return The peripheral clock frequency, in Hz.
*/

CPU_INT32U  BSP_CPU_PclkFreq (void)
{
  //return( BSP_CPU_ClkFreq()/4 );
  return (BSP_BOARD_PCLK_FREQ);
}


/**********************************************************************************************************
*                                          OS_CPU_ExceptHndlr()
**********************************************************************************************************/
/**
* @brief  Handle any exceptions.
*  Caller(s) :   OS_CPU_ARM_EXCEPT_HANDLER(), which is declared in os_cpu_a.s.
*
* @param  except_id     ARM exception type:
*
*                                  OS_CPU_ARM_EXCEPT_RESET             0x00
*                                  OS_CPU_ARM_EXCEPT_UNDEF_INSTR       0x01
*                                  OS_CPU_ARM_EXCEPT_SWI               0x02
*                                  OS_CPU_ARM_EXCEPT_PREFETCH_ABORT    0x03
*                                  OS_CPU_ARM_EXCEPT_DATA_ABORT        0x04
*                                  OS_CPU_ARM_EXCEPT_ADDR_ABORT        0x05
*                                  OS_CPU_ARM_EXCEPT_IRQ               0x06
*                                  OS_CPU_ARM_EXCEPT_FIQ               0x07
*
* @return    none
* 
*/
void  OS_CPU_ExceptHndlr (CPU_INT32U  except_id)
{
    CPU_FNCT_VOID  pfnct;


    if (except_id == OS_CPU_ARM_EXCEPT_IRQ) 
	{
	    pfnct = (CPU_FNCT_VOID)VICVectAddr;                     /* Read the interrupt vector from the VIC                   */
        (*pfnct)();                                                 /* Execute the ISR for the interrupting device              */
        VICVectAddr = 0;                                            /* Acknowlege the VIC interrupt                             */
    } 
    else 
    {                                                               /* Infinite loop on other exceptions.                       */                                                                /* ^^ Should be replaced by other behavior (reboot, etc.)      */
        while (DEF_TRUE) ;
    }
}



/*********************************************************************************************************
*                                           BSP_IntDisAll()
**********************************************************************************************************/
/**
* @brief Disable ALL interrupts.
* @param none
* @return none
*/

void  BSP_IntDisAll (void)
{
    VICIntEnClear = 0xFFFFFFFFL;                                /* Disable ALL interrupts                                   */
	VICVectCntl0  = 0;
}




/*
*********************************************************************************************************
*********************************************************************************************************
**                                     uC/OS-III TIMER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/***********************************************************************************************************
*                                            BSP_Tmr0_TickInit
*********************************************************************************************************/
/**
* @brief Initialize uC/OS-III's tick source.
* @param none
* @return none
*
*/
static  void  BSP_Tmr0_TickInit (void)
{
    CPU_INT32U  pclk_freq;
    CPU_INT32U  tmr_reload;
                                                                /* VIC TIMER #0 Initialization                              */

    T0TCR         = 0;                                          /* Disable timer 0.                                         */
    T0TC 	  = 0;											// jtag wont clr this on reset
	
    VICIntSelect &= ~(1 << VIC_TIMER0);                         /* select as not FIQ                             */
    VICVectAddr0  = (CPU_INT32U)BSP_Tmr_TickISR_Handler;        /* Set the vector address                                   */
    VICVectCntl0  = 0x20 | VIC_TIMER0;                          /* Enable vectored interrupts                               */
    VICIntEnable  = (1 << VIC_TIMER0);                          /* Enable Interrupts                                        */


    
    // determine the interrupt time period
    pclk_freq     = BSP_CPU_PclkFreq();
    tmr_reload    = pclk_freq / OS_CFG_TICK_RATE_HZ;

    T0PC          = 0;                                          /* Prescaler is set to no division.                         */
    T0PR          = 0;

    T0MR0         = tmr_reload;
    T0MCR         = 3;                                          /* Interrupt on MR0 (reset Timer Counter register on match  */

    T0CCR         = 0;                                          /* Capture is disabled.                                     */
    T0EMR         = 0;                                          /* No external match output.                                */
    T0TCR         = 1;                                          /* Enable timer 0                                           */
}


/**********************************************************************************************************
*                                            BSP_Tmr1_Init()
*********************************************************************************************************/
/**
* @brief Initialize CANFestival counter source. 
*               No interrupt will be triggered until setTimer is called
*               or until clock rollover occurs at 34359.7 s (~9.5 hours)
*
*/
static  void  BSP_Tmr1_Init (void)
{
    
    T1TCR         = 0;                                          /* Disable timer 1.       */
    T1TC          = 0;											// jtag wont clr this on reset
	
    VICIntSelect &= ~(1 << VIC_TIMER1);                         /* select as not FIQ   */
    VICVectAddr1  = (CPU_INT32U)BSP_Tmr1_ISR_Handler;           /* Set the vector address        */
    VICVectCntl1  = 0x20 | VIC_TIMER1;                          /* Enable vectored interrupts    */
    VICIntEnable  = (1 << VIC_TIMER1);                          /* Enable Interrupts (zeros have no effect) */

    T1PC          = 0; 	                  			/* increments on every tick of peripheral clock   */
    T1PR          = BSP_BOARD_TMR1_8USEC -1; 	                /*   8usec tick*/
    
	
    T1MR0         = 0 ;		                        /* setTimer() modifies T1MR0 */
    T1MCR         = 0x01 ;                                  /* Interrupt on MR0 match but WILL NOT reset TC*/
    
    T1CCR         = 0;                                          /* Capture is disabled.        */
    T1EMR         = 0;                                          /* No external match output.   */
	
    T1TCR         = 1;                                          /* Enable timer 1              */
	
	
}


/********************************************************************************************************
*                                            incrTimer1MatchReg0()
**********************************************************************************************************/
/**
* @brief Set next match for canOpen code
* @param value 
* @return none
*
*/
void incrTimer1MatchReg0( CPU_INT32U value )
{
	// sets next match value
	T1MR0 = (T1TC + value);		
}


/*********************************************************************************************************
*                                            GetTimer1Count
*********************************************************************************************************/
/**
* @brief Return current T1 time count
* @param none
* @return timer1 count
*/
CPU_INT32U GetTimer1Count(  void )
{
	return ( T1TC );
}


/**********************************************************************************************************
*                                       BSP_Tmr_TickISR_Handler
*********************************************************************************************************/
/**
* @brief Handle the timer interrupt that is used to generate TICKs for uC/OS-III. 1 msec interval            
* @param none
* @return none
*/
void  BSP_Tmr_TickISR_Handler (void)
{                                        
   
    //CPU_ERR err;
    CPU_INT32U regVal = 0;
    
    regVal = T0IR;       // read interrupt register
    
    //IO0SET = BIT0; //JML DEBUG - See IOInit in app.c for debug usage
    // script execution control
    if (scriptTimeCount++ > Control_ScriptDelayTime) // 1msec interval
    {
      scriptTimeCount = 0;
    
      //if not in low power
      if (!(BatteryControl_LowPowerStatus & BIT7)) 
      {
        AddScriptToQueue(ROUND_ROBIN_SCRIPT);
      }
     
    }  
    CAN_UpTime++;
    OSTimeTick();
    /* Call uC/OS-III's OSTi eTick()  */

    
    //IO0CLR = BIT0; //JML DEBUG - See IOInit in app.c for debbug usage
    T0IR = 0xFF;       // Clear interrupt 
    
    VICVectAddr = 0x0; // Acknowledge that ISR has finished execution
    
}


/**********************************************************************************************************
*                                       BSP_Tmr1_ISR_Handler
*********************************************************************************************************/
/**
* @brief Handle the timer interrupt that is used for scheduling for CAN Festival 
* @param none
* @return none
*/
void  BSP_Tmr1_ISR_Handler (void)
{
    T1IR = 0xFF;                        // clear interrupt   
    canOpenAlarm_interrupt(); 
    VICVectAddr = 0x0;                  // Acknowledge that ISR has finished execution
    
  
}

/**********************************************************************************************************
*                                             RTC_Init
**********************************************************************************************************/
/**
* @brief: Initialize the Real-Time-Clock. Trigger interrupt every second.  Do not use RTC Alarm interrupts
*       Note that RTC depends on PLL setting
* @param none
* @return none
*/
void RTC_Init(void)
{  
  SEC  = 0; 
  MIN  = 0; 
  HOUR = 0;          
  DOW  = 0;
  DOM  = 1;
  MONTH= 1;	
  YEAR = 0;	
  
  
  // Enable clock
  CCR = BIT0;  
    
  VICIntSelect &= ~(1 << VIC_RTC);                         /* select as not FIQ   */
  VICVectAddr13  = (CPU_INT32U)RTC_Alarm_ISR;              /* Set the vector address        */
  VICVectCntl13  = 0x20 | VIC_RTC;                          /* Enable vectored interrupts    */
  VICIntEnable  = (1 << VIC_RTC);                          /* Enable Interrupts          */
                               
  /* load clock scaling factors */
  PREINT = ( BSP_BOARD_PCLK_FREQ / 32768 ) - 1;
  PREFRAC= BSP_BOARD_PCLK_FREQ - ((PREINT + 1 ) * 32768);
  
  AMR  = 0xFF; //Disable the alarm interrupt by masking all alarms 
  CIIR = BIT0; //Enable the Counter Increment interrupt for seconds
  
}


/***********************************************************************************************************
*                                             RTC_HaltClock
*********************************************************************************************************/
/**
* @brief Set the Real-Time-Clock
*       This must be called whenever the PM is initially powered up
* @param none
* @return none
*/
void RTC_HaltClock(void)
{
  CCR &=~ BIT0; //disable RTC to allow setting new time counter values in OD
}


/***********************************************************************************************************
*                                             RTC_SetClock
*********************************************************************************************************/
/**
* @brief Set the Real-Time-Clock
*       This must be called whenever the PM is initially powered up from a device with a preserved clock value
*       Call RTC_HaltClock before setting OD values and before calling this function
* @param none
* @return none
*/
void RTC_SetClock(void)
{
                          
  /* copy in new values and update */
  SEC  = (DateTime_Time & 0x0000003F)       ; 
  MIN  = (DateTime_Time & 0x00003F00) >>  8 ; 
  HOUR = (DateTime_Time & 0x001F0000) >> 16 ;          
  DOW  = (DateTime_Time & 0x0F000000) >> 24 ;
  DOM  = (DateTime_Date & 0x000000FF)       ;
  MONTH= (DateTime_Date & 0x0000FF00) >> 8  ;	
  YEAR = (DateTime_Date & 0xFFFF0000) >> 16 ;	
  
  CCR |= BIT0; //reenable RTC
  
  UpdateRTCOD();
  DateTime_ClockSet = 1; //indication for OD that clock has been set
}

/***********************************************************************************************************
*                                             UpdateRTCOD
*********************************************************************************************************/
/**
* @brief Updates the clock value in the OD with the value from the RTC. Called every second, via 
*               the RTC seconds increment ISR
* @param none
* @return none
*/
void UpdateRTCOD(void)
{ 
  if ( CCR & BIT0 ) //if RTC is enabled
  {
    //Update OD values
    DateTime_Time = CTIME0;
    DateTime_Date = CTIME1; 
   }  
} 

/***********************************************************************************************************
*                                             RTC_Alarm_ISR
*********************************************************************************************************/
/**
* @brief Real-Time-Clock ISR
*        triggered every second to update the OD time.
*        If the current hour,minute,second matches an enabled alarm, the attached script will be called
*        
*        This interrupt is also used to wake the PM from idle mode. If an alarm is triggered, the 
*        processor will exit the low-power mode.  If not, the PM just checks system voltage to protect against 
*        deep battery discharge, and goes back into idle mode
* 
* @todo In the future, this could make use of the RTC alarm function
* @param none
* @return none
*/
void RTC_Alarm_ISR(void)
{ 
  CPU_BOOLEAN alarm = FALSE;
  OS_ERR err;

  if(ILR & BIT0) //interrupt triggered by seconds counter increment
  {  
    UpdateRTCOD();  
    
    if ( Control_SystemControl & BIT2 ) // if alarms are globally enabled
    {
      //Every second, this loop checks the second, minute, and hour against all alarm values
      //if second, minute, and hour match, then the script is run
      for (int i = 0; i < sizeof(scriptAlarmTable); i++)
      {       
        if (scriptAlarmTable[i]) // skip if zero, otherwise look for a match
        {
          if ( SEC != (alarmValueTable[i] & 0x3F) ) 
          {
            continue;
          }
          
          if ( MIN != ((alarmValueTable[i] >> 6) & 0x3F) ) 
          {
            continue;
          }
          
          if ( HOUR != ((alarmValueTable[i] >> 12) & 0x1F) ) 
          {
            continue;
          }
          alarm = TRUE;
          AddScriptToQueue(scriptAlarmTable[i]);
        }
      }
    }
  }
  
  ILR |= BIT0 | BIT1 | BIT2; //clear the interrupt flag regardless of source
  
  if(BatteryControl_LowPowerStatus & BIT7) 
  {
    if(alarm) 
      BatteryControl_LowPowerStatus &= ~BIT7; //don't reenter Low Power mode from any source
    else
    {
      if(BatteryControl_LowPowerStatus & BIT3) //sleep task is pending
      {
        BatteryControl_LowPowerStatus |= BIT0; //sleep source is RTC
        OSSemPost(&SleepSem, OS_OPT_POST_1, &err);
      }
    }
  }

  VICVectAddr = 0x0;
 
}

/***********************************************************************************************************
*                                             RTC_SetAlarm
*********************************************************************************************************/
/** 
* @brief Set the alarm interrupt based on OD settings. Must be called to load the RTC alarm table.
*  
* @param none
* @return none
*/
void RTC_SetAlarm( void )
{
  
// check for valid script pointer. if no, then clear any time values
  if ( DateTime_Alarm1 >> 17 )
  {
    scriptAlarmTable[0] = DateTime_Alarm1 >> 17;
    alarmValueTable[0] = DateTime_Alarm1 & 0x0001FFFF;
  }
  else
    DateTime_Alarm1 = 0;
  
  if ( DateTime_Alarm2 >> 17 )
  {
    scriptAlarmTable[1] = DateTime_Alarm2 >> 17;
    alarmValueTable[1] = DateTime_Alarm2 & 0x0001FFFF;
  }
  else
    DateTime_Alarm2 = 0;
  
  if ( DateTime_Alarm3 >> 17 )
  {
    scriptAlarmTable[2] = DateTime_Alarm3 >> 17;
    alarmValueTable[2] = DateTime_Alarm3 & 0x0001FFFF;
  }
  else
    DateTime_Alarm3 = 0;
  
  if ( DateTime_Alarm4 >> 17 )
  {
    scriptAlarmTable[3] = DateTime_Alarm4 >> 17;
    alarmValueTable[3] = DateTime_Alarm4 & 0x0001FFFF;
  }
  else
    DateTime_Alarm4 = 0;
  
  Control_SystemControl |= BIT2; // enable alarms 
  
}

/*********************************************************************************************************
*                                             RTC_Turn_OFF_Alarms()
**********************************************************************************************************/
/**
* @brief Disable Alarms globally
* @param none
* @return none
*
*/
void RTC_Turn_OFF_Alarms(void)
{
  Control_SystemControl &= ~BIT2;
}



/*********************************************************************************************************
*                                             WakeRemoteModule()
**********************************************************************************************************/
/**
* @brief Called when Network goes from off -> on
*
* @param Node to wakeup.  If 0, all nodes are waken 
* @return none
*
*/
void WakeRemoteModule( int nodeNumber )
{
  Message m;

  //CPU_ERR err;
  
  m.rtr = NOT_A_REQUEST;
  
  if( !IS_PWR_NETWORK_ENABLED() )  
    return;
  
  //Note: updateNetworkVoltage includes 100ms delay after network is enabled, so no additional delay is required here
  //Delay at least 50 ms after network is active to make sure RMs are powered and have 
  //completed initialization of the bootloader 
  //measured: 
  //  * ~0.6ms for network voltage to reach requested value after network enabled
  //  * ~8ms for MCU to come online after network turns on (with buck)
  //  * ~16ms for bootloader to initialize and reach can_protocol_task loop

  WaitUntilCANGatewayAvailable(); 
  
  if ( nodeNumber ) // node number not zero - nodeNumber
  {
    
    if(selectBootloaderNode(nodeNumber))
    {
      MakeCANGatewayAvailable(); 
      return; //already running app or had error in selecting node
    }
    
    //wake selected node
    m.cob_id = 0x144;
    m.len = 4;
    m.data[0] = 0x03;
    m.data[1] = 0x80;
    m.data[2] = 0x00;
    m.data[3] = 0x00;
    canSend(0, &m);  
    
    //we don't expect a CAN response, so don't pend here   
  }
  else
  {
     // broadcast restart
      m.cob_id  = 0x14F;
      m.data[0] = 0x03;
      m.data[1] = 0x80;
      m.data[2] = 0x00;
      m.data[3] = 0x00;
      m.len = 4;
      canSend(0 ,&m);
      
      //we don't expect a CAN response, so don't pend here
  }
  
  MakeCANGatewayAvailable(); 
  return;
}
/**********************************************************************************************************
*                                             WDT_ISR()
**********************************************************************************************************/
/**
* @brief WatchDog ISR
* @param none
* return none
*
*/
void WDT_ISR(void)
{
  VICSoftIntClear = 1; // clear the WDT interrupt
  
}

/**********************************************************************************************************
*                                             InitWatchDogTimer()
**********************************************************************************************************/
/**
* @brief Initialize the WatchDogTimer.
* @param none
* @return none
*
*/
void InitWatchDogTimer(void)
{
  CPU_SR cpu_sr;

  if( WDMOD & 0x04 ) 
  {								/* Check for watchdog time out   */
    
	WDMOD &= ~0x04;						   /* Clear time out flag           */
  }
  
  
  WDTC  = 0x00010000;						   /* Set watchdog time out value (about 10sec)    */
  WDMOD = 0x03;   
  
 
  CPU_CRITICAL_ENTER();
  
  WDFEED = 0xAA; // initialize
  WDFEED = 0x55;
  
  
  CPU_CRITICAL_EXIT();
}

/**********************************************************************************************************
*                                             ResetWatchDogTimer()
**********************************************************************************************************/
/**
* @brief Feeds the WatchDog
* @param none
* @return none
*
*/
void ResetWatchDogTimer(void)
{
  CPU_SR cpu_sr;
  
  CPU_CRITICAL_ENTER();
  
  WDFEED = 0xAA; // initialize
  WDFEED = 0x55;
  
  CPU_CRITICAL_EXIT();
  
}

/**********************************************************************************************************
*                                      Reset_Module()
**********************************************************************************************************/
/**
* @brief  Triggers a reset within 1s by enabling watchdog
* @todo:  avoid watchdog to cause reset, or reduce timing
*
* @param none
* @return none
*/
void Reset_Module(void)
{
   CPU_SR cpu_sr;
  
   WDTC  = 1000000; // 4 * WDTC / RC  == number of intervals - 1 secs	
   WDMOD = 0x03; 
   
   CPU_CRITICAL_ENTER();
   WDFEED = 0xAA; // initialize
   WDFEED = 0x55;
   CPU_CRITICAL_EXIT();
}


/**********************************************************************************************************
*                                            idleProcessor
*********************************************************************************************************/
/**
* @brief puts the processor into idle mode.  This function has no OS delays and can be called from an ISR
* 
* @todo currently the PLL remains active during idle.  additional power savings can be attained by disabling PLL
*       However, the system timers (including RTC) depend on PLL.
*
* @param none
* @return none
*
*/
void idleProcessor( void )
{
  CPU_INT32U pconpMem;
  
  pconpMem = PCONP;
  
  BatteryControl_LowPowerStatus |= BIT7; //low power mode enabled
  BatteryControl_LowPowerStatus &= ~(BIT0|BIT1|BIT2); //low power mode enabled, clear source
  
  PCONP = BIT9 | BIT5; //turn off all peripherals except RTC and PWM.  PWM required for charging.
  
  BatteryControl_LowPowerStatus |= BIT3;
  IO0SET = BIT21; //JML DEBUG - See IOInit in app.c for debug usage
  PCON_bit.IDL = 1; //set idle mode
  
  //------Processor is Idling-------//
  //any enabled interrupt will cause code to resume below: 
  //  RTC 
  //  Radio Packet received 
  //  ?future: accelerometer
  
  PCONP = pconpMem;
  IO0CLR = BIT21; //JML DEBUG - See IOInit in app.c for debbug usage

}

/**********************************************************************************************************
*                                            powerDownProcessor
**********************************************************************************************************/
/**
* @brief puts the processor into powerdown mode
*   the RTC is off during pwoerdown mode, so only a radio packet can wake up the processor
*   Deep discharge of batteries cannot be protected against in this mode so it is not advised! 
*
* @param none
* @return none
*
*/
void powerDownProcessor( void)
{
   IO0SET = BIT21; //JML DEBUG - See IOInit in app.c for debug usage
   
   //all peripherals will be off in powerdown mode, so no need to disable
    PCON_bit.PD = 1; //set power down mode

    //------Processor is Powered Down-------//
    //an external interrupt will cause code to resume below
    //  Radio Packet received
    //  ?future: accelerometer
    
    IO0CLR = BIT21; //JML DEBUG - See IOInit in app.c for debbug usage
    BSP_Init(BSP_PLL); //reinitialize PLL 
  
}

/**********************************************************************************************************
*                                            Enter_Low_Power()
**********************************************************************************************************/
/**
* @brief Puts all peripherals into low power states and then idles processor
*        Allows both RTC and Radio to wake from low power state
* @param  param1=1: powerdown, else: enter idle mode.  Use of powerdown mode is not advised!
* @return none
*
*/
void Enter_Low_Power ( CPU_INT08U param1 )
{

  //OS_ERR  	err;
  
  
  
  //don't allow low power mode if network is on or WOR is not enabled
  if(IS_PWR_NETWORK_ENABLED() || !statusRadioWOR()) 
  {
    BatteryControl_LowPowerStatus &= ~BIT7; //disable low power entry
    return;
  }
  

  //allow any SPI or I2C communication to finish before disabling peripherals
  //OSTimeDlyHMSM(0, 0, 0, 20, OS_OPT_TIME_HMSM_STRICT, &err);  
  //UINT32 delay = 1000;
  //while(delay--);
  
  TURN_OFF_THERMISTOR();
  HBT_LED = 0; 
  // turn off anything else here.  I.e., set IO pins to low power settings if necessary
  // Don't use any peripherals beyond this point.  
  //
  
  if(param1 == 1)
    powerDownProcessor();
  else
    idleProcessor();
        
}

/**********************************************************************************************************
*                                      ReadSerialNumber()
**********************************************************************************************************/
/**
* @brief  reads the serial number and configures the RF addresses on power up for default values.
*               Checks on restore for illegal values. if values are illegal use the defaults in the OD.
*
* @param none
* @return none
*/
void ReadSerialNumber(void)
{
  CPU_INT08U data[5];
  CPU_INT08U flag;
  
  ObjDict_obj1018_Serial_Number = *((UNS8 *)(SERIAL_NUMBER_ADDRESS)) << 8; // high byte
  ObjDict_obj1018_Serial_Number += *(UNS8 *)(SERIAL_NUMBER_ADDRESS + 1);       // low byte
  
  // initialize channel data
  data[0] = RADIO_LocalAddress;
  data[1] = RADIO_RemoteAddress;
  data[2] = RADIO_ChannelNumber;
  data[3] = RADIO_TXPower;
  data[4] = 0;
  
  
  //APP_RADIO_SETTINGS:
  //0:Local Addr  (PM Addr, default 0x04, min 0x01, max 0xFE)
  //1:Remote Addr (CT Addr, default 0x03, min 0x01, max 0xFE)
  //2:Chan        (default 0x05, min 0x00, max 0x09)
  //3:TXPower     (default 20, min 0, max 46 anything >46 will result in same power as 46) 
  //4:RestoreValue(default 0, 1:restore defaults)
  //check for legitimate values:
  if ( *(UNS8 *)(APP_SETTINGS_ADDRESS)     > 0xFE || *(UNS8 *)(APP_SETTINGS_ADDRESS    ) < 0x1  || \
       *(UNS8 *)(APP_SETTINGS_ADDRESS + 1) > 0xFE || *(UNS8 *)(APP_SETTINGS_ADDRESS + 1) < 0x1  || \
       *(UNS8 *)(APP_SETTINGS_ADDRESS + 2) > 0x09 )
  {
      flag = 1; // out of range
  }  
  // check for RestoreValue bypass flag
  if ((*(UNS8 *)(APP_SETTINGS_ADDRESS + 4) == 0x01))
  {
    commandByte |= 1;
    flag = 1;
  }
  
  if (flag == 1) // if 1, write default values to flash
  {
    wrCpuNvData( APP_SETTINGS_SECTOR, APP_SETTINGS_SECTOR_ADDRESS, &data[0], sizeof(data) );
  }
  else
  {
    
    RADIO_LocalAddress =  *(UNS8 *)(APP_SETTINGS_ADDRESS); 
    RADIO_RemoteAddress = *(UNS8 *)(APP_SETTINGS_ADDRESS + 1); 
    RADIO_ChannelNumber = *(UNS8 *)(APP_SETTINGS_ADDRESS + 2);
    RADIO_TXPower       = *(UNS8 *)(APP_SETTINGS_ADDRESS + 3);
    
  }
  
  if (ObjDict_obj1018_Serial_Number == 65535)
    ObjDict_obj1018_Serial_Number = 0;  
  
}

