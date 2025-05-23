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
* MODIFIED to include ISRs for RX, TX, and NS and acceptance filter data
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         CAN BSP DRIVER CODE
*
*                                              LPC21XX
*
* Filename : can_bsp.c
* Version  : V2.42.01
*********************************************************************************************************
*/

/*
****************************************************************************************************
*                                             INCLUDES
****************************************************************************************************
*/
#include "drv_can.h"                                  /* driver declarations                      */
#include "drv_def.h"                                  /* driver layer declarations                */
#include "drv_can_reg.h"                              /* register declarations                    */
#include "can_bus.h"
#include "ObjDict.h"
#include "can_cpu.h"  /*CAN Festival*/
/*
****************************************************************************************************
*                                             DEFINES
****************************************************************************************************
*/

/*
****************************************************************************************************
*                                              MACROS
****************************************************************************************************
*/

/*
****************************************************************************************************
*                                            LOCAL DATA
****************************************************************************************************
*/
	/*! This member holds the initialized node id of the module */
static CPU_INT08U LPC21XX_DevIds[LPC21XX_CAN_N_DEV];

/*
****************************************************************************************************
*                                            GOBAL DATA
****************************************************************************************************
*/
CPU_INT32U BEI_Counter = 0;

#if LPC21XX_CAN_FILTER_EN > 0
  #if LPC21XX_CAN_STD_FILTER_SIZE > 0
const CPU_INT32U StdFilter_Tbl[LPC21XX_CAN_STD_FILTER_SIZE] =
{
    0x20002080                            /* Ids 1,2,3,4 on can controller1  allowed       */
};
  #endif

  #if LPC21XX_CAN_STD_GROUP_FILTER_SIZE > 0
const CPU_INT32U StdGroupFilter_Tbl[LPC21XX_CAN_STD_GROUP_FILTER_SIZE] =
{
     0x208A2081, 0x218A2180, 0x220A2201, 0x228A2281,           /* ^^^Ids range 0x10-0x15 on can controller1 allowed 'JC -- I interpret as COB ID 1000 to 1500' */
    0x230A2301, 0x238A2381, 0x240A2401, 0x248A2481,            /* Ids range 0x20-0x30 on can controller2 allowed */
    0x250A2501, 0x258A2581, 0x260A2601, 0x274F2701             /* Ids range 0x20-0x30 on can controller2 allowed */
};
  #endif

  #if LPC21XX_CAN_EXT_FILTER_SIZE > 0
const CPU_INT32U ExtFilter_Tbl[LPC21XX_CAN_EXT_FILTER_SIZE] =
{
    0x20000010,                                       /* Ids 0x10x on can controller1 allowed        */
    0x20000020                                        /* Ids 0x20x on can controller1 allowed        */
};
  #endif

  #if LPC21XX_CAN_EXT_GROUP_FILTER_SIZE > 0
const CPU_INT32U ExtGroupFilter_Tbl[LPC21XX_CAN_EXT_GROUP_FILTER_SIZE] =
{
    0x20000001, 0x2000000F                            /* Ids range 0x1x-0xFx on can controller1 allowed */
};
  #endif

#endif /* can acceptance filter */

/*
****************************************************************************************************
*                                            FUNCTIONS
****************************************************************************************************
*/


/*------------------------------------------------------------------------------------------------*/
/*!
* \brief                      CALCULATING THE TIMING REGISTER VALUES
*
*           In this function the Timing Register Values are calculated according the Baudrate
*           in the para-Struct.
*
* \param    data              Pointer to the para-Struct
*
* \return   Errorcode (0 if ok, <0 if an error occured)
*/
/*------------------------------------------------------------------------------------------------*/

CPU_INT16S LPC21XX_CalcTimingReg (LPC21XX_CAN_BAUD *data)
{
	// 100K from 10MHz/4 pclk, ref: Bosch Timing Doc..
	// 25quanta (22 + 3) or (1ts + 8tp + 8ts1 + 8ts2) with 4tsjw

	data->PRESDIV 	= BSP_PLL -1;  //MSEL in bits 0:4 of PLLSTAT; 
        data->PSEG1   	= 15; // was 15
	data->PSEG2   	= 7; // was 7
	data->RJW 	= 3;

	return LPC21XX_CAN_NO_ERR ;
}

	

/*************************************************************************************************/
/*!
* \brief                         STORE DEV ID FOR ISRs
*
*           Store the devId for transmission of the right parameter to CanBus-modules.
*
*
* \param devId   the CAN device name - use only when irqs are eneabled
* \param devName the CAN device name
* \return void
*/
/*************************************************************************************************/

#if ((LPC21XX_CAN_RX_INTERRUPT_EN > 0) || \
     (LPC21XX_CAN_TX_INTERRUPT_EN > 0) || \
     (LPC21XX_CAN_NS_INTERRUPT_EN > 0))

void LPC21XX_BSP_SetDevIds (CPU_INT08U devId, CPU_INT08U devName)
{
    LPC21XX_DevIds[devName] = devId;
}
#endif


/*************************************************************************************************/
/*!
* \brief                      CAN RX INTERRUPT SERVICE ROUTINE 1
*
*           Interrupt Service Routine for CAN receive interrupt on LPC21XX_CAN_BUS_0
*
*
* \param void
* \return void
*/
/*************************************************************************************************/
#if LPC21XX_CAN_RX_INTERRUPT_EN > 0
void LPC21XXCANISR_Rx1 (void)
{

    if ( LPC21XX_CAN_C1ICR & 0x80 || LPC21XX_CAN_C1ICR & 0x20 ) // test for bus error, if false, process interrupt
    {
      CAN_Receive_BEI++;
    }
    else
    {
      CanBusRxHandler (LPC21XX_DevIds[0]);
      CAN_Receive_Messages++;
    }
    
   
    LPC21XX_CAN_C1CMR = 0x04L;                        /* release receive buffer, clear receive int */
    LPC21XX_CAN_VICVECTADDR = 0;                      /* acknowledge interrupt                     */
}
#endif

/*************************************************************************************************/
/*!
* \brief                      CAN RX INTERRUPT SERVICE ROUTINE 2
*
*           Interrupt Service Routine for CAN receive interrupt on LPC21XX_CAN_BUS_1
*
*
* \param void
* \return void
*/
/*************************************************************************************************/
#if LPC21XX_CAN_RX_INTERRUPT_EN > 0
void LPC21XXCANISR_Rx2 (void)
{
    CanBusRxHandler (LPC21XX_DevIds[1]);

    LPC21XX_CAN_C2CMR = 0x04L;                        /* release receive buffer, clear receive int */
    LPC21XX_CAN_VICVECTADDR = 0;                      /* acknowledge interrupt                     */
}
#endif

/*************************************************************************************************/
/*!
* \brief                      CAN TX INTERRUPT SERVICE ROUTINE 1
*
*           Interrupt Service Routine for CAN transmit interrupt on LPC21XX_CAN_BUS_0
*
*
* \param void
* \return void
*/
/*************************************************************************************************/
#if LPC21XX_CAN_TX_INTERRUPT_EN > 0
void    LPC21XXCANISR_Tx1 (void)
{
    volatile CPU_INT32U Interrupt_Status;

    CanBusTxHandler (LPC21XX_DevIds[0]);
    CAN_Transmit_Messages++;

    Interrupt_Status = LPC21XX_CAN_C1ICR;             /* read ICR to clear interupt                */
    LPC21XX_CAN_VICVECTADDR = 0;                      /* acknowledge interrupt                     */
}
#endif

/*************************************************************************************************/
/*!
* \brief                      CAN TX INTERRUPT SERVICE ROUTINE 2
*
*           Interrupt Service Routine for CAN transmit interrupt on LPC21XX_CAN_BUS_1
*
*
* \param void
* \return void
*/
/*************************************************************************************************/
#if LPC21XX_CAN_TX_INTERRUPT_EN > 0
void    LPC21XXCANISR_Tx2 (void)
{
    volatile CPU_INT32U Interrupt_Status;

    CanBusTxHandler (LPC21XX_DevIds[1]);

    Interrupt_Status = LPC21XX_CAN_C2ICR;             /* read ICR to clear interupt                */
    LPC21XX_CAN_VICVECTADDR = 0;                      /* acknowledge interrupt                     */
}
#endif

/*************************************************************************************************/
/*!
* \brief                      CAN NS INTERRUPT SERVICE ROUTINE
*
*           Interrupt Service Routine for CAN node status interrupt on both can busses
*
*
* \param void
* \return void
*/
/*************************************************************************************************/
#if LPC21XX_CAN_NS_INTERRUPT_EN > 0
void    LPC21XXCANISR_Ns (void)
{
    CPU_INT32U Node_Status = 0;

    Node_Status = LPC21XX_CAN_C1ICR;                  /* read ICR to clear interupt               */
    if (((Node_Status & 0x20L) != 0x0) ||             /* Error Passive                            */
        ((Node_Status & 0x80L) != 0x0)) 
             /* Bus Error                                */
    {   
      switch ((Node_Status & 0x00C00000) >> 22 )
      {
      case 0:
        {
          CAN_BitErrors++;
          break;
        }
      case 1:
        {
          CAN_FormErrors++;
          break;
        }
      case 2:
        {
          CAN_StuffErrors++;
          break;
        }
      case 3:
        {
          CAN_OtherErrors++;
          break;
        }
      }
      
      CAN_Rx_ErrCounter = (LPC21XX_CAN_C1GSR >> 16) & 0x00FF;
      CAN_Tx_ErrCounter = (LPC21XX_CAN_C1GSR >> 24) & 0x00FF;
      CAN_TotalErrors = CAN_BitErrors + CAN_StuffErrors + CAN_FormErrors + CAN_OtherErrors; 
      
      if (CAN_Rx_ErrCounter > 126 || CAN_Tx_ErrCounter > 126)
      {
            canReset();
            CAN_NS_ResetCounter++;
      }    
    
    }

	/*    CAN 2    */
    Node_Status = LPC21XX_CAN_C2ICR;                  /* read ICR to clear interupt               */
    if (((Node_Status & 0x20L) != 0x0) ||             /* Error Passive                            */
        ((Node_Status & 0x80L) != 0x0)) {             /* Bus Error                                */

        CanBusNSHandler (LPC21XX_DevIds[1]);
    }
    LPC21XX_CAN_VICVECTADDR = 0;                      /* acknowledge interrupt                    */
}
#endif
