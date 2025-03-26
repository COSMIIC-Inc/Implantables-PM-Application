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
*                               ERROR CORRECTING CODE (ECC) DEFINITIONS
*
* Filename      : ecc.h
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

#ifndef   ECC_PRESENT
#define   ECC_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef    ECC_MODULE
#define   ECC_EXT
#else
#define   ECC_EXT  extern
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       ECC ERROR CODES DEFINES
*********************************************************************************************************
*/

#define  ECC_ERR_NONE                                      0u   /* No error.                                            */
#define  ECC_ERR_CORRECTABLE                               1u   /* Correctable error detected in data.                  */
#define  ECC_ERR_ECC_CORRECTABLE                           2u   /* Correctable error detected in ECC.                   */
#define  ECC_ERR_INVALID_ARG                               3u   /* Argument passed invalid value.                       */
#define  ECC_ERR_INVALID_LEN                               4u   /* Length argument passed invalid length.               */
#define  ECC_ERR_NULL_PTR                                  5u   /* Pointer argument passed NULL pointer.                */
#define  ECC_ERR_UNCORRECTABLE                             6u   /* Uncorrectable error detected in data.                */


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       ERROR ADDRESS DATA TYPE
*********************************************************************************************************
*/

typedef  struct  ecc_err_addr {
    CPU_INT08U  AddrBit;
    CPU_SIZE_T  AddrOctet;
} ECC_ERR_ADDR;

/*
*********************************************************************************************************
*                                        ECC MODULE DATA TYPE
*********************************************************************************************************
*/

typedef  void        (*ECC_CALC_FNCT)   (void          *p_buf,  /* Calc ECC.                                            */
                                         CPU_SIZE_T     len,
                                         CPU_INT08U    *p_ecc,
                                         CPU_ERR       *p_err);

typedef  CPU_INT08U  (*ECC_CHK_FNCT)    (void          *p_buf,  /* Chk ECC.                                             */
                                         CPU_SIZE_T     len,
                                         CPU_INT08U    *p_ecc,
                                         ECC_ERR_ADDR   err_addr_tbl[],
                                         CPU_INT08U     max_errs,
                                         CPU_ERR       *p_err);

typedef  void        (*ECC_CORRECT_FNCT)(void          *p_buf,  /* Correct ECC errs.                                    */
                                         CPU_SIZE_T     len,
                                         CPU_INT08U    *p_ecc,
                                         CPU_ERR       *p_err);

typedef  struct  ecc_module {
    CPU_SIZE_T        BufLenMin;
    CPU_SIZE_T        BufLenMax;
    CPU_INT08U        ECC_Len;
    ECC_CALC_FNCT     Calc;
    ECC_CHK_FNCT      Chk;
    ECC_CORRECT_FNCT  Correct;
} ECC_MODULE;


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


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


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of ECC module include.                           */
