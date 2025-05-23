/*
*********************************************************************************************************
*                                              uC/CAN
*                                      The Embedded CAN suite
*
*                 Copyright by Embedded Office GmbH & Co. KG www.embedded-office.com
*                    Copyright 1992-2020 Silicon Laboratories Inc. www.silabs.com
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
*                                         CAN BSP DRIVER CODE
*
*                                              LPC21XX
*
* Filename : can_bsp.h
* Version  : V2.42.01
*********************************************************************************************************
*/

#ifndef _CAN_BSP_H_
#define _CAN_BSP_H_

/*
****************************************************************************************************
*                                             INCLUDES
****************************************************************************************************
*/
#include "cpu.h"                                              /* basic type definitions          */
#include "bsp.h"

/*
***************************************************************************************************
*                                           CONFIGURATION
***************************************************************************************************
*/

/*  ---------------------------------------------------------- */
/*  ----------------  PIN SELECTION -------------------------- */
/*  ---------------------------------------------------------- */

#define LPC21XX_CAN0_PIN_SELECTION      0x00040000L          /* Set bit 18, select CAN instead of GPIO */
#define LPC21XX_CAN1_PIN_SELECTION      0x00014000L          /* Set bits 14 and 16, select CAN instead of GPIO */

/*  ---------------------------------------------------------- */
/*  ----------------  INTERRUPT  ----------------------------- */
/*  ---------------------------------------------------------- */

#define LPC21XX_CAN_RX_INTERRUPT_EN           1                /* Enable(1) / Disable(0) CAN Receive       */
                                                               /*  Interrupt Handling                      */
#define LPC21XX_CAN_TX_INTERRUPT_EN           1                /* Enable(1) / Disable(0) CAN Transmit      */
                                                               /*  Interrupt Handling                      */
#define LPC21XX_CAN_NS_INTERRUPT_EN           1                /* Enable(1) / Disable(0) CAN Node Status   */
                                                               /*  Interrupt Handling                      */

/*  ---------------------------------------------------------- */
/*  ---------------- BAUDRATE SETTINGS ----------------------- */
/*  ---------------------------------------------------------- */
#define LPC21XX_CAN_DEF_SP                  750                /* Default bit sample point in 1/10 %       */

#define LPC21XX_CAN_DEF_RJW                 125                /* Default Re-synch Jump Width in 1/10 %    */

#define LPC21XX_CLOCK_FREQ            10000000L                /* The frequency with which the             */
                                                               /* CAN device is clocked                    */

#define LPC21XX_CAN_DEF_BAUD1           100000L                /* Default baud rate CAN 1                  */
#define LPC21XX_CAN_DEF_BAUD2           100000L                /* Default baud rate CAN 2                  */
#define LPC21XX_CAN_DEF_SAM               0x01

/*  ---------------------------------------------------------- */
/*  ----------------  ACCETANCE FILTER ----------------------- */
/*  ---------------------------------------------------------- */

#define LPC21XX_CAN_ACCEPTANCE_FILTER      (volatile CPU_INT32U *) 0xE0038000L

#define LPC21XX_CAN_FILTER_EN                 1                /* Enable(1) / Disable(0) Acceptance Filter */
#define LPC21XX_CAN_STD_FILTER_SIZE           2                /* Size of standard filter table            */
#define LPC21XX_CAN_STD_GROUP_FILTER_SIZE     12                /* Size of standard filter range table      */
#define LPC21XX_CAN_EXT_FILTER_SIZE           2                /* Size of extended filter table            */
#define LPC21XX_CAN_EXT_GROUP_FILTER_SIZE     2                /* Size of extended filter range table      */

#define LPC21XX_CAN_FILTER_SIZE               LPC21XX_CAN_STD_FILTER_SIZE + \
                                              LPC21XX_CAN_STD_GROUP_FILTER_SIZE + \
                                              LPC21XX_CAN_EXT_FILTER_SIZE + \
                                              LPC21XX_CAN_EXT_GROUP_FILTER_SIZE

/*  ---------------------------------------------------------- */
/*  ---------------- ARGUMENT CHECKING ----------------------- */
/*  ---------------------------------------------------------- */

#define LPC21XX_CAN_ARG_CHK_CFG               1                /* Enable(1) / Disable(0) argument checking */


/*
****************************************************************************************************
*                                              MACROS
****************************************************************************************************
*/

/*
****************************************************************************************************
*                                            DATA TYPES
****************************************************************************************************
*/

/*------------------------------------------------------------------------------------------------*/
/*! \brief Static CAN driver baudrate list */
typedef struct
{
	/*! This member holds the baudrate. */
	CPU_INT32U Baudrate;

	/*! This member holds the bit sample point in 1/10 percent. */
	CPU_INT32U SamplePoint;

	/*! This member holds the Re-synchronization Jump Width in 1/10 percent. */
	CPU_INT32U ResynchJumpWith;

	/*! This member holds the prescaler devide factor. */
	CPU_INT08U PRESDIV;

	/*! This member holds the resynchronization jump width (StdValue = 1). */
	CPU_INT08U RJW;

	/*! This member holds the propagation segment time (StdValue = 2). */
	CPU_INT08U PROPSEG;

	/*! This member holds the phase buffer segment 1 (StdValue = 7). */
	CPU_INT08U PSEG1;

	/*! This member holds the phase buffer segment 2 (StdValue = 7). */
	CPU_INT08U PSEG2;
        
        /*! This member holds the one vs 3 sample point testing 0 = 1 test; 1 = 3 test. */
	CPU_INT08U SAM;

} LPC21XX_CAN_BAUD;

/*
****************************************************************************************************
*                                       FUNCTION PROTOTYPES
****************************************************************************************************
*/

CPU_INT16S LPC21XX_CalcTimingReg  (LPC21XX_CAN_BAUD *data);

#if ((LPC21XX_CAN_RX_INTERRUPT_EN > 0) || \
     (LPC21XX_CAN_TX_INTERRUPT_EN > 0) || \
     (LPC21XX_CAN_NS_INTERRUPT_EN > 0))
void    LPC21XX_BSP_SetDevIds (CPU_INT08U devId, CPU_INT08U devName);
#endif

#if LPC21XX_CAN_RX_INTERRUPT_EN > 0
void       LPC21XXCANISR_Rx1 (void);
void       LPC21XXCANISR_Rx2 (void);
#endif

#if LPC21XX_CAN_TX_INTERRUPT_EN > 0
void       LPC21XXCANISR_Tx1 (void);
void       LPC21XXCANISR_Tx2 (void);
#endif

#if LPC21XX_CAN_NS_INTERRUPT_EN > 0
void       LPC21XXCANISR_Ns (void);
#endif

/*
****************************************************************************************************
*                                          ERROR SECTION
****************************************************************************************************
*/

/* Perform Plausibility Checks: CAN */
#if ( LPC21XX_CAN_FILTER_EN > 0) &&\
    ( LPC21XX_CAN_STD_FILTER_SIZE == 0) &&\
    ( LPC21XX_CAN_STD_GROUP_FILTER_SIZE == 0) &&\
    ( LPC21XX_CAN_EXT_FILTER_SIZE == 0) &&\
    ( LPC21XX_CAN_EXT_GROUP_FILTER_SIZE == 0)
#error "LPC21XX/drv_can.h : CAN Filter enabled, at least one table must be filled."
#endif

#if ( LPC21XX_CAN_FILTER_EN > 0) &&\
    ( LPC21XX_CAN_EXT_GROUP_FILTER_SIZE%2 != 0)
#error "LPC21XX/drv_can.h : Size of extended group filter must have alignment of 2."
#endif

#if ( LPC21XX_CAN_FILTER_SIZE > 0x800)
#error "LPC21XX/drv_can.h : Size of can filter too great. It can't be bigger than 0x800."
#endif

#if LPC21XX_CAN_ARG_CHK_EN < 0 || LPC21XX_CAN_ARG_CHK_EN > 1
#error "LPC21XX/drv_can.h : The configuration LPC21XX_CAN_ARG_CHK_EN must be 0 or 1"
#endif

#endif  /* #ifndef _CAN_BSP_H_ */
