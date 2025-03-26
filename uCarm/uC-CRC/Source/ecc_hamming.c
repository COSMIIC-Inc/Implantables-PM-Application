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
* Filename      : ecc_hamming.c
* Version       : V1.07.00
* Programmer(s) : FBJ
*                 BAN
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    ECC_HAMMING_MODULE
#include  <ecc_hamming.h>


/*
*********************************************************************************************************
*                                           LINT INHIBITION
*
* Note(s) : (1) Certain macro's within this file describe commands, command values or register fields
*               that are currently unused.  lint error option #750 is disabled to prevent error messages
*               because of unused macro's :
*
*                   "Info 750: local macro '...' (line ..., file ...) not referenced"
*********************************************************************************************************
*/

/*lint -e750*/

/*$PAGE*/
/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  HAMMING_ADDR_MASK_BIT                        0xF800u
#define  HAMMING_ADDR_MASK_WORD_0064                  0x000Fu
#define  HAMMING_ADDR_MASK_WORD_0128                  0x001Fu
#define  HAMMING_ADDR_MASK_WORD_0256                  0x003Fu
#define  HAMMING_ADDR_MASK_WORD_0512                  0x007Fu
#define  HAMMING_ADDR_MASK_WORD_1024                  0x00FFu
#define  HAMMING_ADDR_MASK_WORD_2048                  0x01FFu
#define  HAMMING_ADDR_MASK_WORD_4096                  0x03FFu
#define  HAMMING_ADDR_MASK_WORD_8192                  0x07FFu

#define  HAMMING_CHK_CORRECTABLE_0256       (HAMMING_ADDR_MASK_BIT       | HAMMING_ADDR_MASK_WORD_0256)

#define  HAMMING_ADDR_MASK_BIT_SHORT                    0xF8u
#define  HAMMING_ADDR_MASK_WORD_SHORT_04                0x00u
#define  HAMMING_ADDR_MASK_WORD_SHORT_08                0x01u
#define  HAMMING_ADDR_MASK_WORD_SHORT_16                0x03u
#define  HAMMING_ADDR_MASK_WORD_SHORT_32                0x07u

#define  HAMMING_CHK_CORRECTABLE_SHORT_0004 (HAMMING_ADDR_MASK_BIT_SHORT | HAMMING_ADDR_MASK_WORD_SHORT_04)

#define  HAMMING_ECC_LEN                                   4u
#define  HAMMING_ECC_LEN_SHORT                             2u
#define  HAMMING_256_HARD_CODED_BITS_NBR                   2u

#define  HAMMING_MASK_1_COL_ODD                   0xAAAAAAAAu   /* binary: 10101010101010101010101010101010             */
#define  HAMMING_MASK_2_COL_ODD                   0xCCCCCCCCu   /* binary: 11001100110011001100110011001100             */
#define  HAMMING_MASK_4_COL_ODD                   0xF0F0F0F0u   /* binary: 11110000111100001111000011110000             */

#ifndef  CPU_CFG_ENDIAN_TYPE
#define  CPU_CFG_ENDIAN_TYPE                 CPU_ENDIAN_TYPE_LITTLE
#endif

#if     (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_BIG)
#define  HAMMING_MASK_1_LINES_ODD                 0x00FF00FFu   /* binary: 00000000111111110000000011111111             */
#define  HAMMING_MASK_2_LINES_ODD                 0x0000FFFFu   /* binary: 00000000000000001111111111111111             */
#define  HAMMING_MASK_EVEN                        0x55555400u   /* binary: 01010101010101010101010000000000             */
#define  HAMMING_INT_HI_BYTE_MASK                 0xFFFFFF00u   /* mask used to clear higher byte of integer            */
#else
#define  HAMMING_MASK_1_LINES_ODD                 0xFF00FF00u   /* binary: 11111111000000001111111100000000             */
#define  HAMMING_MASK_2_LINES_ODD                 0xFFFF0000u   /* binary: 11111111111111110000000000000000             */
#define  HAMMING_MASK_EVEN                        0x00545555u   /* binary: 00000000010101000101010101010101             */
#define  HAMMING_INT_HI_BYTE_MASK                 0x00FFFFFFu   /* mask used to clear higher byte of integer            */
#endif

/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

const  CPU_INT08U  Hamming_BitCount[256] = {
    0u, 1u, 1u, 2u, 1u, 2u, 2u, 3u, 1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u,          
    1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u, 2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u,
    1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u, 2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u,
    2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u, 3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u,
    1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u, 2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u,
    2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u, 3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u,
    2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u, 3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u,
    3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u, 4u, 5u, 5u, 6u, 5u, 6u, 6u, 7u,
    1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u, 2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u,
    2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u, 3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u,
    2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u, 3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u,
    3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u, 4u, 5u, 5u, 6u, 5u, 6u, 6u, 7u,
    2u, 3u, 3u, 4u, 3u, 4u, 4u, 5u, 3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u,
    3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u, 4u, 5u, 5u, 6u, 5u, 6u, 6u, 7u,
    3u, 4u, 4u, 5u, 4u, 5u, 5u, 6u, 4u, 5u, 5u, 6u, 5u, 6u, 6u, 7u,
    4u, 5u, 5u, 6u, 5u, 6u, 6u, 7u, 5u, 6u, 6u, 7u, 6u, 7u, 7u, 8u
};

const  ECC_MODULE  Hamming_ECC = {
    128u,
    8192u,
    HAMMING_ECC_LEN,
    Hamming_Calc,
    Hamming_Chk,
    Hamming_Correct
};

const  ECC_MODULE  Hamming_ECC_256 = {
    256u,
    256u,
    HAMMING_ECC_LEN,
    Hamming_Calc_256,
    Hamming_Chk_256,
    Hamming_Correct_256
};

/*$PAGE*/
/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
                                                                /* Eval failed Hamming code chk.                        */
static  void         Hamming_Eval               (CPU_INT32U   hamming,
                                                 CPU_INT32U   hamming_chk,
                                                 CPU_SIZE_T   len,
                                                 CPU_SIZE_T  *p_addr_octet,
                                                 CPU_INT08U  *p_addr_bit,
                                                 CPU_ERR     *p_err);

                                                                /* Calc err loc.                                        */
static  void         Hamming_CalcErrLoc         (CPU_INT16U   addr_val,
                                                 CPU_SIZE_T   len_word,
                                                 CPU_SIZE_T  *p_addr_octet,
                                                 CPU_INT08U  *p_addr_bit);

static  void         Hamming_CalcErrLoc_256     (CPU_INT32U   xor_val,
                                                 CPU_SIZE_T   len_bytes,
                                                 CPU_SIZE_T  *p_addr_octet,
                                                 CPU_INT08U  *p_addr_bit,
                                                 CPU_ERR     *p_err);

                                                                /* Calc par on 32-bit val.                              */
static  CPU_INT32U   Hamming_ParCalc_32         (CPU_INT32U   data_32);

#if (ECC_HAMMING_CFG_OPTIMIZE_ASM_EN == DEF_DISABLED)
                                                                /* Calc bit- & word-wise par 32-bit words.              */
static  void         Hamming_ParCalcBitWord_32  (void        *p_buf,
                                                 CPU_INT32U  *p_par_bit,
                                                 CPU_INT32U  *p_par_word);
#endif

                                                                /* Calc odd  final par vals.                            */
static  CPU_INT32U   Hamming_ParCalcFinValOdd_32(CPU_INT32U   datum);

                                                                /* Calc base-2 log.                                     */
static  CPU_SIZE_T   Hamming_Log_2              (CPU_SIZE_T   val);

static  CPU_BOOLEAN  Hamming_ChkOneBitSet       (CPU_INT16U   chk_val_even,
                                                 CPU_INT16U   chk_val_odd);

static  CPU_INT08U   Hamming_BitCount_32        (CPU_INT32U   number);

/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/
/*$PAGE*/
/*
*********************************************************************************************************
*                                         Hamming_Calc_256()
*
* Description : Calculate Hamming code for 256-byte data buffer.
*
* Argument(s) : p_buf       Pointer to buffer that contains the data.
*
*               len         Length of buffer, in octets; MUST be 256.
*
*               p_ecc       Pointer to (3 bytes) buffer that will receive Hamming code (see Note #1a).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               ECC_ERR_NONE                    Hamming code calculated.
*                               ECC_ERR_INVALID_LEN             Argument 'len' passed an invalid length.
*                               ECC_ERR_NULL_PTR                Argument 'p_buf' passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) (a) The return parameter 'p_ecc' must point to a valid 3-byte buffer; this buffer
*                       need not be 'CPU_INT32U'-aligned.
*
*                   (b) The calculation is optimized for 'CPU_INT32U'-aligned buffers, though any buffer
*                       alignment is acceptable.
*
*                   (c) The 22-bit Hamming code is stored in the 24-bit buffer.
*                       Data buffers larger than 256 bytes should be divided into 256-byte segments, with
*                       the ECCs calculated for the individual segments concatenated to form the ECC for
*                       the entire buffer.
*
*               (2) Modifications are made to the algorithm to make it faster.
*
*                   (a) The calculation is made on 32 bits data.
*
*                   (b) Loop unrolling is used.
*
*               (3) A Hamming code is an error-correcting code that can correct single-bit errors &
*                   detect multiple-bit errors.  This implementation detects double-bit errors.
*
*                   (a) In a Hamming code calculation, parities are calculated for overlapping bit
*                       groups in a data buffer.
*
*                   (b) Intermediate parities are calculated for each word (the row or word parities) &
*                       for each bit position in all words (the column or bit parities).
*
*                   (c) The intermediate parity values are used to calculate the final parity bits.
*
*                       (1) Eight odd parity bits are calculated from the line/byte parities :
*
*                           (a) L001o is the par bit of lines   1,   3, ...       253,     255   (odd                lines)
*                           (b) L002o is the par bit of lines 2-3, 6-7, ...   250-251, 254-255   (odd  groups of   2 lines)
*                            .
*                            .
*                           (h) L128o is the par bit of lines 128-255                            (odd  groups of 128 lines)
*
*                       (2) Eight even parity bits are calculated from the line/byte parities :
*
*                           (a) L001e is the par bit of lines   0,   2, ...       252,     254   (even               lines)
*                           (b) L002e is the par bit of lines 0-1, 4-5, ...   248-249, 252-253   (even groups of   2 lines)
*                            .
*                            .
*                           (h) L128e is the par bit of lines   0-127                            (even groups of 128 lines)
*
*                       (3) Three odd parity bits are calculated from the column/bit parities :
*
*                           (a) C1o  is the par bit of columns   1,   3,   5,   7                (odd              columns)
*                           (b) C2o  is the par bit of columns 2-3, 6-7                          (odd  groups of 2 columns)
*                           (c) C4o  is the par bit of columns 4-7                               (odd  groups of 4 columns)
*
*                       (4) Three even parity bits are calculated from the column/bit parities :
*
*                           (a) C1e  is the par bit of columns   2,   4,   6,   8                (even             columns)
*                           (b) C2e  is the par bit of columns 0-1, 4-5                          (even groups of 2 columns)
*                           (c) C4e  is the par bit of columns 0-3                               (even groups of 4 columns)
*
*                   (d) 3 8-bit values are formed from these parity bits :
*
*                           hamming[0]  BITS  7- 0:    L008o  L008e  L004o  L004e  L002o  L002e  L001o  L001e
*                           hamming[1]  BITS 15- 8:    L128o  L128e  L064o  L064e  L032o  L032e  L016o  L016e
*                           hamming[2]  BITS 23-16:    C4o    C4e    C2o    C2e    C1o    C1e    1      1
**$PAGE*
*  +----------------------------------------------------------------------------------------------------------------------+
*  |                                                                                                                      |
*  |                                                                 LINE / BYTE       CONTRIBUTING TO THE FOLLOWING      |
*  |                                 BIT ADDRESS                      PARITIES    |--------- LINE PARITY BITS ----------| |
*  |                     7    6    5    4    3    2    1    0                                                             |
*  |                 0  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002e  L004e  L008e ...  L128e  |
*  |                 1  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002e  L004e  L008e ...  L128e  |
*  |                 2  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002o  L004e  L008e ...  L128e  |
*  |                 3  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002o  L004e  L008e ...  L128e  |
*  |      B          4  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002e  L004o  L008e ...  L128e  |
*  |      Y          5  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002e  L004o  L008e ...  L128e  |
*  |      T          6  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002o  L004o  L008e ...  L128e  |
*  |      E          7  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002o  L004o  L008e ...  L128e  |
*  |                 .                                                                                                    |
*  |      A          .                                                                                                    |
*  |      D          .                                                                                                    |
*  |      D        248  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002e  L004e  L008o ...  L128o  |
*  |      R        249  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002e  L004e  L008o ...  L128o  |
*  |      E        250  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002o  L004e  L008o ...  L128o  |
*  |      S        251  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002o  L004e  L008o ...  L128o  |
*  |      S        252  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002e  L004o  L008o ...  L128o  |
*  |               253  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002e  L004o  L008o ...  L128o  |
*  |               254  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001e  L002o  L004o  L008o ...  L128o  |
*  |               255  ---  ---  ---  ---  ---  ---  ---  ---    ==>    0/1       L001o  L002o  L004o  L008o ...  L128o  |
*  |                                                                                                                      |
*  |                     |    |    |    |    |    |    |    |                                                             |
*  |                     V    V    V    V    V    V    V    V                                                             |
*  | COLUMN / BIT                                                                                                         |
*  |  PARITIES          0/1  0/1  0/1  0/1  0/1  0/1  0/1  0/1                                                            |
*  |                                                                                                                      |
*  | CONTRIBUTING TO                                                                                                      |
*  |  THE FOLLOWING                                                                                                       |
*  |   -----                                                                                                              |
*  |     |              C1o  C1e  C1o  C1e  C1o  C1e  C1o  C1e                                                            |
*  |   COLUMN                                                                                                             |
*  |   PARITY           C2o  C2o  C2e  C2e  C2o  C2o  C2e  C2e                                                            |
*  |    BITS                                                                                                              |
*  |     |              C4o  C4o  C4o  C4o  C4e  C4e  C4e  C4e                                                            |
*  |   -----                                                                                                              |
*  +----------------------------------------------------------------------------------------------------------------------+
*
*                   (e) Only odd parity bits are calculated in the loop to improve algorith speed. 
*                       Even parity bits are calculated from odd parity bits and from the parity of the whole data block.
*
*********************************************************************************************************
*/
/*$PAGE*/
void  Hamming_Calc_256 (void        *p_buf,
                        CPU_SIZE_T   len,
                        CPU_INT08U  *p_ecc,
                        CPU_ERR     *p_err)
{
    CPU_ADDR     align_modulo;
    CPU_INT08U  *p_buf_08;
    CPU_INT32U   hamming;
    CPU_INT32U   L001o;
    CPU_INT32U   L002o;
    CPU_INT32U   L004o;
    CPU_INT32U   L008o;
    CPU_INT32U   L016o;
    CPU_INT32U   L032o;
    CPU_INT32U   L064o;
    CPU_INT32U   L128o;
    CPU_INT32U   C1o;
    CPU_INT32U   C2o;
    CPU_INT32U   C4o;
    CPU_INT32U   whole_data_par;
    CPU_INT32U   col_par;
    CPU_INT32U   tot_par;
    CPU_INT32U   big_blk_par_ix;
    CPU_INT32U   big_blk_par;
    CPU_INT32U  *p_line_data;
    CPU_INT32U   line_data;
    CPU_INT32U   ix;
    CPU_INT32U   even_hamming_shifted;
    CPU_INT32U   even_hamming;
    CPU_INT32U   even_hamming_par;



#if (ECC_HAMMING_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)             /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (CPU_ERR *)0) {                                /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_ecc == (CPU_INT08U *)0) {                             /* Validate ECC ptr (see Note #1a).                     */
       *p_err = ECC_ERR_NULL_PTR;
        return;
    }
    if (p_buf == (void *)0) {                                   /* Validate buf ptr.                                    */
        Mem_Clr((void *)p_ecc, HAMMING_ECC_LEN);
       *p_err = ECC_ERR_NULL_PTR;
        return;
    }
    if (len != 256u) {                                          /* Validate len.                                        */
        Mem_Clr((void *)p_ecc, HAMMING_ECC_LEN);
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
#endif

    align_modulo     = (CPU_ADDR)p_buf % sizeof(CPU_INT32U);

                                                                /* ----------------- INIT VARIABLES ------------------- */
    L004o            =  0u;
    L008o            =  0u;
    L016o            =  0u;
    hamming          =  0u;
    tot_par          =  0u;
    big_blk_par      =  0u;


/*$PAGE*/
    if (align_modulo == 0u) {                                   /* ---------- ACCESS MEM ON 4-BYTE BOUNDARIES --------- */
        p_line_data  = (CPU_INT32U *)p_buf;
                                                                /* -------------------- CALC PARITY ------------------- */
                                                                /* for each odd byte of data / each odd bit position    */
                                                                /* See note #3e                                         */
        for (ix = 0u; ix < 8u; ix++) {
            col_par         = *p_line_data;

            p_line_data++;

            L004o          ^= *p_line_data;
            col_par        ^= *p_line_data;

            p_line_data++;

            L008o          ^= *p_line_data;
            col_par        ^= *p_line_data;

            p_line_data++;

            L004o          ^= *p_line_data;
            L008o          ^= *p_line_data;
            col_par        ^= *p_line_data;

            p_line_data++;

            L016o          ^= *p_line_data;
            col_par        ^= *p_line_data;

            p_line_data++;

            L004o          ^= *p_line_data;
            L016o          ^= *p_line_data;
            col_par        ^= *p_line_data;

            p_line_data++;

            L008o          ^= *p_line_data;
            L016o          ^= *p_line_data;
            col_par        ^= *p_line_data;

            p_line_data++;

            L004o          ^= *p_line_data;
            L008o          ^= *p_line_data;
            L016o          ^= *p_line_data;
            col_par        ^= *p_line_data;

            p_line_data++;


            tot_par        ^= col_par;

            col_par         = Hamming_ParCalc_32(col_par);
            big_blk_par_ix  = ix * col_par;
            big_blk_par    ^= big_blk_par_ix;
        }
/*$PAGE*/
    } else {                                                    /* ---------- ACCESS MEM ON 1-BYTE BOUNDARIES --------- */
        p_buf_08  = (CPU_INT08U *)p_buf;

        line_data =  MEM_VAL_GET_INT32U((void *)p_buf_08);
                                                                /* -------------------- CALC PARITY ------------------- */
                                                                /* for each odd byte of data / each odd bit position    */
        for (ix = 0u; ix < 8u; ix++) {
            col_par         = line_data;

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);

            L004o          ^= line_data;
            col_par        ^= line_data;

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);

            L008o          ^= line_data;
            col_par        ^= line_data;

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);

            L004o          ^= line_data;
            L008o          ^= line_data;
            col_par        ^= line_data;

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);

            L016o          ^= line_data;
            col_par        ^= MEM_VAL_GET_INT32U((void *)p_buf_08);

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);

            L004o          ^= line_data;
            L016o          ^= line_data;
            col_par        ^= line_data;

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);

            L008o          ^= line_data;
            L016o          ^= line_data;
            col_par        ^= line_data;

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);

            L004o          ^= line_data;
            L008o          ^= line_data;
            L016o          ^= line_data;
            col_par        ^= line_data;

            p_buf_08       += sizeof(CPU_INT32U);
            line_data       = MEM_VAL_GET_INT32U((void *)p_buf_08);


            tot_par        ^= col_par;

            col_par         = Hamming_ParCalc_32(col_par);
            big_blk_par_ix  = ix * col_par;
            big_blk_par    ^= big_blk_par_ix;
        }

    }
/*$PAGE*/


                                                                /* ------------ CALC ODD LINES PARITY BITS ------------ */
    L128o = (big_blk_par & DEF_BIT_02) >> 2u;
    L064o = (big_blk_par & DEF_BIT_01) >> 1u;
    L032o = (big_blk_par & DEF_BIT_00) >> 0u;
    L016o =  Hamming_ParCalc_32(L016o);
    L008o =  Hamming_ParCalc_32(L008o);
    L004o =  Hamming_ParCalc_32(L004o);
    L002o =  Hamming_ParCalc_32(tot_par & HAMMING_MASK_2_LINES_ODD);
    L001o =  Hamming_ParCalc_32(tot_par & HAMMING_MASK_1_LINES_ODD);


                                                                /* ----------- CALC ODD COLUMNS PARITY BITS ----------- */
    C4o   =  Hamming_ParCalc_32(tot_par & HAMMING_MASK_4_COL_ODD);
    C2o   =  Hamming_ParCalc_32(tot_par & HAMMING_MASK_2_COL_ODD);
    C1o   =  Hamming_ParCalc_32(tot_par & HAMMING_MASK_1_COL_ODD);


                                                                /* --------- SET ODD PAR BITS IN HAMMING CODE --------- */
                                                                /* ------- SET ODD LINES PAR BITS IN FIRST BYTE ------- */
    if (L001o) {
        DEF_BIT_SET(hamming, DEF_BIT_01);
    }
    if (L002o) {
        DEF_BIT_SET(hamming, DEF_BIT_03);
    }
    if (L004o) {
        DEF_BIT_SET(hamming, DEF_BIT_05);
    }
    if (L008o) {
        DEF_BIT_SET(hamming, DEF_BIT_07);
    }


                                                                /* ------- SET ODD LINES PAR BITS IN SECOND BYTE ------ */
    if (L016o) {
        DEF_BIT_SET(hamming, DEF_BIT_09);
    }
    if (L032o) {
        DEF_BIT_SET(hamming, DEF_BIT_11);
    }
    if (L064o) {
        DEF_BIT_SET(hamming, DEF_BIT_13);
    }
    if (L128o) {
        DEF_BIT_SET(hamming, DEF_BIT_15);
    }


                                                                /* ------ SET ODD COLUMNS PAR BITS IN THIRD BYTE ------ */
    if (C1o) {
        DEF_BIT_SET(hamming, DEF_BIT_19);
    }
    if (C2o) {
        DEF_BIT_SET(hamming, DEF_BIT_21);
    }
    if (C4o) {
        DEF_BIT_SET(hamming, DEF_BIT_23);
    }


                                                                /* ---------------- SET EVEN PAR BITS ----------------- */
                                                                /* See Note #3e                                         */
    even_hamming          = (hamming & HAMMING_INT_HI_BYTE_MASK);
    even_hamming_shifted  =  even_hamming >> 1u;
                                                                /* If parity of whole data is odd ...                   */
    whole_data_par        =  Hamming_ParCalc_32(tot_par);
    even_hamming_par      =  HAMMING_MASK_EVEN * whole_data_par;
                                                                /* Invert even bits                                     */
    even_hamming_shifted ^=  even_hamming_par;
                                                                /* Or even and odd bits to obtain full hamming code     */
    hamming              |=  even_hamming_shifted;

                                                                /* Bit 0 and bit 1 are set by default                   */
    DEF_BIT_SET(hamming, DEF_BIT_16);
    DEF_BIT_SET(hamming, DEF_BIT_17);


                                                                /* ----------------- SET HAMMING CODE ----------------- */   
    p_ecc[0u] = (CPU_INT08U) hamming;
    p_ecc[1u] = (CPU_INT08U)(hamming >>  8u);
    p_ecc[2u] = (CPU_INT08U)(hamming >> 16u);

   *p_err = ECC_ERR_NONE;
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          Hamming_Chk_256()
*
* Description : Check previously computed Hamming code against current data for 256-byte buffer.
*
* Argument(s) : p_buf           Pointer to buffer that contains the data.
*
*               len             Length of buffer, in octets; MUST be 256.
*
*               p_ecc           Pointer to (3 byte) buffer that contains the Hamming code.
*
*               err_addr_tbl    Table that will receive the addresses of any errors.
*
*               max_errs        Size of 'err_addr_tbl'; the maximum number of error locations that can
*                               be returned.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   ECC_ERR_NONE                Hamming code verified.
*                                   ECC_ERR_CORRECTABLE         Correctable error detected in data.
*                                   ECC_ERR_INVALID_LEN         Argument 'len' passed an invalid length.
*                                   ECC_ERR_NULL_PTR            Argument 'p_buf', 'p_ecc' or
*                                                                   'err_addr_tbl' passed a NULL pointer.
*                                   ECC_ERR_UNCORRECTABLE       Uncorrectable error detected.
*
* Return(s)   : The number of bit errors in the data.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) If a correctable error is found, 'err_addr_tbl[0].AddrBit' & 'err_addr_tbl[0].AddrOctet'
*                   will specify the location of the bit error.  To correct it, the caller should complement
*                   that bit :
*
*                       Hamming_Chk_256(&buf[0],
*                                       &hamming_prev,
*                                       &err_addr,
*                                        1,
*                                       &err);
*
*                       if (err == ECC_ERR_CORRECTABLE) {
*                           buf[err_addr.AddrOctet] ^= DEF_BIT(err_addr.AddrBit);
*                       }
*
*               (2) (a) The maximum number of errors that can be corrected is 1, so 'max_errs' should be 1.
*
*                   (b) An uncorrectable error is one in which two or more bits of the data have changed
*                       & the error is detectable.
*
*                   (c) If more than two bits of the data have changed, the error may be misdiagnosed as
*                       a single-bit error.
*
*               (3) If a single-bit error occurs in the data, either the odd OR even parity bit for
*                   each line & column parity (but NOT both) will be flipped in the check ECC.  Since
*                   there are 22 parity bits, 11 of them will be flipped. 
*********************************************************************************************************
*/
/*$PAGE*/
CPU_INT08U  Hamming_Chk_256 (void          *p_buf,
                             CPU_SIZE_T     len,
                             CPU_INT08U    *p_ecc,
                             ECC_ERR_ADDR   err_addr_tbl[],
                             CPU_INT08U     max_errs,
                             CPU_ERR       *p_err)
{
    CPU_INT08U  hamming_chk[3];
    CPU_INT32U  computed_hamming;
    CPU_INT32U  stored_hamming;
    CPU_INT32U  hamming_xor;
    CPU_INT08U  diff_cnt;


#if (ECC_HAMMING_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)             /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (CPU_ERR *)0) {                                /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (max_errs > 0u) {                                        /* Validate err cnt.                                    */
        if (err_addr_tbl == (ECC_ERR_ADDR *)0) {
           *p_err = ECC_ERR_NULL_PTR;
            return (0u);
        }
        err_addr_tbl[0].AddrBit   = 0u;                         /* Dflt addr.                                           */
        err_addr_tbl[0].AddrOctet = 0u;
    }
    if (p_buf == (void *)0) {                                   /* Validate buf ptr.                                    */
       *p_err  =  ECC_ERR_NULL_PTR;
        return (0u);
    }
    if (p_ecc == (CPU_INT08U *)0) {                             /* Validate ECC ptr.                                    */
       *p_err  =  ECC_ERR_NULL_PTR;
        return (0u);
    }
    if (len   != 256u) {                                        /* Validate len.                                        */
       *p_err  = ECC_ERR_INVALID_LEN;
        return (0u);
    }
#endif


                                                                /* ----------------- CALC HAMMING CODE ---------------- */
    Hamming_Calc_256( p_buf,                                    /* Calc Hamming code for cur buf.                       */
                      len,
                     &hamming_chk[0u],
                      p_err);

    computed_hamming  = (CPU_INT32U)hamming_chk[0u];
    computed_hamming |= (CPU_INT32U)hamming_chk[1u] << 8u;
    computed_hamming |= (CPU_INT32U)hamming_chk[2u] << 16u;

    stored_hamming    = (CPU_INT32U)p_ecc[0u];
    stored_hamming   |= (CPU_INT32U)p_ecc[1u]       << 8u;
    stored_hamming   |= (CPU_INT32U)p_ecc[2u]       << 16u;

    hamming_xor       = stored_hamming ^ computed_hamming;

                                                                /* Masking the 4th byte of int (code is on 3 bytes)     */
    hamming_xor      &= (CPU_INT32U)HAMMING_INT_HI_BYTE_MASK;

    diff_cnt          = Hamming_BitCount_32(hamming_xor);

    if (diff_cnt == 0u) {
       *p_err = ECC_ERR_NONE;
        return (0u);
    }

    if (diff_cnt == 11u) {                                      /* See Note #3.                                         */
        Hamming_CalcErrLoc_256( hamming_xor,                    /* See Note #1.                                         */
                                256u,
                               &err_addr_tbl[0].AddrOctet,
                               &err_addr_tbl[0].AddrBit,
                                p_err);
        if (*p_err != ECC_ERR_NONE) {
            return (0u);
        } else {
           *p_err = ECC_ERR_CORRECTABLE;
            return (1u);
        }
    }

   *p_err = ECC_ERR_UNCORRECTABLE;
    return (0u);

}

/*$PAGE*/
/*
*********************************************************************************************************
*                                        Hamming_Correct_256()
*
* Description : Check previously computed Hamming code against current data & correct any error(s), if
*               possible, for 256-byte buffer.
*
* Argument(s) : p_buf       Pointer to buffer that contains the data.
*
*               len         Length of buffer, in octets; MUST be 256.
*
*               p_ecc       Pointer to (4 byte) buffer that contains the Hamming code.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               ECC_ERR_NONE                    Hamming code verified.
*                               ECC_ERR_CORRECTABLE             Correctable error detected in data.
*                               ECC_ERR_INVALID_LEN             Argument 'len' passed an invalid length.
*                               ECC_ERR_NULL_PTR                Argument 'p_buf' or 'p_ecc' or passed a NULL
*                                                                   pointer.
*                               ECC_ERR_UNCORRECTABLE           Uncorrectable error detected.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) See 'Hamming_Chk_256()  Note #2'.
*********************************************************************************************************
*/

void  Hamming_Correct_256 (void       *p_buf,
                           CPU_SIZE_T  len,
                           CPU_INT08U *p_ecc,
                           CPU_ERR    *p_err)
{
    CPU_INT08U    *p_buf_08;
    ECC_ERR_ADDR   err_addr;


#if (ECC_HAMMING_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)             /* ------------------- VALIDATE ARGS ------------------ */
    if (p_ecc == (CPU_INT08U *)0) {                             /* Validate ECC ptr.                                    */
       *p_err  =  ECC_ERR_NULL_PTR;
        return;
    }
    if (p_buf == (void *)0) {                                   /* Validate buf ptr.                                    */
       *p_err  =  ECC_ERR_NULL_PTR;
        return;
    }
    if (len   !=  256u) {                                       /* Validate len.                                        */
       *p_err  =  ECC_ERR_INVALID_LEN;
        return;
    }
#endif


                                                                /* ----------------- CHK HAMMING CODE ----------------- */
    (void)Hamming_Chk_256( p_buf,                               /* Calc Hamming code for cur buf.                       */
                           len,
                           p_ecc,
                          &err_addr,
                           1u,
                           p_err);

    if (*p_err != ECC_ERR_CORRECTABLE) {
        return;
    }

                                                                /* Correct err.                                         */
    p_buf_08                      = (CPU_INT08U *)p_buf;
    p_buf_08[err_addr.AddrOctet] ^= (CPU_INT08U  )DEF_BIT(err_addr.AddrBit);
   *p_err                         =               ECC_ERR_NONE;
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                        Hamming_CalcErrLoc_256()
*
* Description : Calculate error location.
*
* Argument(s) : xor_val         Value of the xor between stored and computed hamming code.
*
*               len_bytes       Length of data, in bytes.
*
*               p_addr_octet    Pointer to variable that will receive the octet index in which a bit must
*                               be complemented to correct the data.
*
*               p_addr_bit      Pointer to variable that will receive the bit number that should be
*                               complemented in the octet to correct the data.
*
* Return(s)   : none.
*
* Caller(s)   : Hamming_Chk_256(),
*               Hamming_Eval().
*
* Note(s)     : (1) len_bytes must be 256.
*
*               (2) See 'Hamming_Calc_256()  Note #3d and corresponding table'.
*********************************************************************************************************
*/

static  void  Hamming_CalcErrLoc_256 (CPU_INT32U   xor_val,
                                      CPU_SIZE_T   len_bytes,
                                      CPU_SIZE_T  *p_addr_octet,
                                      CPU_INT08U  *p_addr_bit,
                                      CPU_ERR     *p_err)
{
    CPU_INT08U  addr_bit;
    CPU_SIZE_T  addr_octet;


#if (ECC_HAMMING_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)             /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (CPU_ERR *)0) {                                /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (len_bytes != 256u) {                                    /* Validate length                                      */
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
#endif
    
    xor_val      =  xor_val >>  1u;
    addr_octet   = (xor_val &   DEF_BIT_00);
    xor_val      =  xor_val >>  1u;
    addr_octet  += (xor_val &   DEF_BIT_01);
    xor_val      =  xor_val >>  1u;
    addr_octet  += (xor_val &   DEF_BIT_02);
    xor_val      =  xor_val >>  1u;
    addr_octet  += (xor_val &   DEF_BIT_03);
    xor_val      =  xor_val >>  1u;
    addr_octet  += (xor_val &   DEF_BIT_04);
    xor_val      =  xor_val >>  1u;
    addr_octet  += (xor_val &   DEF_BIT_05);
    xor_val      =  xor_val >>  1u;
    addr_octet  += (xor_val &   DEF_BIT_06);
    xor_val      =  xor_val >>  1u;
    addr_octet  += (xor_val &   DEF_BIT_07);

    xor_val      =  xor_val >> (DEF_OCTET_NBR_BITS + HAMMING_256_HARD_CODED_BITS_NBR + 1u);
    addr_bit     = (xor_val &   DEF_BIT_00);
    xor_val      =  xor_val >>  1u;
    addr_bit    += (xor_val &   DEF_BIT_01);
    xor_val      =  xor_val >>  1u;
    addr_bit    += (xor_val &   DEF_BIT_02);

   *p_addr_bit   = addr_bit;
   *p_addr_octet = addr_octet;
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                           Hamming_Calc()
*
* Description : Calculate Hamming code for data buffer.
*
* Argument(s) : p_buf       Pointer to buffer that contains the data.
*
*               len         Length of buffer, in octets (see Note #2).
*
*               p_ecc       Pointer to (4 byte) buffer that will receive Hamming code (see Note #1a).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               ECC_ERR_NONE           Hamming code calculated.
*                               ECC_ERR_INVALID_LEN    Argument 'len' passed an invalid length.
*                               ECC_ERR_NULL_PTR       Argument 'p_buf' passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) (a) The return parameter 'p_ecc' must point to a valid 4-byte buffer; this buffer
*                       need not be 'CPU_INT32U'-aligned.
*
*                   (b) The calculation is optimized for 'CPU_INT32U'-aligned buffers, though any buffer
*                       alignment is acceptable.
*
*               (2) Certain buffer lengths only are supported :
*
*                   (a) Buffer lengths greater than or equal to 128 that are ALSO multiples of 128 :
*                       128, 256, 384, 512 ....
*                          n
*                   (b) A 2 -bit data buffer requires A 2n-bit ECC.  Consequently, the maximum data
*                       length that can be accommodated with a 32-bit ECC is
*
*                             32/2             16           13
*                           2       bits =  2    bits  =  2   byte  =  8192 byte
*
*               (2) The 20 to 32-bit Hamming code is stored in the 32-bit buffer.
*
*               (3) See 'Hamming_Calc_256()  Note #3'.
*
*                   (a) The row/word parities are calculated from W0001/W0001' through W1024/W1024'.
*
*                   (b) A single 32-bit value is formed from these parity bits :
*
*                           BITS 31-16:    B16    B08    B04    B02    B01    W1024    W0512    W0256    W0128    W0064    W0032    W0016    W0008    W0004    W0002    W0001
*                           BITS 15- 0:    B16'   B08'   B04'   B02'   B01'   W1024'   W0512'   W0256'   W0128'   W0064'   W0032'   W0016'   W0008'   W0004'   W0002'   W0001'
*********************************************************************************************************
*/
/*$PAGE*/
void  Hamming_Calc (void        *p_buf,
                    CPU_SIZE_T   len,
                    CPU_INT08U  *p_ecc,
                    CPU_ERR     *p_err)
{
    CPU_INT08U  *p_buf_08;
    CPU_SIZE_T   grp_cnt;
    CPU_SIZE_T   grp_cnt_log;
    CPU_SIZE_T   grp_ix;
    CPU_INT32U   grp_ix_field;
    CPU_INT32U   grp_mask;
    CPU_INT32U   hamming;
    CPU_INT32U   hamming_temp;
    CPU_SIZE_T   len_align_modulo;
    CPU_SIZE_T   len_pwr_2;
    CPU_INT32U   par_bit;
    CPU_INT32U   par_bit_temp;
    CPU_INT32U   par_even;
    CPU_INT32U   par_tot;
    CPU_INT32U   par_word;
    CPU_INT32U   par_word_par;


#if (ECC_HAMMING_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)             /* ------------------- VALIDATE ARGS ------------------ */
    if (p_ecc == (CPU_INT08U *)0) {                             /* Validate ECC ptr (see Note #1a).                     */
       *p_err = ECC_ERR_NULL_PTR;
        return;
    }

    Mem_Clr((void *)p_ecc, HAMMING_ECC_LEN);
    if (p_buf == (void *)0) {                                   /* Validate buf ptr.                                    */
       *p_err = ECC_ERR_NULL_PTR;
        return;
    }
    if (len < 128u) {                                           /* Validate len (see Note #2).                          */
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
    if (len > 8192u) {
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
    len_align_modulo = len % 128u;
    if (len_align_modulo != 0u) {
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
#endif

                                                                /* -------------- CALC ODD FINAL PAR BITS ------------- */
    p_buf_08    = (CPU_INT08U *)p_buf;
    grp_cnt     = len / 128u;

    grp_cnt_log = Hamming_Log_2(grp_cnt);
    grp_mask    = DEF_BIT_FIELD(grp_cnt_log, 0);
    par_bit     = 0u;
    hamming     = 0u;

    for (grp_ix = grp_cnt; grp_ix > 0u; grp_ix--) {
        Hamming_ParCalcBitWord_32((void *) p_buf_08,            /* Calc pars for bytes   0 - 127.                       */
                                          &par_bit_temp,
                                          &par_word);

        par_bit      ^= par_bit_temp;

        hamming_temp  = Hamming_ParCalcFinValOdd_32(par_word);  /* Calc W0001,  W0002,  W0004,  W0008  & W0016.         */
        hamming      ^= hamming_temp;

        par_word_par  = Hamming_ParCalc_32(par_word);
        if (par_word_par == 1u) {
            grp_ix_field  = ((CPU_INT32U)(grp_ix - 1u))  & grp_mask;
            hamming      ^=               grp_ix_field  << 5u; /* Calc W0032,  W0064,  W0128,  W0256,  W0512 &  W1024. */
        }

        p_buf_08     += 128u;
    }

    hamming_temp  = Hamming_ParCalcFinValOdd_32(par_bit);       /* Calc B01,  B02,  B04,  B08  & B16.                   */
    hamming      |= hamming_temp << 11;


/*$PAGE*/
                                                                /* ------------- CALC EVEN FINAL PAR BITS ------------- */
    par_tot = Hamming_ParCalc_32(par_bit);                      /* Calc tot par                                ...      */
    if (par_tot  == 1u) {                                       /* ... if tot par odd, even par cpl of odd par ...      */
        par_even = (hamming ^ 0xFFFFu);
    } else {                                                    /* ... else            even par equal  odd par.         */
        par_even =  hamming;
    }

    len_pwr_2 =  (len > 4096u) ? 8192u :
                ((len > 2048u) ? 4096u :
                ((len > 1024u) ? 2048u :
                ((len >  512u) ? 1024u :
                ((len >  256u) ?  512u :
                ((len >  128u) ?  256u : 128u)))));

    hamming <<=  16u;
    hamming  |= (par_even & (HAMMING_ADDR_MASK_BIT | (CPU_INT32U)((len_pwr_2 / 4u) - 1u)));



                                                                /* ---------------------- RTN PAR --------------------- */
    MEM_VAL_SET_INT32U((void *)p_ecc, hamming);
   *p_err = ECC_ERR_NONE;
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                            Hamming_Chk()
*
* Description : Check previously computed Hamming code against current data for 256-byte buffer.
*
* Argument(s) : p_buf           Pointer to buffer that contains the data.
*
*               len             Length of buffer, in octets.
*
*               p_ecc           Pointer to (4 byte) buffer that contains the Hamming code.
*
*               err_addr_tbl    Table that will receive the addresses of any errors.
*
*               max_errs        Size of 'err_addr_tbl'; the maximum number of error locations that can
*                               be returned.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   ECC_ERR_NONE               Hamming code verified.
*                                   ECC_ERR_CORRECTABLE        Correctable error detected in data.
*                                   ECC_ERR_ECC_CORRECTABLE    Correctable error detected in ECC.
*                                   ECC_ERR_INVALID_LEN        Argument 'len' passed an invalid length.
*                                   ECC_ERR_NULL_PTR           Argument 'p_buf', 'p_ecc' or
*                                                                  'err_addr_tbl' passed a NULL pointer.
*                                   ECC_ERR_UNCORRECTABLE      Uncorrectable error detected.
*
* Return(s)   : The number of bit errors in the data.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) If a correctable error is found, 'err_addr_tbl[0].AddrBit' & 'err_addr_tbl[0].AddrOctet'
*                   will specify the location of the bit error.  To correct it, the caller should complement
*                   that bit :
*
*                       Hamming_Chk(&buf[0],
*                                    len,
*                                   &hamming_prev,
*                                   &err_addr,
*                                    1,
*                                   &err);
*
*                       if (err == ECC_ERR_CORRECTABLE) {
*                           buf[err_addr.AddrOctet] ^= DEF_BIT(err_addr.AddrBit);
*                       }
*
*               (2) See 'Hamming_Chk_256()  Note #2'.
*
*               (3) Since upper word parity bits may only be 'half' or 'three-quarter' used, an
*                   uncorrectable error may be initially misdiagnosed as a correctable error at a bit
*                   location outside the data range.
*
*               (4) The value passed to the 'len' argument MUST match the value passed to the 'len'
*                   argument of 'Hamming_Calc()'.
*********************************************************************************************************
*/
/*$PAGE*/
CPU_INT08U  Hamming_Chk (void          *p_buf,
                         CPU_SIZE_T     len,
                         CPU_INT08U    *p_ecc,
                         ECC_ERR_ADDR   err_addr_tbl[],
                         CPU_INT08U     max_errs,
                         CPU_ERR       *p_err)
{
    CPU_INT08U  addr_bit;
    CPU_SIZE_T  addr_octet;
    CPU_INT32U  hamming;
    CPU_INT08U  hamming_buf[HAMMING_ECC_LEN];
    CPU_INT32U  hamming_chk;
    CPU_SIZE_T  len_align_modulo;
    CPU_SIZE_T  len_pwr_2;


#if (ECC_HAMMING_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)             /* ------------------- VALIDATE ARGS ------------------ */
    if (max_errs > 0u) {                                        /* Validate err cnt.                                    */
        if (err_addr_tbl == (ECC_ERR_ADDR *)0) {
           *p_err = ECC_ERR_NULL_PTR;
            return (0u);
        }
        err_addr_tbl[0].AddrBit   = 0u;                         /* Dflt addr.                                           */
        err_addr_tbl[0].AddrOctet = 0u;
    }
    if (p_buf == (void *)0) {                                   /* Validate buf ptr.                                    */
       *p_err = ECC_ERR_NULL_PTR;
        return (0u);
    }
    if (p_ecc == (CPU_INT08U *)0) {                             /* Validate ECC ptr.                                    */
       *p_err = ECC_ERR_NULL_PTR;
        return (0u);
    }
    if (len < 128u) {                                           /* Validate len (see Note #2).                          */
       *p_err = ECC_ERR_INVALID_LEN;
        return (0u);
    }
    if (len > 8192u) {
       *p_err = ECC_ERR_INVALID_LEN;
        return (0u);
    }
    len_align_modulo = len % 128u;
    if (len_align_modulo != 0u) {
       *p_err = ECC_ERR_INVALID_LEN;
        return (0u);
    }
#endif



                                                                /* ----------------- CALC HAMMING CODE ---------------- */
    hamming = MEM_VAL_GET_INT32U((void *)p_ecc);

    Hamming_Calc( p_buf,                                        /* Calc Hamming code for cur buf.                       */
                  len,
                 &hamming_buf[0],
                  p_err);

    hamming_chk = MEM_VAL_GET_INT32U((void *)&hamming_buf[0]);

    if (hamming == hamming_chk) {
       *p_err = ECC_ERR_NONE;
        return (0u);
    }


/*$PAGE*/
                                                                /* ------- DETERMINE WHETHER ERR IS CORRECTABLE ------- */
    len_pwr_2 =  (len > 4096u) ? 8192u :
                ((len > 2048u) ? 4096u :
                ((len > 1024u) ? 2048u :
                ((len >  512u) ? 1024u :
                ((len >  256u) ?  512u :
                ((len >  128u) ?  256u : 128u)))));

    Hamming_Eval( hamming,                                      /* Eval Hamming chk.                                    */
                  hamming_chk,
                  len_pwr_2,
                 &addr_octet,
                 &addr_bit,
                  p_err);

    if (*p_err != ECC_ERR_CORRECTABLE) {                        /* See Note #2.                                         */
        return (0u);
    }

    if (len_pwr_2 > len) {
        if (addr_octet < (len_pwr_2 - len)) {                   /* See Note #3.                                         */
           *p_err = ECC_ERR_UNCORRECTABLE;
            return (0u);
        }

        addr_octet = (addr_octet + len) - len_pwr_2;
    }

    if (max_errs > 0u) {                                        /* See Note #2a.                                        */
        err_addr_tbl[0].AddrBit   = addr_bit;
        err_addr_tbl[0].AddrOctet = addr_octet;
    }

    return (1u);                                                /* See Note #1.                                         */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          Hamming_Correct()
*
* Description : Check previously computed Hamming code against current data & correct any error(s), if
*               possible.
*
* Argument(s) : p_buf       Pointer to buffer that contains the data.
*
*               len         Length of buffer, in octets.
*
*               p_ecc       Pointer to (4 byte) buffer that contains the Hamming code.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               ECC_ERR_NONE             Hamming code verified.
*                               ECC_ERR_CORRECTABLE      Correctable   error detected in data.
*                               ECC_ERR_INVALID_LEN      Argument 'len' passed an invalid length.
*                               ECC_ERR_NULL_PTR         Argument 'p_buf' or 'p_ecc' or passed a NULL
*                                                            pointer.
*                               ECC_ERR_UNCORRECTABLE    Uncorrectable error detected.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) See 'Hamming_Chk_256()  Note #2'.
*********************************************************************************************************
*/

void  Hamming_Correct (void        *p_buf,
                       CPU_SIZE_T   len,
                       CPU_INT08U  *p_ecc,
                       CPU_ERR     *p_err)
{
    ECC_ERR_ADDR   err_addr;
    CPU_SIZE_T     len_align_modulo;
    CPU_INT08U    *p_buf_08;


#if (ECC_HAMMING_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)             /* ------------------- VALIDATE ARGS ------------------ */
    if (p_ecc == (CPU_INT08U *)0) {                             /* Validate ECC ptr.                                    */
       *p_err = ECC_ERR_NULL_PTR;
        return;
    }
    if (p_buf == (void *)0) {                                   /* Validate buf ptr.                                    */
       *p_err = ECC_ERR_NULL_PTR;
        return;
    }
    if (len < 128u) {                                           /* Validate len (see Note #1).                          */
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
    if (len > 8192u) {
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
    len_align_modulo = len % 128u;
    if (len_align_modulo != 0u) {
       *p_err = ECC_ERR_INVALID_LEN;
        return;
    }
#endif



                                                                /* ----------------- CHK HAMMING CODE ----------------- */
    (void)Hamming_Chk( p_buf,                                   /* Calc Hamming code for cur buf.                       */
                       len,
                       p_ecc,
                      &err_addr,
                       1u,
                       p_err);

    if (*p_err != ECC_ERR_CORRECTABLE) {
        return;
    }

                                                                /* Correct err.                                         */
    p_buf_08                      = (CPU_INT08U *)p_buf;
    p_buf_08[err_addr.AddrOctet] ^= (CPU_INT08U  )DEF_BIT(err_addr.AddrBit);
}

/*$PAGE*/
/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           Hamming_Eval()
*
* Description : Evaluate result of failed Hamming code check.
*
* Argument(s) : hamming         Hamming code of original data.
*
*               hamming_chk     Hamming code of corrupted data.
*
*               len             Length of data, in octets.
*
*               p_addr_octet    Pointer to variable that will receive the octet index in which a bit must
*                               be complemented to correct the data.
*               ------------    Argument validated by caller.
*
*               p_addr_bit      Pointer to variable that will receive the bit number that should be
*                               complemented in the octet to correct the data.
*               ------------    Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   ECC_ERR_CORRECTABLE        Correctable error detected in data.
*                                   ECC_ERR_ECC_CORRECTABLE    Correctable error detected in ECC.
*                                   ECC_ERR_UNCORRECTABLE      Uncorrectable error detected.
*
* Return(s)   : none.
*
* Caller(s)   : Hamming_Chk().
*
* Note(s)     : (1) (a) If a single-bit error occurs in the data, either the odd OR even parity bit for
*                       each word & byte parity (but NOT both) will be flipped in the check ECC.  When
*                       the four parity words (the odd & even parity words for the original data AND the
*                       odd & even parity words for the current data) are XOR'd together, all the parity
*                       bits will be set.
*
*                   (b) If a single-bit error occurs in the ECC, there will be a 1-bit difference between
*                       the original ECC & the new ECC.
*********************************************************************************************************
*/
/*$PAGE*/
static  void  Hamming_Eval (CPU_INT32U   hamming,
                            CPU_INT32U   hamming_chk,
                            CPU_SIZE_T   len,
                            CPU_SIZE_T  *p_addr_octet,
                            CPU_INT08U  *p_addr_bit,
                            CPU_ERR     *p_err)
{
    CPU_INT16U   chk_mask;
    CPU_INT16U   chk_val;
    CPU_INT16U   chk_val_even;
    CPU_INT16U   chk_val_odd;
    CPU_INT16U   hamming_chk_even;
    CPU_INT16U   hamming_chk_odd;
    CPU_INT16U   hamming_even;
    CPU_INT16U   hamming_odd;
    CPU_BOOLEAN  one_bit_set;
    CPU_INT16U   word_mask;



                                                                /* ------- DETERMINE WHETHER ERR IS CORRECTABLE ------- */
    word_mask        = (CPU_INT16U)((len / sizeof(CPU_INT32U)) - 1u);
    chk_mask         = HAMMING_ADDR_MASK_BIT | word_mask;

    hamming_even     = (CPU_INT16U)((hamming     >>  0) & chk_mask);
    hamming_odd      = (CPU_INT16U)((hamming     >> 16) & chk_mask);

    hamming_chk_even = (CPU_INT16U)((hamming_chk >>  0) & chk_mask);
    hamming_chk_odd  = (CPU_INT16U)((hamming_chk >> 16) & chk_mask);

    chk_val_even     = hamming_even ^ hamming_chk_even;
    chk_val_odd      = hamming_odd  ^ hamming_chk_odd;
    chk_val          = chk_val_even ^ chk_val_odd;
    if (chk_val != chk_mask) {                                  /* Error ...                                                */
        one_bit_set = Hamming_ChkOneBitSet(chk_val_odd,         /*       ... cnt set bits        ...                        */
                                           chk_val_even);
        if (one_bit_set == DEF_YES) {                           /*       ... if ONLY one bit set ...                        */
           *p_err = ECC_ERR_ECC_CORRECTABLE;                    /*                               ... err in ECC ...         */
        } else {
           *p_err = ECC_ERR_UNCORRECTABLE;                      /*       ... otherwise uncorrectable err (see Note #1b).    */
        }
        return;
    }



                                                                /* ------------------ GET ADDR OF ERR ----------------- */
    Hamming_CalcErrLoc(            chk_val_odd,
                       (CPU_SIZE_T)word_mask + 1u,
                                   p_addr_octet,
                                   p_addr_bit);

   *p_err = ECC_ERR_CORRECTABLE;
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                        Hamming_CalcErrLoc()
*
* Description : Calculate error location.
*
* Argument(s) : addr_val        Address value.
*
*               len_word        Length of data, in words.
*
*               p_addr_octet    Pointer to variable that will receive the octet index in which a bit must
*                               be complemented to correct the data.
*
*               p_addr_bit      Pointer to variable that will receive the bit number that should be
*                               complemented in the octet to correct the data.
*
* Return(s)   : none.
*
* Caller(s)   : Hamming_Chk_256(),
*               Hamming_Eval().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  Hamming_CalcErrLoc (CPU_INT16U   addr_val,
                                  CPU_SIZE_T   len_word,
                                  CPU_SIZE_T  *p_addr_octet,
                                  CPU_INT08U  *p_addr_bit)
{
    CPU_INT08U  addr_bit;
    CPU_SIZE_T  addr_octet;
    CPU_SIZE_T  addr_word;
    CPU_INT16U  addr_word_temp;


    addr_bit       = (CPU_INT08U)((addr_val & HAMMING_ADDR_MASK_BIT)       >> 11);
    addr_word_temp = (CPU_INT16U)((addr_val & HAMMING_ADDR_MASK_WORD_8192) >>  0);
    addr_word      = (len_word - 1u) - addr_word_temp;

    addr_octet     = (CPU_SIZE_T)addr_word * (CPU_SIZE_T)(DEF_INT_32_NBR_BITS / DEF_INT_08_NBR_BITS);
    addr_octet    += (CPU_SIZE_T)addr_bit  / (CPU_SIZE_T) DEF_INT_08_NBR_BITS;
    addr_bit      %= (CPU_INT08U)DEF_INT_08_NBR_BITS;

   *p_addr_bit     = addr_bit;
   *p_addr_octet   = addr_octet;
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                     Hamming_ParCalcBitWord_32()
*
* Description : Calculate bit & word-wise parity for 32 4-byte words (a 128-byte buffer).
*
* Argument(s) : p_buf       Pointer to buffer that contains the data.
*
*               p_par_bit   Pointer to variable that will receive the  bit-wise parity.
*
*               p_par_word  Pointer to variable that will receive the word-wise parity.
*
* Return(s)   : none.
*
* Caller(s)   : Hamming_Calc()
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (ECC_HAMMING_CFG_OPTIMIZE_ASM_EN == DEF_DISABLED)
static  void  Hamming_ParCalcBitWord_32 (void        *p_buf,
                                         CPU_INT32U  *p_par_bit,
                                         CPU_INT32U  *p_par_word)
{
    CPU_ADDR     align_modulo;
    CPU_INT32U   par_bit;
    CPU_INT32U   par_word;
    CPU_INT32U   word;
    CPU_SIZE_T   word_ix;
    CPU_INT08U  *p_buf_08;
    CPU_INT32U  *p_buf_32;


    align_modulo = (CPU_ADDR)p_buf % sizeof(CPU_INT32U);
    par_word     = 0u;
    par_bit      = 0u;



    if (align_modulo == 0u) {                                   /* ---------- ACCESS MEM ON 4-BYTE BOUNDARIES --------- */
        p_buf_32 = (CPU_INT32U *)p_buf;

        for (word_ix = 0u; word_ix < 128u / sizeof(CPU_INT32U); word_ix++) {
            word       = *p_buf_32;
            par_bit   ^=  word;
            par_word <<=  1;
            par_word  |= (CPU_INT32U)Hamming_ParCalc_32(word);

            p_buf_32++;
        }


    } else {                                                    /* ---------- ACCESS MEM ON 1-BYTE BOUNDARIES --------- */
        p_buf_08 = (CPU_INT08U *)p_buf;

        for (word_ix = 0u; word_ix < 128u / sizeof(CPU_INT32U); word_ix++) {
            word       =  MEM_VAL_GET_INT32U((void *)p_buf_08);
            par_bit   ^=  word;
            par_word <<=  1;
            par_word  |= (CPU_INT32U)Hamming_ParCalc_32(word);

            p_buf_08  +=  sizeof(CPU_INT32U);
        }
    }

   *p_par_word = par_word;
   *p_par_bit  = par_bit;
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                    Hamming_ParCalcFinValOdd_32()
*
* Description : Calculate odd final parity values for 32-bit word.
*
* Argument(s) : datum       Datum to examine.
*
* Return(s)   : Final odd parity values.
*
* Caller(s)   : Hamming_Calc(),
*               Hamming_Calc_XXX().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  Hamming_ParCalcFinValOdd_32 (CPU_INT32U  datum)
{
    CPU_INT32U  datum_low;
    CPU_INT32U  datum_temp;
    CPU_INT32U  hamming;


                                                                /* Calc W16 or B16.                                     */
    datum_low    = (datum      >> 16);
    datum_temp   = (datum_low  >>  8) ^ datum_low;
    datum_temp   = (datum_temp >>  4) ^ datum_temp;
    datum_temp   = (datum_temp >>  2) ^ datum_temp;
    datum_temp   = (datum_temp >>  1) ^ datum_temp;
    hamming      =  datum_temp        & DEF_BIT_00;
    hamming    <<= 1;

                                                                /* Calc W08 or B08.                                     */
    datum        = (datum_low  >>  0) ^ datum;
    datum_low    = (datum      >>  8);
    datum_temp   = (datum_low  >>  4) ^ datum_low;
    datum_temp   = (datum_temp >>  2) ^ datum_temp;
    datum_temp   = (datum_temp >>  1) ^ datum_temp;
    hamming     |=  datum_temp        & DEF_BIT_00;
    hamming    <<= 1;

                                                                /* Calc W04 or B04.                                     */
    datum        = (datum_low  >>  0) ^ datum;
    datum_low    = (datum      >>  4);
    datum_temp   = (datum_low  >>  2) ^ datum_low;
    datum_temp   = (datum_temp >>  1) ^ datum_temp;
    hamming     |=  datum_temp        & DEF_BIT_00;
    hamming    <<= 1;

                                                                /* Calc W02 or B02.                                     */
    datum        = (datum_low  >>  0) ^ datum;
    datum_low    = (datum      >>  2);
    datum_temp   = (datum_low  >>  1) ^ datum_low;
    hamming     |=  datum_temp        & DEF_BIT_00;
    hamming    <<= 1;

                                                                /* Calc W01 or B01.                                     */
    datum        = (datum_low  >>  0) ^ datum;
    datum_low    = (datum      >>  1);
    hamming     |=  datum_low         & DEF_BIT_00;

    return (hamming);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                           Hamming_Log_2()
*
* Description : Calculate ceiling of base-2 logarithm of integer.
*
* Argument(s) : val         Integer value.
*
* Return(s)   : Logarithm.
*
* Caller(s)   : Hamming_Calc().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_SIZE_T  Hamming_Log_2 (CPU_SIZE_T  val)
{
    CPU_SIZE_T  log_val;
    CPU_SIZE_T  val_cmp;


    val_cmp = 1u;
    log_val = 0u;
    while (val_cmp < val) {
        val_cmp *= 2u;
        log_val += 1u;
    }

    return (log_val);
}


/*
*********************************************************************************************************
*                                        Hamming_ParCalc_32()
*
* Description : Calculate parity for 32-bit data value.
*
* Argument(s) : data_32     32-bit data value.
*
* Return(s)   : Parity.
*
* Caller(s)   : Hamming_Calc(),
*               Hamming_Calc_XXX(),
*               Hamming_ParCalcBitWord_32().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  Hamming_ParCalc_32 (CPU_INT32U  data_32)
{
    CPU_INT32U  par;


    data_32 = (data_32 >> 16) ^ data_32;
    data_32 = (data_32 >>  8) ^ data_32;
    data_32 = (data_32 >>  4) ^ data_32;
    data_32 = (data_32 >>  2) ^ data_32;
    data_32 = (data_32 >>  1) ^ data_32;

    par     =  data_32  & DEF_BIT_00;

    return (par);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                       Hamming_ChkOneBitSet()
*
* Description : Check whether only one bit set in check values.
*
* Argument(s) : chk_val_even    Even check value.
*
*               chk_val_odd     Odd  check value.
*
* Return(s)   : DEF_YES, if one bit set in check values.
*               DEF_NO,  otherwise.
*
* Caller(s)   : Hamming_Eval()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  Hamming_ChkOneBitSet (CPU_INT16U  chk_val_even,
                                           CPU_INT16U  chk_val_odd)
{
    CPU_INT08U  bit_ix;
    CPU_INT08U  bit_set_cnt;
    CPU_INT16U  chk_val;


    if ((chk_val_even == 0u) ||
        (chk_val_odd  == 0u)) {
        if (chk_val_even == 0u) {
            chk_val = chk_val_odd;
        } else {
            chk_val = chk_val_even;
        }
        bit_set_cnt = 0u;
        for (bit_ix = 0u; bit_ix < DEF_INT_16_NBR_BITS; bit_ix++) {
            if (DEF_BIT_IS_SET(chk_val, DEF_BIT(bit_ix)) == DEF_YES) {
                bit_set_cnt++;
                if (bit_set_cnt > 1u) {
                    return (DEF_NO);
                }
            }
        }
        if (bit_set_cnt == 1u) {
            return (DEF_YES);
        }
    }
    return (DEF_NO);
}

/*
*********************************************************************************************************
*                                     Hamming_BitCount32()
*
* Description : Check number of bits set in a number using precompiled tables (for fast calculation)
*
* Argument(s) : value           value in which bits set are counted
*
* Return(s)   : count           the number of bits set in number
*
* Caller(s)   : Hamming_Chk_256()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  Hamming_BitCount_32 (CPU_INT32U  number)
{
    CPU_INT32U result;
    CPU_INT08U octet;
    CPU_INT32U i;
    CPU_INT32U nb_octets;


    result    = 0u;
    nb_octets = DEF_INT_32_NBR_BITS / DEF_OCTET_NBR_BITS;

    for(i = 0u; i < nb_octets; i++) {
        octet    = (CPU_INT08U) (number & DEF_OCTET_MASK);      /* Count number of bit for one octet        */
        result   =  result + Hamming_BitCount[octet];           /* Add to the total                         */
        number >>=  DEF_OCTET_NBR_BITS;                         /* Shift to next octet                      */
    }

    return result;
}
