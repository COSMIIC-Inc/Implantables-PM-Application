/*
*********************************************************************************************************
*                                                uC/CRC
*           ERROR DETECTING CODE (EDC) & ERROR CORRECTING CODE (ECC) CALCULATION UTILITIES
*
*                         (c) Copyright 2007-2010; Micrium, Inc.; Weston, FL
*
*               All rights reserved. Protected by international copyright laws.
*
*               uC/FS is provided in source form to registered licensees ONLY.  It is
*               illegal to distribute this source code to any third party unless you receive
*               written permission by an authorized Micrium representative.  Knowledge of
*               the source code may NOT be used to develop a similar product.
*
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*
*               You can contact us at www.micrium.com.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                      BINARY BOSE-CHAUDHURI-HOCQUENGHEM (BCH) CODE CALCULATION
*
*                               MODEL FOR 8-BIT ECC FOR 512-BYTE BUFFER
*
* Filename      : ecc_bch_8bit.h
* Version       : V1.07.00
* Programmer(s) : BAN
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef   ECC_BCH_8BIT_PRESENT
#define   ECC_BCH_8BIT_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>

#include  <lib_def.h>
#include  <lib_mem.h>

#include  <crc_cfg.h>

#include  <ecc.h>


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef    ECC_BCH_8BIT_MODULE
#define   ECC_BCH_8BIT_EXT
#else
#define   ECC_BCH_8BIT_EXT  extern
#endif


/*
*********************************************************************************************************
*                                        DEFAULT CONFIGURATION
*********************************************************************************************************
*/

#ifndef  ECC_BCH_8BIT_CFG_ARG_CHK_EXT_EN
#define  ECC_BCH_8BIT_CFG_ARG_CHK_EXT_EN         DEF_ENABLED
#endif

#ifndef  ECC_BCH_8BIT_CFG_OPTIMIZE_ASM_EN
#define  ECC_BCH_8BIT_CFG_OPTIMIZE_ASM_EN        DEF_DISABLED
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  ECC_MODULE  BCH_ECC_8Bit;


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void        BCH_8Bit_Calc   (void          *p_buf,              /* Calc BCH code for 512- to 528-byte buffer.           */
                             CPU_SIZE_T     len,
                             CPU_INT08U    *p_ecc,
                             CPU_ERR       *p_err);

void        BCH_8Bit_CalcEx (void          *p_buf,              /* Calc BCH code for 512- to 528-byte buffer.           */
                             CPU_SIZE_T     len,
                             CPU_INT08U    *p_ecc_chk,
                             CPU_INT08U    *p_ecc,
                             CPU_ERR       *p_err);

CPU_INT08U  BCH_8Bit_Chk    (void          *p_buf,              /* Chk BCH code for 512- to 528-byte buffer.            */
                             CPU_SIZE_T     len,
                             CPU_INT08U    *p_ecc,
                             ECC_ERR_ADDR   err_addr_tbl[],
                             CPU_INT08U     max_errs,
                             CPU_ERR       *p_err);

void        BCH_8Bit_Correct(void          *p_buf,              /* Correct errors in 512- to 528-byte buffer.           */
                             CPU_SIZE_T     len,
                             CPU_INT08U    *p_ecc,
                             CPU_ERR       *p_err);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  ECC_BCH_8BIT_CFG_ARG_CHK_EXT_EN
#error  "ECC_BCH_8BIT_CFG_ARG_CHK_EXT_EN        not #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "

#elif  ((ECC_BCH_8BIT_CFG_ARG_CHK_EXT_EN != DEF_DISABLED) && \
        (ECC_BCH_8BIT_CFG_ARG_CHK_EXT_EN != DEF_ENABLED ))
#error  "ECC_BCH_8BIT_CFG_ARG_CHK_EXT_EN  illegally #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "
#endif



#ifndef  ECC_BCH_8BIT_CFG_OPTIMIZE_ASM_EN
#error  "ECC_BCH_8BIT_CFG_OPTIMIZE_ASM_EN       not #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "
#elif  ((ECC_BCH_8BIT_CFG_OPTIMIZE_ASM_EN != DEF_ENABLED ) && \
        (ECC_BCH_8BIT_CFG_OPTIMIZE_ASM_EN != DEF_DISABLED))
#error  "ECC_BCH_8BIT_CFG_OPTIMIZE_ASM_EN illegally #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of BCH 8BIT module include.                      */
