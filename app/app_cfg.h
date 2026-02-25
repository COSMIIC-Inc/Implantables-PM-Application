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
*
*                                      APPLICATION CONFIGURATION
*					NNP Power Module
*                                    
*
* Filename      : app_cfg.h
* Version       : V3.20.01
* Programmer(s) : CM/JDCC
*********************************************************************************************************
*/

#ifndef  __APP_CFG_H__
#define  __APP_CFG_H__


/*
*********************************************************************************************************
*                                       MODULE ENABLE / DISABLE
*********************************************************************************************************
*/
/* DEF_ENABLED = Present, DEF_DISABLED = Not Present         */
#define  APP_CFG_FS_EN                             DEF_DISABLED
#define  APP_CFG_USB_DEV_EN                        DEF_DISABLED
#define  APP_CFG_USB_HOST_EN                       DEF_DISABLED
#define  APP_CFG_USB_OTG_EN                        DEF_DISABLED
#define  APP_CFG_CODE_FLASH_EN		           DEF_ENABLED
#define  APP_CFG_USB_OTG_EN                        DEF_DISABLED
#define  APP_CFG_USB_HOST_EN                       DEF_DISABLED
#define  uC_LCD_MODULE   			   DEF_DISABLED




/*
*********************************************************************************************************
*                                            TASK PRIORITIES
*********************************************************************************************************
*/

/* $$$$ Declare here other tasks priority related to your application */

#define  RUNCANSERVER_PRIO                             4  /* incoming CAN messages */
#define  RUNCANTIMER_PRIO                              3  /* CAN timer Alarms */
#define  SLEEP_PRIO                                    1  /* sleep event */
#define  RUNIOSCAN_TASK_PRIO                           5  /* background I2C updates  */
#define  APP_TASK_START_PRIO                           2  /* run gateway task  */
#define  RUNSCRIPT_TASK_PRIO                           7  /* run script task - lower priority than tick task (6) */
                                  /* TICK_TASK PRIORITY =6 (See: os_cfg_app.h)*/
#define  OS_TASK_TMR_PRIO                       (OS_CFG_PRIO_MAX - 2)


/*
*********************************************************************************************************
*                                            TASK STACK SIZES
*********************************************************************************************************
*/

/* $$$$ Declare here other tasks size related to your application -- stack size is in 4 byte words */
//JML Note: To determine necessary stack space: 
// 1. Enable Statistics Task (requires ~500 Bytes free RAM)
// 2. While Debugging, place the following items in Watch
//      AppTaskStartTCB.StkUsed
//      RunCanServerTaskTCB.StkUsed
//      RunCanTimerTaskTCB.StkUsed
//      SleepTaskTCB.StkUsed
//      RunIOScanTaskTCB.StkUsed
//      RunScriptTCB.StkUsed
//      OSIdleTaskTCB.StkUsed
//      OSTickTaskTCB.StkUsed
//      OSStatTaskTCB.StkUsed
// 3. Run program through tests, pausing program occasionally to watch stack usage
// 4. Apply safety factor of >1.5

#define  RUNCANSERVER_STK_SIZE                          160 
#define  RUNCANTIMER_STK_SIZE                           200  
#define  SLEEP_STK_SIZE                                  80 
#define  RUNIOSCAN_STK_SIZE                             110 
#define  APP_TASK_START_STK_SIZE                        500  
#define  RUNSCRIPT_STK_SIZE                             500


///*
//*********************************************************************************************************
//*                                       uC/FS: DRIVER CONFIGURATION
//*********************************************************************************************************
//*/
//
//#define  APP_FS_DRV_NONE                             0            /* None     driver.                                         */
//#define  APP_FS_DRV_RAM_DISK                         1            /* RAM Disk driver                                          */
//#define  APP_FS_DRV_SD                               2            /* SD Card  driver                                          */
//#define  APP_FS_DRV_USB_HOST                         3            /* USB Host driver                                          */
//
//#define  APP_CFG_FS_DRV_SEL                   APP_FS_DRV_USB_HOST
//
//#define  APP_CFG_FS_FORMAT_MAX_NBR_TRIES            10
//
//#define  APP_CFG_FS_ALLOC_SIZE                    8192  	      /* Memory pool size used for semi-dynamic allocation         */
//#define  APP_CFG_FS_RAM_DISK_SECTORS                64            /* RAM Disk number of sectors                                */
//#define  APP_CFG_FS_RAM_DISK_SECTOR_SIZE           512            /* RAM Disk sector size                                      */
//#define  APP_CFG_FS_NBR_TEST                        10            /* Number of files to be created                             */
//
///*
//*********************************************************************************************************
//*                                        uC/LIB CONFIGURATION
//*********************************************************************************************************
//*/
//
////#define  LIB_MEM_CFG_OPTIMIZE_ASM_EN            DEF_ENABLED     /* DEF_ENABLED = Mem_Copy defined in lib_mem_a.asm, otherwise lib_mem.c */
//#define  LIB_MEM_CFG_ARG_CHK_EXT_EN             DEF_DISABLED    /* ^^^ changed from enabled */
//#define  LIB_MEM_CFG_ALLOC_EN                   DEF_DISABLED    /* ^^^ changed from enabled */
//#define  LIB_MEM_CFG_POOL_NBR                   10
//#define  LIB_MEM_CFG_HEAP_SIZE                  1024u          /* ^^^ was 302000L */
//#define  LIB_STR_CFG_FP_EN                      DEF_DISABLED
//
///*
//*********************************************************************************************************
//*                                    BSP CONFIGURATION
//* Note(s) : (1) BSP_CFG_SER_COMM_SEL defines the UART to be used to output the system state:
//*
//*                   BSP_SER_COMM_UART_00           UART #0  is selected to output the system state
//*                   BSP_SER_COMM_UART_01           UART #1  is selected to output the system state
//*            
//*********************************************************************************************************
//*/
//
//#define  BSP_CFG_SER_COMM_SEL                      BSP_SER_COMM_UART_00
//#define  uC_CFG_OPTIMIZE_ASM_EN                    DEF_ENABLED
//
//
///*
//*********************************************************************************************************
//*                                 uC/USB-DEVICE LPC2xxx DRIVER CONFIGURATION
//*
//* Note(s):  (1) USBF_LPC2xxx_CFG_EXT_ATX_EN    defines if the USB controller needs an external USB Analog transceiver.
//*
//*           (2) USBF_LPC2xxx_CFG_REG_BASE_ADDR defines the base address of the USB controller.
//*
//*           (3) USBF_LPC2xxx_CFG_EP_RAM_SIZE_MAX
//*********************************************************************************************************
//*/
//
//#define  USBF_LPC2xxx_CFG_EXT_ATX_EN               DEF_DISABLED
//#define  USBF_LPC2xxx_CFG_REG_BASE_ADDR            0xFFE0C200
//#define  USBF_LPC2xxx_CFG_EP_RAM_SIZE_MAX              4096
//
//
//
///*
//*********************************************************************************************************
//*                                          uC/LCD CONFIGURATION
//*********************************************************************************************************
//*/
//
//#define  DISP_BUS_WIDTH                                   4  /* LCD controller is accessed with a 4 bit bus */
//
///*
//*********************************************************************************************************
//*                                     TRACE / DEBUG CONFIGURATION
//*********************************************************************************************************
//*/
//
////#define  TRACE_LEVEL_OFF                               0
////#define  TRACE_LEVEL_INFO                              1
////#define  TRACE_LEVEL_DBG                               2
//
//#define  APP_CFG_TRACE_LEVEL                     TRACE_LEVEL_OFF
//#define  APP_CFG_TRACE                           printf
//
//#define  APP_TRACE_INFO(x)               ((APP_CFG_TRACE_LEVEL >= TRACE_LEVEL_INFO)  ? (void)(APP_CFG_TRACE x) : (void)0)
//#define  APP_TRACE_DBG(x)                ((APP_CFG_TRACE_LEVEL >= TRACE_LEVEL_DBG)   ? (void)(APP_CFG_TRACE x) : (void)0)
//
//#define  BSP_CFG_TRACE                             BSP_Ser_Printf
//#define  BSP_CFG_TRACE_LEVEL                       TRACE_LEVEL_INFO
//#define  BSP_TRACE_INFO(x)                        ((BSP_CFG_TRACE_LEVEL  >= TRACE_LEVEL_INFO) ? (void)(BSP_CFG_TRACE x) : (void)0)
//#define  BSP_TRACE_DBG(x)                         ((BSP_CFG_TRACE_LEVEL  >= TRACE_LEVEL_DBG)  ? (void)(BSP_CFG_TRACE x) : (void)0)
//
//
///*
//*********************************************************************************************************
//*                                       uC/USB-DEVICE CONFIGURATION
//*********************************************************************************************************
//*/
//
//#define  APP_CFG_USB_DEV_BULK_EN                   DEF_DISABLED       
//#define  APP_CFG_USB_DEV_CDC_EN                    DEF_DISABLED
//#define  APP_CFG_USB_DEV_HID_EN                    DEF_DISABLED
//#define  APP_CFG_USB_DEV_MSC_EN                    DEF_DISABLED
//
//                                                                /* ------- USB DEVICE DEMO BULK TEST DEFINITION ------ */
//#define  APP_USB_DEV_BULK_TEST_ECHO1                      1
//#define  APP_USB_DEV_BULK_TEST_ECHO2                      2
//#define  APP_USB_DEV_BULK_TEST_ECHO3                      3
//#define  APP_USB_DEV_BULK_TEST_SPEED                      4  
//#define  APP_USB_DEV_BULK_TEST_MICS                       5  
//#define  APP_USB_DEV_BULK_TEST_CAN                        6  
//
//#define  APP_CFG_USB_DEV_BULK_MAX_BUF_SIZE             1024
//
//#define  APP_CFG_USB_DEV_BULK_TEST_SEL          APP_USB_DEV_BULK_TEST_CAN  //^^APP_USB_DEV_BULK_TEST_ECHO3
//
//                    /* ------- USB DEVICE DEMO BULK TEST DEFINITION ------ */
//#define  APP_USB_DEV_HID_TEST_MOUSE                       1
//#define  APP_USB_DEV_HID_SIMPLE_RD_WR                     2
//
//#define  APP_CFG_USB_DEV_HID_TEST_SEL              APP_USB_DEV_HID_TEST_MOUSE
//
//#define  USBF_RAMDISK_CFG_BLK_SIZE                      512
//#define  USBF_RAMDISK_CFG_NBR_BLKS                       32
//#define  USBF_RAMDISK_CFG_NBR_UNITS                       1
//
//
//
///*
//*********************************************************************************************************
//*                                      CONFIGURATION ERRORS
//*********************************************************************************************************
//*/
//
///* $$$$ Other configuration errors can be added */
//
//#if   (APP_USB_DEV_HID_SIMPLE_RD_WR == DEF_ENABLED) 
//
//#if   (USBF_HID_CFG_OPTIONAL_INTR_OUT_EP  == DEF_DISABLED)
//    #error  "If APP_USB_DEV_HID_SIMPLE_RD_WR chosen for HID demo:                       "
//    #error  "USBF_HID_CFG_OPTIONAL_INTR_OUT_EP      must be DEF_ENABLED in 'usbf_cfg.h' "
//#endif
//#endif
//
//#if ((APP_CFG_USB_HOST_EN == DEF_ENABLED) && (APP_CFG_USB_HOST_DEMO == APP_USB_HOST_DEMO_MSC))
//#if (APP_CFG_FS_EN != DEF_ENABLED)
//#error  "MSC demo chosen.  APP_CFG_FS_EN must be DEF_ENABLED           "
//#endif
//#endif
//

#endif
	 	 			 		    	 				 	  	 	 	 	 		  	 	  	    	      	   		 	 	 		  		  	 		 	   	  		      		    		 	       	   	  		      		      		  	  			  				 	  	   		  	   		  				 	      		    	 		       	   	 			   		   	 		  	 	  	  	 	 					 	   	   	 	 	  		  		 			 	  	 		 		   		 	 	  		 	 		 	  	 		  	 		  	 	 		    	 					 	 	 		  	  	  		      		      		  	  			  	 		 	  	   		      		  	 	 		   	  		  
