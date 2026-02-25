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
*                                        BOARD SUPPORT PACKAGE
*
*                                  ST Microelectronics ST STM32F10xxE
*                                              on the 
*                                 IAR LPC2103-SK Evaluation Board
*
* Filename      : bsp.h
* Version       : V1.00
* Programmer(s) : FT
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                                 MODULE
*
* Note(s) : (1) This header file is protected from multiple pre-processor inclusion through use of the
*               BSP present pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef  BSP_PRESENT
#define  BSP_PRESENT

/*
*********************************************************************************************************
*                                              INCLUDE FILES
*********************************************************************************************************
*/
#include "includes.h"
#include "File_Operations.h"

/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/


#ifdef   BSP_MODULE
#define  BSP_EXT
#else
#define  BSP_EXT  extern
#endif

/*
*********************************************************************************************************
*                                        DEFAULT CONFIGURATION
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define APP_SETTINGS_SECTOR_ADDRESS 0x1E00
#define APP_SETTINGS_SECTOR         16
#define APP_SETTINGS_ADDRESS        0x3DE00 // last 512 segment of allowable memory

#define SERIAL_NUMBER_ADDRESS       0x1FFE // last 2 bytes of radio bootloader

#define  BSP_BOARD_XTAL_FREQ                 10000000UL
#define  BSP_PLL                             5 //use 1 through 6
#define  BSP_BOARD_CCLK_FREQ                 BSP_BOARD_XTAL_FREQ * BSP_PLL
#define  BSP_BOARD_PCLK_FREQ                 BSP_BOARD_CCLK_FREQ / 4            //assumes APBDIV = 0 (set to divide by 4) 
#define  BSP_BOARD_TMR1_8USEC                (10L * 8 / 4)* BSP_PLL		//  8usec, pclk is osc/4 already
#define  BSP_BOARD_TMR1_100USEC              (10L * 100 / 4)* BSP_PLL		//  100usec, pclk is osc/4 already

#define BSP_PLL_STATUS (PLLSTAT & BIT8 && PLLSTAT & BIT9)

#define HBT_LED		                     IO1PIN_bit.P1_24


/*
*********************************************************************************************************
*                                              DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                              GLOBAL VARIABLES
*********************************************************************************************************
*/
extern volatile CPU_BOOLEAN runSchedulerFlag;
extern volatile CPU_INT32U oneSecCounter[MAX_NUM_FILES];
/*
*********************************************************************************************************
*                                                 MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void         BSP_Init           (CPU_INT08U pll_val);
void         RTC_Init           (void);
void         RTC_SetClock       (void);
void         RTC_HaltClock      (void);
void         RTC_SetAlarm       (void );
void         RTC_Turn_OFF_Alarms(void);
void         BSP_IntDisAll      (void);
CPU_INT32U   BSP_CPU_ClkFreq    (void);
CPU_INT32U   BSP_CPU_PclkFreq   (void);
CPU_INT32U   GetTimer1Count     (void);
void WakeRemoteModule (int maxNodeNumber);
void ResetWatchDogTimer(void);
void InitWatchDogTimer(void);
void Enter_Low_Power ( CPU_INT08U param1 );
void idleProcessor( void);
void powerDownProcessor( void);
void ReadSerialNumber(void);
void InitWatchdog( void );
void Watchdog( void );
void ChangeWatchdogTime( CPU_INT32U time );
void Reset_Module(void);
/*
*********************************************************************************************************
*                                             LED SERVICES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         PUSH BUTTON SERVICES
*********************************************************************************************************
*/

//^^CPU_BOOLEAN  BSP_PB_GetStatus       (CPU_INT08U pb);

/*
*********************************************************************************************************
*                                         LCD SERVICES
*********************************************************************************************************
*/

void         BSP_LCD_LightOn        (void);
void         BSP_LCD_LightOff       (void);

/*
*********************************************************************************************************
*                                             TICK SERVICES
*********************************************************************************************************
*/

void         BSP_Tmr_TickISR_Handler(void);

 

#endif
