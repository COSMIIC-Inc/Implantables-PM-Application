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
*                Silicon Laboratories Inc. pursusant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*MODIFIED by JML (See <<)
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        uC/CAN CONFIGURATION
*
*                                              TEMPLATE
*
* Filename : can_cfg.c
* Version  : V2.42.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                              INCLUDES
*********************************************************************************************************
*/

#include  "can_cfg.h"
#include  "can_sig.h"
#include  "can_bus.h"
#include  "can_frm.h"
#include  "can_msg.h"
#include  "can_os.h"
#include  "drv_can.h"


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#if (CANSIG_GRANULARITY == CAN_CFG_BIT)
#define  CAN_CFG_SIZE_MOD           8u
#else
#define  CAN_CFG_SIZE_MOD           1u
#endif

#define  CAN_CFG_LITTLE_ENDIAN      1u

#if  (CAN_CFG_LITTLE_ENDIAN == 1u)
#define  CAN_CFG_ENDIAN             CANFRM_LITTLE_ENDIAN
#else
#define  CAN_CFG_ENDIAN             CANFRM_BIG_ENDIAN
#endif


/*
*********************************************************************************************************
*                                             LOCAL DATA
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                   CAN SIGNAL CALLBACK DEFINITION
*
* Description : Prototype of Signal Callback Function. This is only an example to be replaced by
*               one or more user Callback Functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (CANSIG_CALLBACK_EN == 1u)
void  CallBackFct(void*  arg, CANSIG_VAL_T*  value, CPU_INT32U  CallbackId);
#endif


/*
*********************************************************************************************************
*                                             GLOBAL DATA
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             CAN SIGNALS
*
* Description : Allocation of CAN Signals
*
* Note(s)     : This table must be modified by the user to define all Signals needed for the
*               Application. The below defined Signals are only examples and might be modified or
*               removed.
*********************************************************************************************************
*/

const  CANSIG_PARA  CanSig[CANSIG_N] = {
                                                                /* ---------------- SIGNAL NODESTATUS ----------------- */
    {CANSIG_UNCHANGED,                                          /*      Initial Status                                  */ 
      1,                                                        /*      Width in Bytes                                  */ 
      0,                                                        /*      Initial Value                                   */ 
#if (CANSIG_CALLBACK_EN > 0)
      0},                                                       /*      Callback Function: User Defined                 */ 
#else
    },
#endif
                                                                /* ----------------- SIGNAL CPULOAD ------------------- */ 
    {CANSIG_UNCHANGED,                                          /*      Initial Status                                  */ 
      1,                                                        /*      Width in Bytes                                  */ 
      0,                                                        /*      Initial Value                                   */ 
#if (CANSIG_CALLBACK_EN > 0)
      0}                                                        /*      No Callback                                     */ 
#else
    },
#endif
}; 


/*
*********************************************************************************************************
*                                            CAN MESSAGES
*
* Description : Allocation of CAN Messages
*
* Note(s)     : This Table must be modified by the user to define all Messages needed for 
*               the Application. The below defined Messages are only examples and might be
*               modified or removed.
*********************************************************************************************************
*/

const  CANMSG_PARA  CanMsg[CANMSG_N] = 
{ 
                                                                /* ------------------ MESSAGE STATUS ------------------ */
   { 0x150L,                                                    /*   <<   CAN-Identifier                                  */  
     CANMSG_TX,                                                 /*      Message Type                                    */ 
     2,                                                         /*    <<  DLC of Message                                  */ 
     2,                                                         /*    <<  No. of Links                                    */ 
      { { S_NODESTATUS,                                         /*      Signal ID                                       */ 
         0 },                                                   /*      Byte Position                                   */ 
        { S_CPULOAD,                                            /*      Signal ID                                       */ 
         1 }                                                    /*    <<  Byte Position                                   */ 
      },
   },

                                                                /* ----------------- MESSAGE COMMAND ------------------ */
   { 0x140L,                                                    /*   <<   CAN-Identifier                                  */ 
     CANMSG_RX,                                                 /*      Message Type                                    */ 
     1,                                                         /*      DLC of Message                                  */ 
     1,                                                         /*      No. of Links                                    */ 
     { { S_NODESTATUS,                                          /*      Signal ID                                       */ 
         0 }                                                    /*      Byte Position                                   */ 
     }
   }
}; 


/*
*********************************************************************************************************
*                                      CAN SIGNAL CONFIGURATION
*
* Description : Allocation of Global CAN Signal Table.
*
* Note(s)     : This is the Signal Table on which the CAN
*               Signal Layer will work. If the CANSIG_STATIC Configuration is chosen it will be
*               initialized with the CanCfg_Init() Function otherwise it will be filled by calling
*               CanSigCreate().
*********************************************************************************************************
*/

CANSIG_DATA  CanSigTbl[CANSIG_N];


/*
*********************************************************************************************************
*                                        CAN BUS CONFIGURATION
*
* Description : This structure contains the information for a bus for Can Controller 0.
*               A bus represents one interface to the world.
*
* Note(s)     : none.
*********************************************************************************************************
*/

const CANBUS_PARA CanCfg = {
    CAN_FALSE,                                                  /* EXTENDED FLAG                                        */
    CAN_DEFAULT_BAUDRATE,                                       /* BAUDRATE                                             */
    0u,                                                         /* BUS NODE                                             */
    0u,                                                         /* BUS DEVICE                                           */
                                                                /* << DRIVER FUNCTIONS                                     */
    LPC21XXCANInit,                                        /*      Init                                            */
    LPC21XXCANOpen,                                        /*      Open                                            */
    LPC21XXCANClose,                                       /*      Close                                           */
    LPC21XXCANIoCtl,                                       /*      IoCtl                                           */
    LPC21XXCANRead,                                        /*      Read                                            */
    LPC21XXCANWrite,                                       /*      Write                                           */
    {                                                           /* DRIVER IO FUNCTION CODES                             */
        IO_LPC21XX_CAN_SET_BAUDRATE,                         /*      Set Baud Rate                                   */
        IO_LPC21XX_CAN_START,                                /*      Start                                           */
        IO_LPC21XX_CAN_STOP,                                 /*      Stop                                            */
        IO_LPC21XX_CAN_RX_STANDARD,                          /*      Rx Standard                                     */
        IO_LPC21XX_CAN_RX_EXTENDED,                          /*      Rx Extended                                     */
        IO_LPC21XX_CAN_TX_READY,                             /*      Tx Ready                                        */
        IO_LPC21XX_CAN_GET_NODE_STATUS,                      /*      Get Node Status                                 */
    }
};


/*
*********************************************************************************************************
*                                      CAN BUS NODE STATUS HOOK
*
* Description : This function is a hook function called by CanBusNSHandler() when CANBUS_HOOK_NS_EN found
*               in 'can_cfg.h' is set to 1.
*
* Argument(s) : busId       BusId passed by CanBusNSHandler.
*
* Return(s)   : none.
*
* Caller(s)   : CanBusNSHandler()
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (CANBUS_HOOK_NS_EN == 1u)
void  CanBusNSHook (CPU_INT16S  busId)
{
    (void)&busId;                                               /* Prevent Compiler Warning                             */
}
#endif


/*
*********************************************************************************************************
*                                           CAN BUS RX HOOK
*
* Description : This function is a hook function called by CanBusRxHandler() when CANBUS_HOOK_RX_EN found
*               in 'can_cfg.h' is set to 1.
*
* Argument(s) : busId       BusId passed by CanBusRxHandler.
*               buffer      Buffer of CAN Frame Received.
*
* Return(s)   : Error code:  0 = No Error
*                           -1 = Error Occurred.
*
* Caller(s)   : CanBusRxHandler()
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (CANBUS_HOOK_RX_EN == 1u)
CPU_INT16S  CanBusRxHook (CPU_INT16S  busId, void  *buffer)
{
    (void)&busId;                                               /* Prevent Compiler Warning                             */
    (void)&buffer;                                              /* Prevent Compiler Warning                             */

    return (0);
}
#endif


/*
*********************************************************************************************************
*                                          CALLBACK FUNCTION
*
* Description : This function is the callback function example of the CAN signal layer. It
*               has to be replaced by one or more user callback functions.
*
* Arguments   : arg         Pointer to signal
*               value       Value of signal
*               CallbackId  ID to identify where this callback function was called from
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (CANSIG_CALLBACK_EN == 1u)
void  CallBackFct(void*  arg, CANSIG_VAL_T*  value, CPU_INT32U  CallbackId)
{
    (void)&CallbackId;                                          /* Prevent Compiler Warning                             */
    (void)&value;                                               /* Prevent Compiler Warning                             */
    (void)&arg;                                                 /* Prevent Compiler Warning                             */

}
#endif
