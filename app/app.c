/*                                              uC/OS-III
*                                        The Real-Time Kernel
*
*                    Copyright 2009-2022 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*/
/*
*********************************************************************************************************
*                                              NNP Power Module
*
*
*                                             
*
		Based on 'core Project' for no particular EVB. jayh
		
//		Svn Version:   $Rev: 14 $     $Date: 2010-03-29 08:41:59 -0400 (Mon, 29 Mar 2010) $

* Filename      : app.c
* Version       : V1.00
* Programmer(s) : FT
*
*********************************************************************************************************
*/

// Doxygen
/*!
** @defgroup osmanagement OS Management
** @ingroup userapi
*/
/*!
** @defgroup uciii uC-III
** @ingroup osmanagement
*/
/*!
** @file   app.c
** @author JDCC
** @date   
**
** @brief Main entry point for OS
** @ingroup uciii
**
**
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include "includes.h"
#include "gateway.h"
#include "can_cpu.h"
#include "RunCANServer.h"
#include "can_bus.h"
#include "can_cfg.h"
#include "sys.h"
#include "cc1101radio.h"
#include "ObjDict.h"
#include "pwrnet.h"
#include "SPI_Memory.h"
#include "os_app_hooks.h"
/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/
/* Prototypes for internals functions */




/*
*********************************************************************************************************
*                                       GLOBAL VARIABLES
*********************************************************************************************************
*/
/* ----------------- APPLICATION GLOBALS ------------------ */

static  OS_TCB      AppTaskStartTCB;
static  CPU_STK     App_TaskStartStk[APP_TASK_START_STK_SIZE];

static  OS_TCB      RunCanServerTaskTCB;
static  CPU_STK     RunCanServerStk[RUNCANSERVER_STK_SIZE];

static  OS_TCB      RunCanTimerTaskTCB;
static  CPU_STK     RunCanTimerStk[RUNCANTIMER_STK_SIZE];

static  OS_TCB      SleepTaskTCB;
static  CPU_STK     SleepStk[SLEEP_STK_SIZE];

static  OS_TCB      RunIOScanTaskTCB;
static  CPU_STK     RunIOScanTaskStk[RUNIOSCAN_STK_SIZE];

static  OS_TCB      RunScriptTCB;
static  CPU_STK     RunScriptTaskStk[RUNSCRIPT_STK_SIZE];

OS_SEM GatewaySem;
OS_Q ScriptScheduler_Q;


/*
*********************************************************************************************************
*                                         LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  App_TaskStart               (void        *p_arg);
static  void  App_TaskCreate              (void);

void IOInit(void);


/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*               NOTE: Power Module download requires address remapping to 0x00002000
*               EVB uses Linker starting address of 0x00000000
*
* Argument(s) : none
*
* Return(s)   : none
*********************************************************************************************************
*/
int  main (void)
{
    OS_ERR  err;


    BSP_IntDisAll();                                            /* Disable all interrupts until we are ready to accept them */
    OSInit(&err);                                                         

    OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,                /* Create the start task                                */
                 (CPU_CHAR   *)"Run Gateway State Machine",
                 (OS_TASK_PTR )App_TaskStart, 
                 (void       *)0,
                 (OS_PRIO     )APP_TASK_START_PRIO,
                 (CPU_STK    *)&App_TaskStartStk[0],
                 (CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10,
                 (CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

#if (OS_TASK_NAME_SIZE > 5)
    OSTaskNameSet(APP_CFG_TASK_START_PRIO, "Start", &err);
#endif

    App_OS_SetAllHooks(); 
    OSStart(&err);                                                  /* Start multitasking (i.e. give control to uC/OS-III)       */
}


/*
*********************************************************************************************************
*                                          AppTaskStart()
*
* Description : The startup task.  The uC/OS-III ticker should only be initialize once multitasking starts.
*
* Argument(s) : p_arg       Argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Return(s)   : none.
*
* Note(s)     : (1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                   used.  The compiler should not generate any code for this statement.
*
*               (2) Interrupts are enabled once the task starts because the I-bit of the CCR register was
*                   set to 0 by 'OSTaskCreate()'.
*********************************************************************************************************
*/
/**
@brief Initializes IO, PLL, CAN.  Then creates other tasks and runs radio gateway
* Note(s)     : (1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                   used.  The compiler should not generate any code for this statement.
*
*               (2) Interrupts are enabled once the task starts because the I-bit of the CCR register was
*                   set to 0 by 'OSTaskCreate()'.
@param p_arg Argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*/
static  void  App_TaskStart (void *p_arg)
{
    (void)p_arg;
    
    IOInit();                                                  /* Set up IO pins  */
    BSP_Init(BSP_PLL); 
    //CPU_TS_TmrFreqSet(BSP_BOARD_CCLK_FREQ);  //this is used for timestamp timer which isn't used in our app.
                                    //BSP_Init actually sets frequency based on PLL setting and 10MHz osc.
     
    canInit(); // initializes the logical structure for CAN bus.
      
    App_TaskCreate();                                           /* Create the other application tasks */
    //InitWatchDogTimer(); // initializes WDT
  
    runRadioGateway(); //infinite loop

}

/**
@brief post for CAN Gateway (SDOs and RM Bootloader messages that expect a response)
@param none 
@return err
*/
OS_ERR postGatewaySem( void )
{
  OS_ERR err;
  OSSemPost(&GatewaySem, OS_OPT_POST_1, &err);
  
  return err;
}

/**
@brief pend for CAN Gateway (SDOs and RM Bootloader messages that expect a response)
@param timeout 
@return err
*/
OS_ERR pendGatewaySem( OS_TICK timeout )
{
  OS_ERR err;
  CPU_TS ts;
  OSSemPend(&GatewaySem, timeout, OS_OPT_PEND_BLOCKING, &ts, &err);
  
  
  return err;
}

/**
@brief Performs various initializations and creates all tasks
*/
static  void  App_TaskCreate (void)
{
  
  OS_ERR    err;
   
  /* timing is important here - don't change the order below */
  Status_NV_Flash_Status |= InitNvMemory();
  ReadSerialNumber(); // this has to run before restoreValues()
  Status_Restore = RestoreValues();   // restore values loads from serial flash for changed configurations
  DateTime_ClockSet = 0; //ignore value that was restored 
  
  InitCANServerTask();
  InitIOScanTask(); 
  
  
  initRadioConfig();
  RTC_Init();         /* need to run after restoring TIME */
  InitPowerNetwork(); 
  
  OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_HMSM_STRICT, &err);  //JML: why is this time delay here?
  
  OSSemCreate(&GatewaySem, "CAN Gateway Semaphore", 0, &err); 
  if (err != OS_ERR_NONE) { while(1){asm("nop");}}
  OSSemCreate(&GatewayAccessControl,  "CAN Gateway Access Control",  1,  &err); 
  if (err != OS_ERR_NONE) { while(1){asm("nop");}}
  OSSemCreate(&CanTimerSem, "CANopen Alarms", 0, &err);
  if (err != OS_ERR_NONE) { while(1){asm("nop");}}
  OSSemCreate(&SleepSem, "Sleep Event", 0, &err);
  if (err != OS_ERR_NONE) { while(1){asm("nop");}}
  OSQCreate(&ScriptScheduler_Q, "Script Q", MAX_QUEUED_SCRIPTS, &err);
  if (err != OS_ERR_NONE) { while(1){asm("nop");}}
    
  
  
  
  OSTaskCreate((OS_TCB    *)&RunCanServerTaskTCB,                                           
                 (CPU_CHAR   *)"RunCANServerTask",
                 (OS_TASK_PTR )RunCANServerTask, 
                 (void       *)0,
                 (OS_PRIO     )RUNCANSERVER_PRIO,
                 (CPU_STK    *)&RunCanServerStk[0],
                 (CPU_STK_SIZE)RUNCANSERVER_STK_SIZE / 10,
                 (CPU_STK_SIZE)RUNCANSERVER_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
  
  OSTaskCreate((OS_TCB    *)&RunCanTimerTaskTCB,                                           
                 (CPU_CHAR   *)"RunCANTimerTask",
                 (OS_TASK_PTR )RunCANTimerTask, 
                 (void       *)0,
                 (OS_PRIO     )RUNCANTIMER_PRIO,
                 (CPU_STK    *)&RunCanTimerStk[0],
                 (CPU_STK_SIZE)RUNCANTIMER_STK_SIZE / 10,
                 (CPU_STK_SIZE)RUNCANTIMER_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
    
  OSTaskCreate((OS_TCB    *)&SleepTaskTCB,                                           
                 (CPU_CHAR   *)"SleepTask",
                 (OS_TASK_PTR )SleepTask, 
                 (void       *)0,
                 (OS_PRIO     )SLEEP_PRIO,
                 (CPU_STK    *)&SleepStk[0],
                 (CPU_STK_SIZE)SLEEP_STK_SIZE / 10,
                 (CPU_STK_SIZE)SLEEP_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
  
  OSTaskCreate((OS_TCB    *)&RunIOScanTaskTCB,                                           
                 (CPU_CHAR   *)"RunIOScanTask",
                 (OS_TASK_PTR )RunIOScanTask, 
                 (void       *)0,
                 (OS_PRIO     )RUNIOSCAN_TASK_PRIO,
                 (CPU_STK    *)&RunIOScanTaskStk[0],
                 (CPU_STK_SIZE)RUNIOSCAN_STK_SIZE / 10,
                 (CPU_STK_SIZE)RUNIOSCAN_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
  
  OSTaskCreate((OS_TCB    *)&RunScriptTCB,                                           
                 (CPU_CHAR   *)"RunScriptTask",
                 (OS_TASK_PTR )RunScriptTask, 
                 (void       *)0,
                 (OS_PRIO     )RUNSCRIPT_TASK_PRIO,
                 (CPU_STK    *)&RunScriptTaskStk[0],
                 (CPU_STK_SIZE)RUNSCRIPT_STK_SIZE / 10,
                 (CPU_STK_SIZE)RUNSCRIPT_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
  
  
  Scripts_Init(); //only init scripts after semaphore has been created
  InitFiles(1); // do a full file reset only if no directory present
  
}


/**
@brief Initializes port pin values and directions
*/
void IOInit(void)
{
     /* note that batPack.c defines bits 10, 12, 23, 11, 13, and 24 as outputs*/
     //See PRJ-NNPS-SYS-SPEC-20

      PINSEL0 = 0x00005550 ;  //SPI0 enabled, I2C enabled
      PINSEL1 = 0x154002A8 ;  //analog in enabled, SPI1 enabled, EINT0 will be configured later

      IO0PIN = 0x00000000;
      IO0DIR = 0x01E07D03;  //PWMBT should be assigned as output, nBOOT set as output

      IO1PIN = 0x02EA0000;
      IO1DIR = 0x03EB0000; 
     
     //DEBUG ONLY
     IO0DIR |= 0x00000003;  //TX and RX set as outputs 
     IO0CLR  = 0x00000000;
     //END DEBUG ONLY
     
     //debugging pin descriptions
     //P0.0  (pin19/TXD0) - SYNC entry(1)/exit(0)
     //P0.1  (pin21/RXD0) - ScriptInterpreter entry(1)/exit(0)
     //P0.21 (pin1/N.C.)  - processor idled with peripherals OFF (1), processor normal(0)
     //P0.22 (pin2/N.C.)  - processor idled by Idle Task(1), processor running(0)
     
     //disable UART, reserved, and CAN peripherals.  CAN peripheral will be enabled when network ON
     //JML TODO: All peripherals should only be enabled and disabled as needed.
     PCONP &= ~ ( BIT3 | BIT4 | BIT11 |BIT13 | BIT14 );
   
}

/**
@brief copies the stack usage (as percentage) to OD
*/
void setTaskStackUsage(void)
{
  StackApp = (UNS8)((AppTaskStartTCB.StkUsed*100)/AppTaskStartTCB.StkSize);
  StackCANServer = (UNS8)((RunCanServerTaskTCB.StkUsed*100)/RunCanServerTaskTCB.StkSize);
  StackCANTimer = (UNS8)((RunCanTimerTaskTCB.StkUsed*100)/RunCanTimerTaskTCB.StkSize);
  StackIOScan = (UNS8)((RunIOScanTaskTCB.StkUsed*100)/RunIOScanTaskTCB.StkSize);
  StackSleep = (UNS8)((SleepTaskTCB.StkUsed*100)/SleepTaskTCB.StkSize);
  StackScript = (UNS8)((RunScriptTCB.StkUsed*100)/RunScriptTCB.StkSize);
  //StackTick = (UNS8)((OSTickTaskTCB.StkUsed*100)/OSTickTaskTCB.StkSize); Tick Task removed in uC3.07
  StackIdle = (UNS8)((OSIdleTaskTCB.StkUsed*100)/OSIdleTaskTCB.StkSize);
  StackStats = (UNS8)((OSStatTaskTCB.StkUsed*100)/OSStatTaskTCB.StkSize);
}
