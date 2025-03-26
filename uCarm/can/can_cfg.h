/*
****************************************************************************************************
*                                               uC/CAN
*                                       The Embedded CAN suite
*
*                          (c) Copyright 2003-2011; Micrium, Inc.; Weston, FL
*
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
* Filename      : can_cfg.h
* Version       : V2.41.00
* Programmer(s) : E0
****************************************************************************************************
*/

#ifndef _CAN_CFG_H_
#define _CAN_CFG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
****************************************************************************************************
*                                           CONFIGURATION
****************************************************************************************************
*/
/*------------------------------------------------------------------------------------------------*/
/*                               C O M M O N  D E F I N E S                                       */
/*------------------------------------------------------------------------------------------------*/

#define CAN_CFG_BIT               0                   /* Definiton for CANSIG_GRANULARITY ..      */
#define CAN_CFG_BYTE              1                   /* .. choose bit or byte                    */

#ifndef CAN_FALSE
#define CAN_FALSE                 0
#endif

#ifndef CAN_TRUE
#define CAN_TRUE                  1
#endif

#ifndef NULL_PTR
#define NULL_PTR                  (void *)0
#endif

/*------------------------------------------------------------------------------------------------*/
/*                                      C A N   B U S                                             */
/*------------------------------------------------------------------------------------------------*/

#define CANBUS_EN                 1                   /*   Enable CAN bus management              */
#define CANBUS_N                  1                   /*   Number of busses                       */
#define CANBUS_ARG_CHK_EN         1                   /*   Enable runtime argument checking       */
#define CANBUS_TX_HANDLER_EN      1                   /*   Enable usage of CanBusTxHandler        */
#define CANBUS_RX_HANDLER_EN      1                   /*   Enable usage of CanBusRxHandler        */
#define CANBUS_NS_HANDLER_EN      1                   /*   Enable usage of CanBusNsHandler        */
#define CANBUS_STAT_EN            0                   /*   Enable bus statistic                   */
#define CANBUS_TX_QSIZE          (3 * CANBUS_N)       /*   Transmit queue size in CAN frames for  */ 
                                                      /*   each can bus                           */
  //JML note: TX_Q_SIZE should be 2 or more.  PM may sometimes send out a SYNC and SDO nearly 
  //          simultaneously. Each Q adds 16bytes to RAM
#define CANBUS_RX_QSIZE          (4 * CANBUS_N)       /*   Receive queue size in CAN frames for   */ 
                                                      /*   each can bus                           */ 
  //JML note: RX_Q_SIZE should be 3 or more (as high as possible).  PM may receive Heartbeats, PDOs, 
  //          and SDOs nearly simultaneously.  Each Q adds 16bytes to RAM
#define CANBUS_HOOK_NS_EN         1                   /*   Enable node status handler hook func.  */
#define CANBUS_HOOK_RX_EN         0                   /*   Enable rx handler hook function        */
#define CANBUS_RX_READ_ALWAYS_EN  1                   /*   If enabled the rx handler executes a   */
                                                      /*   read even when frames can't be allocated */

/*------------------------------------------------------------------------------------------------*/
/*                                   C A N   M E S S A G E                                        */
/*------------------------------------------------------------------------------------------------*/

#define CANMSG_EN                 1                   /* Enable CAN message support               */
#define CANMSG_N                  2                   /*   Number of messages                     */
#define CANMSG_ARG_CHK_EN         1                   /*   Enable runtime argument checking       */

/*------------------------------------------------------------------------------------------------*/
/*                                    C A N   S I G N A L                                         */
/*------------------------------------------------------------------------------------------------*/

#define CANSIG_EN                 1                   /* Enable CAN signal database               */
#define CANSIG_N                  2                   /*   Number of signals                      */
#define CANSIG_ARG_CHK_EN         1                   /*   Enable runtime argument checking       */
#define CANSIG_MAX_WIDTH          4                   /*   Maximal signal width in byte           */
#define CANSIG_GRANULARITY        CAN_CFG_BYTE        /*   Set signal resolution to byte          */
#define CANSIG_STATIC_CONFIG      1                   /*   To reduce memory usage, declare a      */
                                                      /*   staticsignal table                     */
#define CANSIG_USE_DELETE         0                   /*   To reduce memory usage don't use       */
                                                      /*   delete funktions for signal            */
#define CANSIG_CALLBACK_EN        0                   /*   ^^^1 Enable callback functions              */

/*------------------------------------------------------------------------------------------------*/
/*                                    C A N   F R A M E                                           */
/*------------------------------------------------------------------------------------------------*/

#define CANFRM_ARG_CHK_EN         0                   /* Enable runtime argument checking         */

/*------------------------------------------------------------------------------------------------*/
/*                                    C A N   O S                                                 */
/*------------------------------------------------------------------------------------------------*/

#define CANOS_ARG_CHK_EN         0                    /* Enable runtime argument checking         */

/*
****************************************************************************************************
*                                   APPLICATION SPECIFIC DEFINES
****************************************************************************************************
*/
#define COUNTER_SIZE      4
#define TX_MSG_ID         0x7FFL

enum {
   S_NODESTATUS = 0, S_CPULOAD, S_MAX
};
enum {
   M_STATUS = 0, M_COMMAND, M_MAX
};

/*
****************************************************************************************************
*                                          ERROR SECTION OS
****************************************************************************************************
*/

#if (CANOS_ARG_CHK_EN < 0) || (CANOS_ARG_CHK_EN > 1)
#error "CANOS_ARG_CHK_EN is invalid; check definition to be 0 or 1!"
#endif

/*
****************************************************************************************************
*                                          ERROR SECTION FRAME
****************************************************************************************************
*/

#if (CANFRM_ARG_CHK_EN < 0) || (CANFRM_ARG_CHK_EN > 1)
#error "CANFRM_ARG_CHK_EN is invalid; check definition to be 0 or 1!"
#endif

/*
****************************************************************************************************
*                                          ERROR SECTION SIGNALS
****************************************************************************************************
*/

#if (CANSIG_EN < 0) || (CANSIG_EN > 1)
#error "CANSIG_EN is invalid; check definition to be 0 or 1!"
#endif

#if (CANSIG_N < 1) || (CANSIG_N > 32767)
#error "CANSIG_N is invalid; check definition to be in range 1 ... 32767!"
#endif

#if (CANSIG_ARG_CHK_EN < 0) || (CANSIG_ARG_CHK_EN > 1)
#error "CANSIG_ARG_CHK_EN is invalid; check definition to be 0 or 1!"
#endif

#if (CANSIG_MAX_WIDTH != 1) && (CANSIG_MAX_WIDTH != 2) && (CANSIG_MAX_WIDTH != 4)
#error "CANSIG_MAX_WIDTH is invalid; check definition to be 1, 2 or 4!"
#endif

#if (CANSIG_GRANULARITY < 0) || (CANSIG_GRANULARITY > 1)
#error "CANSIG_GRANULARITY is invalid; check definition to be 0 or 1!"
#endif

#if (CANSIG_STATIC_CONFIG < 0) || (CANSIG_STATIC_CONFIG > 1)
#error "CANSIG_STATIC_CONFIG is invalid; check definition to be 0 or 1!"
#endif

#if (CANSIG_USE_DELETE < 0) || (CANSIG_USE_DELETE > 1)
#error "CANSIG_USE_DELETE is invalid; check definition to be 0 or 1!"
#endif

#if (CANSIG_CALLBACK_EN < 0) || (CANSIG_CALLBACK_EN > 1)
#error "CANSIG_CALLBACK_EN is invalid; check definition to be 0 or 1!"
#endif


/*
****************************************************************************************************
*                                          ERROR SECTION MSGS
****************************************************************************************************
*/

#if CANMSG_EN < 0 || CANMSG_EN > 1
#error "CANMSG_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANMSG_N < 1 || CANMSG_N > 32767
#error "CANMSG_N is invalid; check definition to be in range 1 ... 32767!"
#endif

#if CANMSG_ARG_CHK_EN < 0 || CANMSG_ARG_CHK_EN > 1
#error "CANMSG_ARG_CHK_EN is invalid; check definition to be 0 or 1!"
#endif


/*
****************************************************************************************************
*                                          ERROR SECTION CAN BUS
****************************************************************************************************
*/

#if CANBUS_EN < 0 || CANBUS_EN > 1
#error "CANBUS_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_N < 1
#error "CANBUS_N is invalid; check definition to be greater than 0!"
#endif

#if CANBUS_ARG_CHK_EN < 0 || CANBUS_ARG_CHK_EN > 1
#error "CANBUS_ARG_CHK_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_TX_HANDLER_EN < 0 || CANBUS_TX_HANDLER_EN > 1
#error "CANBUS_TX_HANDLER_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_RX_HANDLER_EN < 0 || CANBUS_RX_HANDLER_EN > 1
#error "CANBUS_RX_HANDLER_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_NS_HANDLER_EN < 0 || CANBUS_NS_HANDLER_EN > 1
#error "CANBUS_NS_HANDLER_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_STAT_EN < 0 || CANBUS_STAT_EN > 1
#error "CANBUS_STAT_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_RX_QSIZE < 1
#error "CANBUS_RX_QSIZE is invalid; check definition to be greater than 0!"
#endif

#if CANBUS_TX_QSIZE < 1
#error "CANBUS_TX_QSIZE is invalid; check definition to be greater than 0!"
#endif

#if CANBUS_HOOK_RX_EN < 0 || CANBUS_HOOK_RX_EN > 1
#error "CANBUS_HOOK_RX_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_HOOK_NS_EN < 0 || CANBUS_HOOK_NS_EN > 1
#error "CANBUS_HOOK_NS_EN is invalid; check definition to be 0 or 1!"
#endif

#if CANBUS_RX_READ_ALWAYS_EN < 0 || CANBUS_RX_READ_ALWAYS_EN > 1
#error "CANBUS_RX_READ_ALWAYS_EN is invalid; check definition to be 0 or 1!"
#endif

#ifdef __cplusplus
}
#endif

#endif  /* #ifndef _CAN_CFG_H_ */

