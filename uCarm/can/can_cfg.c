/*
****************************************************************************************************
*                                               uC/CAN
*                                       The Embedded CAN suite
*
*                          (c) Copyright 2003-2011; Micrium, Inc.; Weston, FL
*					modified by JDCC.
*               All rights reserved.  Protected by international copyright laws.
*
*               uC/CAN is provided in source form to registered licensees ONLY.  It is 
*               illegal to distribute this source code to any third party unless you receive 
*               written permission by an authorized Micrium representative.  Knowledge of 
*               the source code may NOT be used to develop a similar product.
*
*               Please help us continue to provide the Embedded community with the finest 
*               software available.  Your honesty is greatly appreciated.
*
*               You can contact us at www.micrium.com.
****************************************************************************************************
*/

/*
****************************************************************************************************
* Filename      : can_cfg.c
* Version       : V2.41.00
* Programmer(s) : E0, JDCC
****************************************************************************************************
*/

/*
****************************************************************************************************
*                                             INCLUDES
****************************************************************************************************
*/
#include "can_cfg.h"                                  /* CAN abstraction module configuration     */
#include "can_sig.h"                                  /* CAN signal handling functions            */
#include "can_bus.h"                                  /* CAN bus handling function                */
#include "can_frm.h"                                  /* CAN frame handling function              */
#include "can_msg.h"                                  /* CAN msg handling function                */
#include "can_os.h"                                   /* CAN OS abstraction layer                 */
#include "drv_can.h"                                  /* CAN driver                               */

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
#if CANSIG_GRANULARITY == CAN_CFG_BIT
#define CAN_CFG_SIZE_MOD 8
#else
#define CAN_CFG_SIZE_MOD 1
#endif

#define CAN_CFG_LITTLE_ENDIAN 1

#if  CAN_CFG_LITTLE_ENDIAN == 1
#define CAN_CFG_ENDIAN  CANFRM_LITTLE_ENDIAN
#else
#define CAN_CFG_ENDIAN  CANFRM_BIG_ENDIAN
#endif

/*
****************************************************************************************************
*                                            LOCAL DATA
****************************************************************************************************
*/
/*------------------------------------------------------------------------------------------------*/
/*!
* \brief                     CAN SIGNAL CALLBACK DEFINITION
* \ingroup  UCCAN
*
*           Prototype of signal callback function. This is only an example to be replaced by
*           one or more user callback function.
*/
/*------------------------------------------------------------------------------------------------*/

#if CANSIG_CALLBACK_EN == 1
void CallBackFct(void* arg, CANSIG_VAL_T* value, CPU_INT32U CallbackId);
#endif

/*
****************************************************************************************************
*                                            GOBAL DATA
****************************************************************************************************
*/
/*------------------------------------------------------------------------------------------------*/
/*!
* \brief                     CAN BUS SEMAPHORES
* \ingroup  UCCAN
*
*           Allocation of semaphores for the buffering ressources. The following
*           implementation is an implementation for µC/OS-III.
*/
/*------------------------------------------------------------------------------------------------*/
OS_SEM    CANOS_TxSem[CANBUS_N];
OS_SEM    CANOS_RxSem[CANBUS_N];

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief                     CAN SIGNALS
* \ingroup  UCCAN
*
*           Allocation of can signals. This table must be modified by the user to define all signals
*           needed for tha application. The below defined signals are only examples and might be
*           removed.
*/
/*------------------------------------------------------------------------------------------------*/
const CANSIG_PARA CanSig[CANSIG_N] = {

                                            /*--- S_NODESTATUS */
   { CANSIG_UNCHANGED,                      /* Initial Status  */
      1,                                    /* Width in Bytes  */
      0,                                    /* Initial Value   */
#if CANSIG_CALLBACK_EN == 1
      StatusChange },                       /* Callback Func.  */
#else
    },
#endif

                                            /*------ S_CPULOAD */
   { CANSIG_UNCHANGED,                      /* Initial Status  */
      1,                                    /* Width in Bytes  */
      0,                                    /* Initial Value   */
#if CANSIG_CALLBACK_EN == 1
      0 }                                   /* No Callback     */
#else
    },
#endif
};


/*------------------------------------------------------------------------------------------------*/
/*!
* \brief                     CAN MESSAGES
* \ingroup  UCCAN
*
*           Allocation of can messages. This table must be modified by the user to define all messages
*           needed for tha application. The below defined messages are only examples and might be
*           removed.
*/
/*------------------------------------------------------------------------------------------------*/
const CANMSG_PARA CanMsg[CANMSG_N] = {
                                     /*------- M_STATUS */
   { 0x150L,                         /* CAN-Identifier  */
     CANMSG_TX,                      /* Message Type    */
     2,                              /* DLC of Message  */
     2,                              /* No. of Links    */
      { { S_NODESTATUS,              /* Signal ID       */
         0 },                        /* Byte Position   */
      { S_CPULOAD,                   /* Signal ID       */
       1 } } },                      /* Byte Position   */
                                     /*------ M_COMMAND */
   { 0x140L,                         /* CAN-Identifier  */
     CANMSG_RX,                      /* Message Type    */
     1,                              /* DLC of Message  */
     1,                              /* No. of Links    */
     { { S_NODESTATUS,               /* Signal ID       */
       0 } } }                       /* Byte Position   */
};

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief                     CAN SIGNAL DATA
* \ingroup  UCCAN
*
*           Allocation of global can signal table. This is the signal table on which CAN signal layer
*           will work. If the CANSIG_STATIC configuration is chosen it will be initialised with the
*           CanSig_Init() function otherwise it will be filled by calling CanSigCreate().
*/
/*------------------------------------------------------------------------------------------------*/

CANSIG_DATA CanSigTbl[CANSIG_N];

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief                      CAN BUS CONFIGURATION
* \ingroup  UCCAN
*
*           This structure contains the informations for a bus. A bus represents one
*           interface to the world.
*
*/
/*------------------------------------------------------------------------------------------------*/
const CANBUS_PARA CanCfg = 
{
    CAN_FALSE,                                        /* EXTENDED FLAG                           */
    100000L,                                         /* BAUDRATE                                */
    0,                                                /* BUS NODE                                */
    0,                                                /* BUS DEVICE                              */
                                                      /* DRIVER FUNCTIONS                        */
    LPC21XXCANInit,                                  /* Init                                  */
    LPC21XXCANOpen,                                  /* Open                                  */
    LPC21XXCANClose,                                 /* Close                                 */
    LPC21XXCANIoCtl,                                 /* IoCtl                                 */
    LPC21XXCANRead,                                  /* Read                                  */
    LPC21XXCANWrite,                                 /* Write                                 */
    {                                                 /* DRIVER IO FUNCTION CODES                */
        IO_LPC21XX_CAN_SET_BAUDRATE,                    /* Set baud rate                         */
        IO_LPC21XX_CAN_START,                           /* Start                                 */
        IO_LPC21XX_CAN_STOP,                            /* Stop                                  */
        IO_LPC21XX_CAN_RX_STANDARD,                     /* Rx Standard                           */
        IO_LPC21XX_CAN_RX_EXTENDED,                     /* Rx Extended                           */
        IO_LPC21XX_CAN_TX_READY,                        /* Tx ready                              */
        IO_LPC21XX_CAN_GET_NODE_STATUS,                 /* Get node status                       */
    }
};

/*
*********************************************************************************************************
*                                       CAN BUS NODE STATUS HOOK
*
* Description: This function is called by CanBusNSHandler().
*
* Arguments  : bus Id
*********************************************************************************************************
*/
#if CANBUS_HOOK_NS_EN == 1
void CanBusNSHook (CPU_INT16S busId)
{
    (void)busId;                                      /* prevent compiler warning                 */
}
#endif

/*
*********************************************************************************************************
*                                       CAN BUS RX HOOK
*
* Description: This function is called by CanBusRxHandler().
*
* Arguments  : bus Id
*********************************************************************************************************
*/
#if CANBUS_HOOK_RX_EN == 1
CPU_INT16S CanBusRxHook (CPU_INT16S busId, void *buffer)
{
    (void)busId;                                      /* prevent compiler warning                 */
    (void)buffer;                                     /* prevent compiler warning                 */

    return 0;
}
#endif


/*
*********************************************************************************************************
*                                       CALLBACK FUNCTION
*
* Description: This function is the callback function example of the CAN signal layer. It
*              has to be replaced by one or more user callback functions.
*
* Arguments  : arg          pointer to signal
*              value        of signal
*              CallbackId   to identify where this callback function was called from
*********************************************************************************************************
*/
#if CANSIG_CALLBACK_EN == 1
void CallBackFct(void* arg, CANSIG_VAL_T* value, CPU_INT32U CallbackId)
{
    (void)CallbackId;                                 /* prevent compiler warning                 */
    (void)value;                                      /* prevent compiler warning                 */
    (void)arg;                                        /* prevent compiler warning                 */

}
#endif
