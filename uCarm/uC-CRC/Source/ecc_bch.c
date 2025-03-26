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
* Filename      : ecc_bch.c
* Version       : V1.07.00
* Programmer(s) : BAN
*********************************************************************************************************
* Reference(s)  : Lin, Chu & Daniel Costello.  Error Control Coding.  2nd Edition.  New Jersey: Pearson, 2004.
*********************************************************************************************************
* Note(s)       : (1) The implemented algorithms calculate BCH codes for 512-byte (4096-bit) buffers for
*                     variable correcting strengths.  The following variable names are adopted from Lin &
*                     Costello :
*
*                     (a) n is the block length, in bits.  The block length includes both the parity-check
*                         digits & the data.
*
*                     (b) m is specified so that n = 2^m - 1.
*                         (1) The Galois field GF(2^m) is used in the calculation.
*
*                     (c) k is the data length, in bits.
*
*                     (d) t is the number of errors that can be corrected.
*                         (1) n - k <= m * t; however, for small t, n - k == m * t.
*
*                         (2) n - k is the number of parity-check bits.
*
*                     Additional variable names are assigned :
*
*                     (a) len is the length of the buffer, in bits.  Here, 4096.
*
*                         (1) k NEED NOT be equal to the len; indeed, for this implementation, it WILL
*                             NOT be equal to the len.  Instead, an appropriate m must be chosen so that
*
*                                 n - (m * t) = 2^m - 1 - (m * t) = k >= len
*
*                             For a 4096-bit buffer, & 'reasonably small' t, m will be 13.  The number of
*                             parity check bits will be
*
*                                 # parity check bits <= m * t = 13 * t
*
*                             For 'small' t (up to approximately 64), the equality holds.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    ECC_BCH_MODULE
#include  <ecc_bch.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  BCH_GF_MAX_NBR_CONJ                              16

#define  BCH_MAX_GEN_POLY_LEN                            150

/*
*********************************************************************************************************
*                                   PRIMITIVE POLYNOMIALS OF GF(2)
*
* Note(s) : (1) Taken from [Reference 1] Table 2.7 'List of primitive polynomials'.
*********************************************************************************************************
*/

#define  BCH_GF_PRIM_POLY_03    (DEF_BIT_00 | DEF_BIT_01 |                           DEF_BIT_03)/* 1 + X   + X^3                */
#define  BCH_GF_PRIM_POLY_04    (DEF_BIT_00 | DEF_BIT_01 |                           DEF_BIT_04)/* 1 + X   + X^4                */
#define  BCH_GF_PRIM_POLY_05    (DEF_BIT_00 | DEF_BIT_02 |                           DEF_BIT_05)/* 1 + X^2 + X^5                */
#define  BCH_GF_PRIM_POLY_06    (DEF_BIT_00 | DEF_BIT_01 |                           DEF_BIT_06)/* 1 + X   + X^6                */
#define  BCH_GF_PRIM_POLY_07    (DEF_BIT_00 | DEF_BIT_03 |                           DEF_BIT_07)/* 1 + X^3 + X^7                */
#define  BCH_GF_PRIM_POLY_08    (DEF_BIT_00 | DEF_BIT_02 | DEF_BIT_03 | DEF_BIT_04 | DEF_BIT_08)/* 1 + X^2 + X^3  + X^4  + X^8  */
#define  BCH_GF_PRIM_POLY_09    (DEF_BIT_00 | DEF_BIT_04 |                           DEF_BIT_09)/* 1 + X^4 + X^9                */
#define  BCH_GF_PRIM_POLY_10    (DEF_BIT_00 | DEF_BIT_03 |                           DEF_BIT_10)/* 1 + X^3 + X^10               */
#define  BCH_GF_PRIM_POLY_11    (DEF_BIT_00 | DEF_BIT_02 |                           DEF_BIT_11)/* 1 + X^2 + X^11               */
#define  BCH_GF_PRIM_POLY_12    (DEF_BIT_00 | DEF_BIT_01 | DEF_BIT_04 | DEF_BIT_06 | DEF_BIT_12)/* 1 + X   + X^4  + X^6  + X^12 */
#define  BCH_GF_PRIM_POLY_13    (DEF_BIT_00 | DEF_BIT_01 | DEF_BIT_03 | DEF_BIT_04 | DEF_BIT_13)/* 1 + X   + X^3  + X^4  + X^13 */
#define  BCH_GF_PRIM_POLY_14    (DEF_BIT_00 | DEF_BIT_01 | DEF_BIT_06 | DEF_BIT_10 | DEF_BIT_14)/* 1 + X   + X^6  + X^10 + X^14 */
#define  BCH_GF_PRIM_POLY_15    (DEF_BIT_00 | DEF_BIT_01 |                           DEF_BIT_15)/* 1 + X   + X^15               */


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


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
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/

#define  BCH_GF_ADD(gf1, gf2)      BCH_GF_Add(m, p_gf, (gf1), (gf2))


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

static  void             BCH_ArrayShiftLeftBit(CPU_INT08U       *p_array,
                                               CPU_INT08U        len);

static  void             BCH_ArrayXOR         (CPU_INT08U       *p_array1,
                                               CPU_INT08U       *p_array2,
                                               CPU_INT08U        len);

static  BCH_GF_POLY      BCH_GF_GetPoly       (CPU_INT08U        m);

static  BCH_GF_POLY_PWR  BCH_GF_Add           (CPU_INT08U        m,
                                               BCH_GF_POLY      *pgf,
                                               BCH_GF_POLY_PWR   gf1,
                                               BCH_GF_POLY_PWR   gf2);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         BCH_GF_Construct()
*
* Description : Construct the Galois field GF(2^m).
*
* Argument(s) : m           Degree of the extension field of GF(2) to construct.
*
*               p_gf        Pointer to table that will receive Galois field elements.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) 'm' MUST be between 3 & 15, inclusive.
*                   (a) Larger 'm' could be accomodated by change 'pgf' to an array of 'CPU_INT32U'.
*
*               (2) 'p_gf' MUST point to an array of 2^m values.  Array overflow is NOT checked.
*
*               (3) The Galois field GF(2^m) is generated with the primitive polymonial of GF(2) or order
*                   m with the fewest terms, as given in [Reference 1].
*
*               (4) (a) The elements of GF(2^m) can be constructed recursively :
*
*                           alpha^i = alpha * alpha^(i - 1)
*
*                   (b) If an alpha^m term is introduced into element, this can be substituted out.  The
*                       generator polynomial, p(alpha) can be expressed as the highest-order term plus a
*                       remainder polynomial :
*
*                           p(alpha) = alpha^m + r(alpha)
*
*                       Setting this equal to zero & adding alpha^m :
*
*                           alpha^m = r(alpha).
*
*                       Consequently, r(alpha) should be substituted for alpha^m.
*********************************************************************************************************
*/

void  BCH_GF_Construct (CPU_INT08U    m,
                        BCH_GF_POLY  *p_gf)
{
    BCH_GF_POLY      gf_next;
    BCH_GF_POLY      gf_prev;
    BCH_GF_POLY      poly;
    BCH_GF_POLY      poly_hi;
    BCH_GF_POLY_PWR  pwr;
    BCH_GF_POLY_PWR  pwr_max;


                                                                /* ------------------- VALIDATE ARGS ------------------ */
    if ((m < 3) || (m > 15)) {                                  /* Validate 'm' (see Note #1).                          */
        return;
    }

    if (p_gf == (BCH_GF_POLY *)0) {                             /* Validate 'pgf' (see Note #2).                        */
        return;
    }



                                                                /* -------------------- CONSTRUCT GF ------------------ */
    poly    =  BCH_GF_GetPoly(m);                               /* Find prim poly (see Note #3).                        */
    poly_hi = (BCH_GF_POLY)DEF_BIT(m - 1);                      /* Second highest-order bit in poly.                    */
    gf_prev =  DEF_BIT_00;                                      /* Prev GF element.                                     */

   *p_gf    =  DEF_BIT_00;                                      /* 1st non-zero element (1 = alpha^0).                  */
    p_gf++;

                                                                /* Gen recursively (see Note #4a).                      */
    pwr_max = (BCH_GF_POLY_PWR)(1u << m);
    for (pwr = 1; pwr < pwr_max; pwr++) {
        gf_next = (BCH_GF_POLY)(gf_prev << 1);                  /* alpha^i = alpha * alpha^(i - 1).                     */

        if (DEF_BIT_IS_SET(gf_prev, poly_hi) == DEF_YES) {      /* Sub alpha^m = poly - alpha^m (see Note #4b).         */
            gf_next ^= poly;
        }

       *p_gf    = (BCH_GF_POLY)gf_next;
        p_gf++;

        gf_prev = gf_next;
    }
}


/*
*********************************************************************************************************
*                                         BCH_GF_MakeLogTbl()
*
* Description : Make logarithm table for Galois field.
*
* Argument(s) : m           Degree of the extension field of GF(2).
*
*               p_gf        Pointer to table of Galois field elements.
*
*               p_log_tbl   Pointer to table that will receive Galois field logarithms.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) 'm' MUST be between 3 & 15, inclusive.
*                   (a) Larger 'm' could be accomodated by change 'pgf' to an array of 'CPU_INT32U'.
*
*               (2) 'p_gf' MUST point to an array of 2^m values.  Array overflow is NOT checked.
*
*               (2) 'p_log_tbl' MUST point to an array of 2^m values.  Array overflow is NOT checked.
*
*               (4) The ith element of a Galois field GF(2^m), alpha^i, can be represented as a unique
*                   polynomial :
*
*                       alpha^i = a0 + a1*alpha + a2*alpha^2 + ... + a(m-1)*alpha^(m-1)
*
*                   where a0, a1, ..., a(m-1) are elements of GF(2) (i.e., 0 or 1).  The logarithm table
*                   allows the exponent in the power representation of a Galois field element to be
*                   determined in constant time from its polynomial representation.
*********************************************************************************************************
*/

void  BCH_GF_MakeLogTbl (CPU_INT08U        m,
                         BCH_GF_POLY      *p_gf,
                         BCH_GF_POLY_PWR  *p_log_tbl)
{
    BCH_GF_POLY       gf_element;
    BCH_GF_POLY_PWR   pwr;
    BCH_GF_POLY_PWR   pwr_max;
    BCH_GF_POLY      *p_gf_pos;


                                                                /* ------------------- VALIDATE ARGS ------------------ */
    if ((m < 3) || (m > 15)) {                                  /* Validate 'm' (see Note #1).                          */
        return;
    }

    if (p_gf == (BCH_GF_POLY *)0) {                             /* Validate 'pgf' (see Note #2).                        */
        return;
    }

    if (p_log_tbl == (BCH_GF_POLY_PWR *)0) {                    /* Validate 'p_log_tbl' (see Note #3).                  */
        return;
    }



                                                                /* ------------------- MAKE LOG TBL ------------------- */
    pwr_max  = (BCH_GF_POLY_PWR)(1u << m);
    p_gf_pos = p_gf;

                                                                /* 'Clr' tbl.                                           */
    Mem_Clr((void *)p_log_tbl, sizeof(BCH_GF_POLY_PWR) * pwr_max);

    for (pwr = 0; pwr < pwr_max - 1; pwr++) {
        gf_element = *p_gf_pos;
        if ((gf_element == 0) ||                                /* Chk GF val.                                          */
            (gf_element >  pwr_max - 1)) {
            Mem_Clr((void *)p_log_tbl, sizeof(BCH_GF_POLY_PWR) * pwr_max);
            return;
        }

        if (p_log_tbl[gf_element] != 0) {                       /* Chk if log tbl pos occupied.                         */
            Mem_Clr((void *)p_log_tbl, sizeof(BCH_GF_POLY_PWR) * pwr_max);
            return;
        }

        p_log_tbl[gf_element] = pwr;                            /* Place val in tbl.                                    */

        p_gf_pos++;
    }
}


/*
*********************************************************************************************************
*                                        BCH_GF_CalcMinPolys()
*
* Description : Calculate minimal polynomials of alpha, alpha^3, ... alpha^(2^(m-1)-3), alpha^(2^(m-1)-1).
*
* Argument(s) : m           Degree of the extension field of GF(2).
*
*               p_gf        Pointer to table of Galois field elements.
*
*               p_min_tbl   Pointer to table that will receive the minimal polynomials.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) 'm' MUST be between 3 & 15, inclusive.
*                   (a) Larger 'm' could be accomodated by change 'p_gf' & 'p_min_tbl' to arrays of 'CPU_INT32U'.
*
*               (2) 'p_gf' MUST point to an array of 2^m values.  Array overflow is NOT checked.
*
*               (3) 'p_min_tbl' MUST point to an array of 2^(m-2) values.  Array overflow is NOT checked.
*
*               (4) The minimal polynomials are generated with the primitive polymonial of GF(2) of order
*                   m with the fewest terms, as given in [Reference 1].
*                   (a) If a minimal polynomial is zero, then it corresponds to a generator that is a
*                       conjugate of an earlier generator.
*
*               (5) The minimal polynomial has roots beta^2^0, beta^2^1, ... beta^2^(e - 1); consequently,
*                   it is the product
*
*                                  e-1
*                       phi(X) = PRODUCT [X + beta^2^i]
*                                  i=0
*
*                   (a) These terms are multiplied one-by-one to form the minimal polynomial.  At any
*                       point, 'min_poly_coeff[]' contains the 'element index' of the coefficients of the
*                       polynomial :
*
*                           If min_poly_coeff[j] = -1, the coefficient is 0       = GF(2^m) element 0.
*                           If min_poly_coeff[j] =  0, the coefficient is 1       = GF(2^m) element 1.
*                           If min_poly_coeff[j] =  1, the coefficient is alpha   = GF(2^m) element 2.
*                                                .
*                                                .
*                           If min_poly_coeff[j] =  b, the coefficient is alpha^b = GF(2^m) element b-1.
*
*                   (b) Multiplying a term into the current polynomial involves a polynomial
*                       multiplication like :
*
*                           p(X) * (X + alpha^j) = (alpha^a(i) * X^(i) + alpha^a(i-1) * X^(i - 1) + ... + alpha^a1 * X^1 + alpha^a0) * (X + alpha^j)
*                                                =  alpha^a(i) * X^(i+1) + (alpha^(a(i) + j) + alpha^a(i-1)) * X^i + ... + (alpha^(a1 + j) + alpha^a0) * X^1 + alpha^j
*
*                       So the new coefficient of X^i can be computed from the old coefficients of X^i & X^(i-1) :
*
*                           alpha^(a(i) + j) + alpha^a(i-1)
*
*                       which equals alpha^anew for some 0 <= anew <= 2^m.  The value of anew is determined
*                       by a reverse look-up within the table of Galois field elements.
*********************************************************************************************************
*/


void  BCH_GF_CalcMinPolys (CPU_INT08U    m,
                           BCH_GF_POLY  *p_gf,
                           BCH_GF_POLY  *p_min_tbl)
{
    BCH_GF_POLY_PWR  coeff_ix;
    BCH_GF_POLY_PWR  coeff_max;
    BCH_GF_POLY_PWR  min_poly_coeff[BCH_GF_MAX_NBR_CONJ];
    BCH_GF_POLY      min_poly;
    BCH_GF_POLY_PWR  j;
    BCH_GF_POLY_PWR  pwr;
    BCH_GF_POLY_PWR  pwr_max;


                                                                /* ------------------- VALIDATE ARGS ------------------ */
    if ((m < 3) || (m > 15)) {                                  /* Validate 'm' (see Note #1).                          */
        return;
    }

    if (p_gf == (BCH_GF_POLY *)0) {                             /* Validate 'p_gf' (see Note #2).                       */
        return;
    }

    if (p_min_tbl == (BCH_GF_POLY *)0) {                        /* Validate 'p_min_tbl' (see Note #3).                  */
        return;
    }



                                                                /* ------------------ CALC MIN POLYS ------------------ */
    pwr_max = (BCH_GF_POLY_PWR)(1u << m);
    for (pwr = 1; pwr <= (pwr_max / 2) - 1; pwr += 2) {
        min_poly_coeff[0] = 0;                                  /* Init min poly result to alpha^0 = 1.                 */
        for (coeff_ix = 1; coeff_ix < BCH_GF_MAX_NBR_CONJ; coeff_ix++) {
            min_poly_coeff[coeff_ix] = DEF_INT_16U_MAX_VAL;
        }

        coeff_max = 0;
        j         = pwr;

        do {
            if (j < pwr) {                                      /* Pwr is conj of earlier pwr.                          */
                coeff_max = 0;
                break;
            }

                                                                /* Mult most sig term by X.                             */
            min_poly_coeff[coeff_max + 1] = min_poly_coeff[coeff_max];

                                                                /* Form new coeff (see Note #5b).                       */
            for (coeff_ix = coeff_max; coeff_ix > 0; coeff_ix--) {
                                                                /* If ai != 0 ...                                       */
                if (min_poly_coeff[coeff_ix] != DEF_INT_16U_MAX_VAL) {
                    min_poly_coeff[coeff_ix] +=  j;             /* ... atemp = (ai + j) % (2^m) ...                     */
                    if (min_poly_coeff[coeff_ix] >= pwr_max - 1) {
                        min_poly_coeff[coeff_ix] -= pwr_max - 1;
                    }
                                                                /* ... anew  = lookup(alpha^atemp + alpha^a(i-1)).      */
                    min_poly_coeff[coeff_ix] = BCH_GF_ADD(min_poly_coeff[coeff_ix], min_poly_coeff[coeff_ix - 1]);

                                                                /* If ai == 0 ...                                       */
                } else {                                        /* ... anew = a(i-1).                                   */
                    min_poly_coeff[coeff_ix] = min_poly_coeff[coeff_ix - 1];
                }
            }
            if (min_poly_coeff[0] != DEF_INT_16U_MAX_VAL) {     /* Mult least sig term by alpha^j.                      */
                min_poly_coeff[0] +=  j;
                if (min_poly_coeff[0] >= pwr_max - 1) {
                    min_poly_coeff[0] -= pwr_max - 1;
                }
            } else {
                min_poly_coeff[0] = DEF_INT_16U_MAX_VAL;
            }
            coeff_max++;

            j *= 2;
            if (j >= pwr_max) {
                j -= pwr_max - 1;
            }
        } while (j != pwr);

        min_poly = DEF_BIT_NONE;                                /* Create min poly.                                     */
        for (coeff_ix = 0; coeff_ix <= coeff_max; coeff_ix++) {
            if (min_poly_coeff[coeff_ix] == 0) {
                min_poly |= (BCH_GF_POLY)DEF_BIT(coeff_ix);
            } else if (min_poly_coeff[coeff_ix] != DEF_INT_16U_MAX_VAL) {

            }
        }

       *p_min_tbl = min_poly;
        p_min_tbl++;
    }
}


/*
*********************************************************************************************************
*                                      BCH_GF_CalcBCH_GenPoly()
*
* Description : Calculate BHC generator polynomial using Galois field GF(2^m) that will be able to
*               correct at least t bits.
*
* Argument(s) : m           Degree of the extension field of GF(2).
*
*               t           Minimum number of correctable data bits.
*
*               p_gf        Pointer to table of Galois field elements.
*
*               p_min_tbl   Pointer to table of minimal polynomials.
*
*               p_bch_gen   Pointer to buffer that will receive generator polynomial.
*
* Return(s)   : The degree of the generator polynomial (or length of polynomial, in bits).
*
* Caller(s)   : Application.
*
* Note(s)     : (1) 'm' MUST be between 3 & 15, inclusive.
*                   (a) Larger 'm' could be accomodated by change 'p_gf' & 'p_min_tbl' to arrays of 'CPU_INT32U'.
*
*               (2) 'p_gf' MUST point to an array of 2^m values.  Array overflow is NOT checked.
*
*               (3) 'p_min_tbl' MUST point to an array of 2^(m-2) values.  Array overflow is NOT checked.
*
*               (4) If 'p_bch_gen' is not long enough, then an error is returned.
*                   (a) For small 't', the BCH generator polynomial will be m * t bits.
*
*               (5) The minimal polynomial(s) are binary multiplied to form the generator polynomial.
*                   If B is the current minimal polynomial & G is the current generator polynomial, then
*                   these may be written
*
*                       B = b0 + b1*X + b2*X^2 + ... + bm*X^m
*                       G = g0 + g1*X + g2*X^2 + ... + gv*X^v
*
*                   The product B * G can be re-written
*
*                       B * G = (((bm * G) * X + bm-1 * G) * X + ... + b1 * G) * X + b0 * G
*
*                   Recursively, the successive partial products are formed :
*
*                       G0 = bm * G * X
*                       Gi = (Gi-1 + bm-i-2 * G) * X   (1 <= i < m)
*                       G  = Gm + b0 * G
*
*                   Since G must be preserved for the entire product, the partial products are stored in
*                   a temporary (local) buffer.
*********************************************************************************************************
*/

CPU_INT16U  BCH_GF_CalcBCH_GenPoly (CPU_INT08U    m,
                                    CPU_INT08U    t,
                                    BCH_GF_POLY  *p_gf,
                                    BCH_GF_POLY  *p_min_tbl,
                                    CPU_INT08U   *p_bch_gen)
{
    CPU_INT08U       bch_gen_bit;
    CPU_INT16U       bch_gen_deg;
    CPU_INT16U       bch_gen_ix;
    CPU_INT16U       bch_gen_len;
    BCH_GF_POLY_PWR  bch_gen_len_max;
    CPU_INT08U       bch_gen_poly;
    CPU_INT08U       bch_gen_temp[BCH_MAX_GEN_POLY_LEN];
    CPU_INT08U       carry_last;
    CPU_INT08U       carry_next;
    BCH_GF_POLY      min_poly;
    BCH_GF_POLY      min_poly_mask;
    BCH_GF_POLY_PWR  poly_ix_max;
    BCH_GF_POLY_PWR  poly_ix;
    CPU_INT08U       t_cur;


                                                                /* ------------------- VALIDATE ARGS ------------------ */
    if ((m < 3) || (m > 15)) {                                  /* Validate 'm' (see Note #1).                          */
        return ((CPU_INT16U)0);
    }

    if (t > 254) {                                              /* Validate 't'.                                        */
        return ((CPU_INT16U)0);
    }

    if (p_gf == (BCH_GF_POLY *)0) {                             /* Validate 'p_gf' (see Note #2).                       */
        return ((CPU_INT16U)0);
    }

    if (p_min_tbl == (BCH_GF_POLY *)0) {                        /* Validate 'p_min_tbl' (see Note #3).                  */
        return ((CPU_INT16U)0);
    }

    if (p_bch_gen == (CPU_INT08U *)0) {                         /* Validate 'p_bch_gen'.                                */
        return ((CPU_INT16U)0);
    }

                                                                /* Validate gen len (see Note #4).                      */
    bch_gen_len_max = (BCH_GF_POLY_PWR)m * (BCH_GF_POLY_PWR)t;
    if (BCH_MAX_GEN_POLY_LEN < (bch_gen_len_max + DEF_INT_08_NBR_BITS - 1) / DEF_INT_08_NBR_BITS) {
        return ((CPU_INT16U)0);
    }



                                                                /* ------------------- CALC GEN POLY ------------------ */
    bch_gen_len = DEF_MIN(BCH_MAX_GEN_POLY_LEN, (bch_gen_len_max + DEF_INT_08_NBR_BITS - 1) / DEF_INT_08_NBR_BITS + 1);

    Mem_Clr((void *)bch_gen_temp, (CPU_SIZE_T)bch_gen_len);
    Mem_Clr((void *)p_bch_gen,     (CPU_SIZE_T)bch_gen_len);
    p_bch_gen[0] = DEF_BIT_00;                                  /* Init poly to 1.                                      */

    t_cur       =  0;
    poly_ix_max = (BCH_GF_POLY_PWR)(1u << m);
    for (poly_ix = 1; poly_ix <= (poly_ix_max / 2) - 1; poly_ix += 2) {
        min_poly = *p_min_tbl;

        if (min_poly != 0) {                                    /* Mult BCH gen poly by min poly.                       */
            min_poly_mask = (BCH_GF_POLY)DEF_BIT(m);
            while (min_poly_mask != DEF_BIT_00) {
                                                                /* Add in next bit to partial prod: Gi + bm-i-1 * G.    */
                if (DEF_BIT_IS_SET(min_poly, min_poly_mask) == DEF_YES) {
                    for (bch_gen_ix = 0; bch_gen_ix < bch_gen_len; bch_gen_ix++) {
                        bch_gen_temp[bch_gen_ix] ^= p_bch_gen[bch_gen_ix];
                    }
                }

                carry_last = DEF_BIT_NONE;                      /* Mult cur partial prod by X.                          */
                for (bch_gen_ix = 0; bch_gen_ix < bch_gen_len; bch_gen_ix++) {
                    carry_next               = (DEF_BIT_IS_SET(bch_gen_temp[bch_gen_ix], DEF_BIT_07) == DEF_YES) ? DEF_BIT_00 : DEF_BIT_NONE;
                    bch_gen_temp[bch_gen_ix] = (CPU_INT08U)(bch_gen_temp[bch_gen_ix] << 1) | carry_last;
                    carry_last               = carry_next;
                }
                min_poly_mask >>= 1;
            }

                                                                /* Add in final bit to partial prod: Gi + bm-i-1 * G.   */
            if (DEF_BIT_IS_SET(min_poly, DEF_BIT_00) == DEF_YES) {
                for (bch_gen_ix = 0; bch_gen_ix < bch_gen_len; bch_gen_ix++) {
                    bch_gen_temp[bch_gen_ix] ^= p_bch_gen[bch_gen_ix];
                }
            }

                                                                /* Copy final prod to real buf & clr temp buf.          */
            for (bch_gen_ix = 0; bch_gen_ix < bch_gen_len; bch_gen_ix++) {
                p_bch_gen[bch_gen_ix]    = bch_gen_temp[bch_gen_ix];
                bch_gen_temp[bch_gen_ix] = 0;
            }
        }

        t_cur++;
        p_min_tbl++;

        if (t_cur >= t) {
            break;
        }
    }

    if (t_cur < t) {
        return ((CPU_INT16U)0);
    }



                                                                /* ------------------- CALC POLY DEG ------------------ */
    for (bch_gen_ix = bch_gen_len; bch_gen_ix > 0; bch_gen_ix--) {
        bch_gen_poly = p_bch_gen[bch_gen_ix - 1];
        if (bch_gen_poly != 0) {
            bch_gen_bit = 8;
            while (bch_gen_bit != 0) {
                if (DEF_BIT_IS_SET(bch_gen_poly, DEF_BIT(bch_gen_bit - 1)) == DEF_YES) {
                    bch_gen_deg = (CPU_INT16U)((bch_gen_ix - 1) * DEF_INT_08_NBR_BITS) + bch_gen_bit - 1;
                    return (bch_gen_deg);
                }
                bch_gen_bit--;
            }
        }
    }

    return ((CPU_INT16U)0);
}


/*
*********************************************************************************************************
*                                    BCH_GF_CalcBCH_GenTblEntry()
*
* Description : Calculate BHC generator table entry for generator polynomial.
*
* Argument(s) : p_bch_gen       Pointer to generator polynomial.
*
*               gen_poly_deg    Degree of generator polynomial.
*
*               p_gen_tbl_entry Pointer to array that will receive the generator table entry.
*
*               tbl_ix          Index of entry in table.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  BCH_GF_CalcBCH_GenTblEntry (CPU_INT08U  *p_bch_gen,
                                  CPU_INT16U   gen_poly_deg,
                                  CPU_INT08U  *p_gen_tbl_entry,
                                  CPU_INT08U   tbl_ix)
{
    CPU_INT08U  gen_poly_octets;
    CPU_INT08U  gen_poly_rem;
    CPU_INT08U  bit_ix;


    gen_poly_octets = (CPU_INT08U)((gen_poly_deg + DEF_OCTET_NBR_BITS - 1) / DEF_OCTET_NBR_BITS);
    gen_poly_rem    = (CPU_INT08U)( gen_poly_deg                           % DEF_OCTET_NBR_BITS);

                                                                /* Clr entry.                                           */
    Mem_Clr((void *)p_gen_tbl_entry, (CPU_SIZE_T)gen_poly_octets);

    if (gen_poly_deg < 8) {
        return;
    }

    p_gen_tbl_entry[gen_poly_octets - 1] = tbl_ix;              /* Assign 'tbl_ix' to highest 8-bits in entry.          */

                                                                /* Shift data through entry.                            */
    for (bit_ix = 0; bit_ix < DEF_OCTET_NBR_BITS; bit_ix++) {
        if (DEF_BIT_IS_SET(p_gen_tbl_entry[gen_poly_octets - 1], DEF_BIT_07) == DEF_YES) {
            BCH_ArrayShiftLeftBit(p_gen_tbl_entry, gen_poly_octets);
            BCH_ArrayXOR(p_gen_tbl_entry, p_bch_gen, gen_poly_octets);
        } else {
            BCH_ArrayShiftLeftBit(p_gen_tbl_entry, gen_poly_octets);
        }
    }

    if (gen_poly_rem != 0) {                                    /* Clr unused bits.                                     */
        p_gen_tbl_entry[0] &= ~DEF_BIT_FIELD(8 - gen_poly_rem, 0);
    }
}


/*
*********************************************************************************************************
*                                         BCH_GF_CalcAuxTbl()
*
* Description : Calculate auxiliary table.
*
* Argument(s) : m           Degree of the extension field of GF(2).
*
*               poly        Polynomial.
*
*               p_aux_tbl   Pointer to array that will receive the auxiliary table.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  BCH_GF_CalcAuxTbl (CPU_INT08U    m,
                         BCH_GF_POLY   poly,
                         BCH_GF_POLY  *p_aux_tbl)
{
    CPU_INT08U    bit_ix;
    BCH_GF_POLY   mask;
    BCH_GF_POLY   rem;
    CPU_INT16U    tbl_ix;


                                                                /* ------------------- VALIDATE PTRS ------------------ */
    if (p_aux_tbl == (BCH_GF_POLY *)0) {
        return;
    }

    Mem_Clr((void *)p_aux_tbl, sizeof(BCH_GF_POLY) * 256);

    if ((m < 3) || (m > 15)) {
        return;
    }



                                                                /* ---------------------- MAKE TBL -------------------- */
    poly <<=  16u - m;                                          /* Move poly to uppermost bits.                         */
    mask   = (BCH_GF_POLY)DEF_BIT_FIELD(m, 16 - m);

    for (tbl_ix = 0; tbl_ix <= DEF_INT_08U_MAX_VAL; tbl_ix++) {
        rem   = (BCH_GF_POLY)tbl_ix;
        rem <<= 8;

        for (bit_ix = 0; bit_ix < 8; bit_ix++) {
            if (DEF_BIT_IS_SET(rem, DEF_BIT_15) == DEF_YES) {
                rem = (CPU_INT16U)(rem << 1) ^ poly;
            } else {
                rem <<= 1;
            }
        }

       *p_aux_tbl = (BCH_GF_POLY)(rem & mask);
        p_aux_tbl++;
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       BCH_ArrayShiftLeftBit()
*
* Description : Shift array left one bit.
*
* Argument(s) : p_array     Pointer to array to shift.
*
*               len         Length of array, in octets.
*
* Return(s)   : none.
*
* Caller(s)   : BCH_GF_CalcBCH_GenTblEntry().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BCH_ArrayShiftLeftBit (CPU_INT08U  *p_array,
                                     CPU_INT08U   len)
{
    CPU_INT08U   i;
    CPU_INT08U   bit_shift;
    CPU_INT08U   bit_shift_next;
    CPU_INT08U   entry;


    bit_shift = DEF_BIT_NONE;
    for (i = 0; i < len; i++) {
        entry          = *p_array;
        bit_shift_next = (DEF_BIT_IS_SET(entry, DEF_BIT_07) == DEF_YES) ? DEF_BIT_00 : DEF_BIT_NONE;

       *p_array        = (CPU_INT08U)(entry << 1) | bit_shift;
        p_array       +=  sizeof(CPU_INT08U);

        bit_shift      =  bit_shift_next;
     }
}


/*
*********************************************************************************************************
*                                           BCH_ArrayXOR()
*
* Description : XOR one array with another array.
*
* Argument(s) : p_array1    Pointer to first  array.
*
*               p_array2    Pointer to second array.
*
*               len         Length of arrays, in octets.
*
* Return(s)   : none.
*
* Caller(s)   : BCH_GF_CalcBCH_GenTblEntry().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BCH_ArrayXOR (CPU_INT08U  *p_array1,
                            CPU_INT08U  *p_array2,
                            CPU_INT08U   len)
{
    CPU_INT08U  i;


    for (i = 0; i < len; i++) {
        *p_array1 ^= *p_array2;
         p_array1 +=  sizeof(CPU_INT08U);
         p_array2 +=  sizeof(CPU_INT08U);
    }
}


/*
*********************************************************************************************************
*                                          BCH_GF_GetPoly()
*
* Description : Get primitive polynomial for GF calculation.
*
* Argument(s) : m               Degree of the extension field of GF(2).
*
* Return(s)   : Polynomial.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  BCH_GF_POLY  BCH_GF_GetPoly (CPU_INT08U  m)
{
    BCH_GF_POLY  poly;


    switch (m) {
        case 3:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_03;
             break;

        case 4:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_04;
             break;

        case 5:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_05;
             break;

        case 6:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_06;
             break;

        case 7:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_07;
             break;

        case 8:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_08;
             break;

        case 9:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_09;
             break;

        case 10:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_10;
             break;

        case 11:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_11;
             break;

        case 12:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_12;
             break;

        case 13:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_13;
             break;

        case 14:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_14;
             break;

        case 15:
             poly = (BCH_GF_POLY)BCH_GF_PRIM_POLY_15;
             break;

        default:
             poly = (BCH_GF_POLY)0;
             break;
    }

    return (poly);
}


/*
*********************************************************************************************************
*                                            BCH_GF_Add()
*
* Description : Add two Galois field elements.
*
* Argument(s) : m           Degree of the extension field of GF(2).
*
*               p_gf        Pointer to table of Galois field elements.
*
*               gf1         First element.
*
*               gf2         Second element.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  BCH_GF_POLY_PWR  BCH_GF_Add (CPU_INT08U        m,
                                     BCH_GF_POLY      *p_gf,
                                     BCH_GF_POLY_PWR   gf1,
                                     BCH_GF_POLY_PWR   gf2)
{
    BCH_GF_POLY_PWR  gf_sum;
    BCH_GF_POLY_PWR  gf_tmp;
    BCH_GF_POLY_PWR  pwr;
    BCH_GF_POLY_PWR  pwr_max;


    if (gf1 == DEF_INT_16U_MAX_VAL) {
        gf_sum = gf2;
    } else if (gf2 == DEF_INT_16U_MAX_VAL) {
        gf_sum = gf1;
    } else {
        gf_tmp  =  p_gf[gf1] ^ p_gf[gf2];
        pwr_max = (BCH_GF_POLY_PWR)(1u << m);

        if (gf_tmp == 0) {
            gf_sum = DEF_INT_16U_MAX_VAL;
        } else {
            gf_sum = DEF_INT_16U_MAX_VAL;
            for (pwr = 0; pwr < pwr_max; pwr++) {
                if (*p_gf == gf_tmp) {
                    gf_sum = pwr;
                    break;
                }

                p_gf++;
            }
        }
    }

    return (gf_sum);
}
