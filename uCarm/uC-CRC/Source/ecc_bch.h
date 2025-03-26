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
* Filename      : ecc_bch.h
* Version       : V1.07.00
* Programmer(s) : BAN
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This header file is protected from multiple pre-processor inclusion through use of the
*               BCH present pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef   ECC_BCH_PRESENT
#define   ECC_BCH_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <app_cfg.h>
#include  <cpu.h>
#include  <lib_def.h>
#include  <lib_mem.h>

#include  <ecc.h>


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef    ECC_BCH_MODULE
#define   ECC_BCH_EXT
#else
#define   ECC_BCH_EXT  extern
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
*                                BCH GALOIS FIELD POLYNOMIAL DATA TYPE
*
* Note(s) : (a) A BCH Galois field polynomial is encoded in bit-form :
*
*                   0 0 0 0 1 0 0 0 1 1 0 1 0 1 1
*                   ^ ^                       ^ ^
*                   | |                       | |
*                   | |                       | +------ constant
*                   | |                       +-------- coefficient of X
*                   | |                                     .  .  .
*                   | |                                     .  .  .
*                   | +-------------------------------- coefficient of X^14
*                   +---------------------------------- coefficient of X^15
*
*           (b) Since type 'CPU_INT16U' can only encode polynomials of degree 15 & lower, BCH & GF
*               calculations are limited to buffers of 2^15 bits & extension fields of GF(2) up to
*               GF(2^15), respectively.
*********************************************************************************************************
*/

typedef  CPU_INT16U  BCH_GF_POLY;

/*
*********************************************************************************************************
*                             BCH GALOIS FIELD POLYNOMIAL POWER DATA TYPE
*
* Note(s) : (a) The Galois field GF(2^m) contains 2^m - 1 non-zero elements; these may be represented by
*               the powers 1, alpha, ..., alpha^(2^m - 3), alpha^(2^m - 2).  'BCH_GF_POLY_PWR' must be
*               configured large enough to hold the maximum power (as well as the maximum quantity of
*               Galois field elements).
*
*               (1) Since the maximum m is 15, the maximum (2^m - 1) is 32767.
*********************************************************************************************************
*/

typedef  CPU_INT16U  BCH_GF_POLY_PWR;


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

void        BCH_GF_Construct          (CPU_INT08U        m,         /* Construct Galois field GF(2^m).                  */
                                       BCH_GF_POLY      *p_gf);

void        BCH_GF_MakeLogTbl         (CPU_INT08U        m,         /* Make logarithm table for Galois field.           */
                                       BCH_GF_POLY      *p_gf,
                                       BCH_GF_POLY_PWR  *p_log_tbl);

void        BCH_GF_CalcMinPolys       (CPU_INT08U        m,         /* Calculate minimal polynomials for GF(2^m).       */
                                       BCH_GF_POLY      *p_gf,
                                       BCH_GF_POLY      *p_min_tbl);

CPU_INT16U  BCH_GF_CalcBCH_GenPoly    (CPU_INT08U        m,         /* Calculate BCH generator polynomial.              */
                                       CPU_INT08U        t,
                                       BCH_GF_POLY      *p_gf,
                                       BCH_GF_POLY      *p_min_tbl,
                                       CPU_INT08U       *p_bch_gen);

void        BCH_GF_CalcBCH_GenTblEntry(CPU_INT08U       *p_bch_gen, /* Calculate BCH generator table.                   */
                                       CPU_INT16U        gen_poly_deg,
                                       CPU_INT08U       *p_gen_tbl_entry,
                                       CPU_INT08U        tbl_ix);

void        BCH_GF_CalcAuxTbl         (CPU_INT08U        m,         /* Calculate auxiliary table.                       */
                                       BCH_GF_POLY       poly,
                                       BCH_GF_POLY      *p_aux_tbl);


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

#endif                                                          /* End of BCH module include.                           */
