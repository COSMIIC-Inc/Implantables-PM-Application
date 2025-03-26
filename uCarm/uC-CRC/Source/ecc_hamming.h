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
*                                      HAMMING CODE CALCULATION
*
* Filename      : ecc_hamming.h
* Version       : V1.07.00
* Programmer(s) : FBJ
*                 BAN
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef   ECC_HAMMING_PRESENT
#define   ECC_HAMMING_PRESENT


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

#ifdef   ECC_HAMMING_MODULE
#define  ECC_HAMMING_EXT
#else
#define  ECC_HAMMING_EXT  extern
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                        DEFAULT CONFIGURATION
*********************************************************************************************************
*/

#ifndef  ECC_HAMMING_CFG_ARG_CHK_EXT_EN
#define  ECC_HAMMING_CFG_ARG_CHK_EXT_EN          DEF_ENABLED
#endif

#ifndef  ECC_HAMMING_CFG_ARG_CHK_DBG_EN
#define  ECC_HAMMING_CFG_ARG_CHK_DBG_EN          DEF_ENABLED
#endif

#ifndef  ECC_HAMMING_CFG_OPTIMIZE_ASM_EN
#define  ECC_HAMMING_CFG_OPTIMIZE_ASM_EN         DEF_DISABLED
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

extern  const  ECC_MODULE  Hamming_ECC;

extern  const  ECC_MODULE  Hamming_ECC_16;

extern  const  ECC_MODULE  Hamming_ECC_128;

extern  const  ECC_MODULE  Hamming_ECC_256;

extern  const  ECC_MODULE  Hamming_ECC_512;


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/

/*$PAGE*/
/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* ------------------ 256-BYTE BUFFER ----------------- */
void        Hamming_Calc_256         (void          *p_buf,     /* Calc Hamming code for 256-byte buffer.               */
                                      CPU_SIZE_T     len,
                                      CPU_INT08U    *p_ecc,
                                      CPU_ERR       *p_err);

CPU_INT08U  Hamming_Chk_256          (void          *p_buf,     /* Chk Hamming code for 256-byte buffer.                */
                                      CPU_SIZE_T     len,
                                      CPU_INT08U    *p_ecc,
                                      ECC_ERR_ADDR   err_addr_tbl[],
                                      CPU_INT08U     max_errs,
                                      CPU_ERR       *p_err);

void        Hamming_Correct_256      (void          *p_buf,     /* Correct errors in 256-byte buffer.                   */
                                      CPU_SIZE_T     len,
                                      CPU_INT08U    *p_ecc,
                                      CPU_ERR       *p_err);


                                                                /* ------------------ GENERAL LENGTH ------------------ */
void        Hamming_Calc             (void          *p_buf,     /* Calculate Hamming code.                              */
                                      CPU_SIZE_T     len,
                                      CPU_INT08U    *p_ecc,
                                      CPU_ERR       *p_err);

CPU_INT08U  Hamming_Chk              (void          *p_buf,     /* Check Hamming code.                                  */
                                      CPU_SIZE_T     len,
                                      CPU_INT08U    *p_ecc,
                                      ECC_ERR_ADDR   err_addr_tbl[],
                                      CPU_INT08U     max_errs,
                                      CPU_ERR       *p_err);

void        Hamming_Correct          (void          *p_buf,     /* Correct errors.                                      */
                                      CPU_SIZE_T     len,
                                      CPU_INT08U    *p_ecc,
                                      CPU_ERR       *p_err);

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*                                      DEFINED IN hamming_a.asm
*********************************************************************************************************
*/

#if (ECC_HAMMING_CFG_OPTIMIZE_ASM_EN == DEF_ENABLED)
void        Hamming_ParCalcBitWord_32(void         *p_buf,      /* Calc bit- & word-wise par 32-bit words.              */
                                      CPU_INT32U   *p_par_bit,
                                      CPU_INT32U   *p_par_word);
#endif


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  ECC_HAMMING_CFG_ARG_CHK_EXT_EN
#error  "ECC_HAMMING_CFG_ARG_CHK_EXT_EN         not #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "

#elif  ((ECC_HAMMING_CFG_ARG_CHK_EXT_EN != DEF_DISABLED) && \
        (ECC_HAMMING_CFG_ARG_CHK_EXT_EN != DEF_ENABLED ))
#error  "ECC_HAMMING_CFG_ARG_CHK_EXT_EN   illegally #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "
#endif



#ifndef  ECC_HAMMING_CFG_OPTIMIZE_ASM_EN
#error  "ECC_HAMMING_CFG_OPTIMIZE_ASM_EN        not #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "
#elif  ((ECC_HAMMING_CFG_OPTIMIZE_ASM_EN != DEF_ENABLED ) && \
        (ECC_HAMMING_CFG_OPTIMIZE_ASM_EN != DEF_DISABLED))
#error  "ECC_HAMMING_CFG_OPTIMIZE_ASM_EN  illegally #define'd in 'app_cfg.h'"
#error  "                                 [MUST be DEF_ENABLED ]            "
#error  "                                 [     || DEF_DISABLED]            "
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of HAMMING module include.                       */
