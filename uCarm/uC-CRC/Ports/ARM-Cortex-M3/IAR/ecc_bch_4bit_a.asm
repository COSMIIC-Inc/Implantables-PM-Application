;********************************************************************************************************
;                                                uC/CRC
;           ERROR DETECTING CODE (EDC) & ERROR CORRECTING CODE (ECC) CALCULATION UTILITIES
;
;                         (c) Copyright 2007-2010; Micrium, Inc.; Weston, FL
;
;               All rights reserved. Protected by international copyright laws.
;
;               uC/FS is provided in source form to registered licensees ONLY.  It is
;               illegal to distribute this source code to any third party unless you receive
;               written permission by an authorized Micrium representative.  Knowledge of
;               the source code may NOT be used to develop a similar product.
;
;               Please help us continue to provide the Embedded community with the finest
;               software available.  Your honesty is greatly appreciated.
;
;               You can contact us at www.micrium.com.
;********************************************************************************************************


;********************************************************************************************************
;
;                      BINARY BOSE-CHAUDHURI-HOCQUENGHEM (BCH) CODE CALCULATION
;
;                                            ARM-Cortex-M3
;                                            IAR Compiler
;
; Filename      : ecc_bch_4bit_a.asm
; Version       : V1.07
; Programmer(s) : BAN
;********************************************************************************************************
; Note(s)       : (1) Assumes ARM CPU mode configured for Little Endian.
;********************************************************************************************************


;********************************************************************************************************
;                                          PUBLIC FUNCTIONS
;********************************************************************************************************

        PUBLIC BCH_4Bit_Calc
        PUBLIC BCH_4Bit_CalcEx


;********************************************************************************************************
;                                          EXTERN VARIABLES
;********************************************************************************************************

        EXTERN BCH_4Bit_GenTbl


;********************************************************************************************************
;                                     CODE GENERATION DIRECTIVES
;********************************************************************************************************

        RSEG CODE:CODE:NOROOT(2)


;********************************************************************************************************
;                                           BCH_4Bit_Calc()
;
; Description : Calculate 4-bit BCH code for 512-byte data buffer.
;
; Argument(s) : p_buf       Pointer to buffer that contains the data.
;
;               p_ecc       Pointer to buffer that will receive ECC (see Note #1a).
;
;               p_err       Pointer to variable that will receive the return error code from this function :
;
;                               ECC_ERR_NONE        Hamming code calculated.
;                               ECC_ERR_NULL_PTR    Argument 'p_buf' passed a NULL pointer.
;
; Return(s)   : none.
;
; Caller(s)   : Application.
;
; Note(s)     : (1) The return parameter 'p_ecc' must point to a valid 7-byte buffer.
;
;               (2) The 52-bit BCH code is stored in the 7-byte buffer.
;
;               (3) Data buffers larger than 512 bytes should  be divided into 512-byte segments, with
;                   the ECCs calculated for the individual segments concatenated to form the ECC for the
;                   entire buffer.
;********************************************************************************************************

;void  BCH_4Bit_Calc (void        *p_buf,        @ ==> R0
;                     CPU_INT08U  *p_ecc,        @ ==> R1
;                     CPU_ERR     *p_err)        @ ==> R2
;                                  p_gen_tbl     @ ==> R3
;                                  ecc0          @ ==> R4
;                                  ecc1          @ ==> R5
;                                  nbr_octets    @ ==> R6
;                                  data          @ ==> R7
;                                  gen_entry[0]  @ ==> R7
;                                  gen_entry[1]  @ ==> R8
;                                  temp          @ ==> R9

BCH_4Bit_Calc:
        STMDB  SP!, {R0, R3-R9, LR}

        CMP    R1, #0                   ; if (p_ecc == (void *)0) {
        BNE    BCH_4Bit_Calc_0

        MOV    R3, #1                   ;    *p_err =  ECC_ERR_NULL_PTR;
        STRH   R3, [R2]
        LDMIA  SP!, {R0, R3-R9, LR}     ;     return;
        BX     LR                       ; }


BCH_4Bit_Calc_0:
        CMP    R0, #0                   ; if (p_buf == (CPU_INT08U *)0) {
        BNE    BCH_4Bit_Calc_1

        MOV    R3, #1                   ;    *p_err =  ECC_ERR_NULL_PTR;
        STRH   R3, [R2]
        LDMIA  SP!, {R0, R3-R9, LR}     ;     return;
        BX     LR                       ; }


BCH_4Bit_Calc_1:
        LDR    R3, =BCH_4Bit_GenTbl

        MOV    R4, #0                   ; ecc0 = 0
        MOV    R5, #0                   ; ecc1 = 0

        MOV    R6, #512


BCH_4Bit_Calc_Loop:
        LDRB   R7, [R0], #+1            ; data = *p_buf_08++

        EOR    R9,  R7,  R5, LSR #16    ; R9   = ((ecc1 >> 16) & 0x00FF) ^ data
        AND    R9,  R9,  #0xFF

        ADD    R9,  R3,  R9, LSL #3     ; R9   = p_gen_tbl + (R9 * 8)
        LDM    R9, {R7-R8}

        MOV    R9,  R4,  LSR #24        ; R9   =  (ecc0 >> 24)
        ORR    R5,  R9,  R5, LSL #8     ; R5   =  (ecc0 >> 24) | (ecc1 << 8)
        EOR    R5,  R8,  R5             ; ecc1 = ((ecc0 >> 24) | (ecc1 << 8)) ^ p_gen_tbl_entry[1]

        EOR    R4,  R7,  R4, LSL #8     ; ecc0 =                 (ecc0 << 8)  ^ p_gen_tbl_entry[0]

        SUBS   R6,  R6, #1

        BNE    BCH_4Bit_Calc_Loop


        STRB   R4, [R1, #+6]
        MOV    R4, R4, LSR #8
        STRB   R4, [R1, #+5]
        MOV    R4, R4, LSR #8
        STRB   R4, [R1, #+4]
        MOV    R4, R4, LSR #8
        STRB   R4, [R1, #+3]

        STRB   R5, [R1, #+2]
        MOV    R5, R5, LSR #8
        STRB   R5, [R1, #+1]
        MOV    R5, R5, LSR #8
        STRB   R5, [R1, #+0]


        LDMIA  SP!, {R0, R3-R9, LR}
        BX     LR


;********************************************************************************************************
;                                          BCH_4Bit_CalcEx()
;
; Description : Calculate 8-bit BCH code for 512-byte data buffer & previous BCH code.
;
; Argument(s) : p_buf       Pointer to buffer that contains the data.
;
;               p_ecc_chk   Pointer to buffer that contains the ECC to check.
;
;               p_ecc       Pointer to buffer that will receive ECC (see Note #1a).
;
;               p_err       Pointer to variable that will receive the return error code from this function :
;
;                               ECC_ERR_NONE        Hamming code calculated.
;                               ECC_ERR_NULL_PTR    Argument 'p_buf' passed a NULL pointer.
;
; Return(s)   : none.
;
; Caller(s)   : Application.
;
; Note(s)     : (1) The return parameter 'p_ecc' must point to a valid 7-byte buffer.
;
;               (2) The 52-bit BCH code is stored in the 7-byte buffer.
;
;               (3) Data buffers larger than 512 bytes should  be divided into 512-byte segments, with
;                   the ECCs calculated for the individual segments concatenated to form the ECC for the
;                   entire buffer.
;********************************************************************************************************

;void  BCH_4Bit_CalcEx (void        *p_buf,        @ ==> R0
;                       CPU_INT08U  *p_ecc_chk,    @ ==> R1
;                       CPU_INT08U  *p_ecc,        @ ==> R2
;                       CPU_ERR     *p_err)        @ ==> R3
;                                    p_gen_tbl     @ ==> R3
;                                    ecc0          @ ==> R4
;                                    ecc1          @ ==> R5
;                                    nbr_octets    @ ==> R6
;                                    data          @ ==> R7
;                                    gen_entry[0]  @ ==> R7
;                                    gen_entry[1]  @ ==> R8
;                                    temp          @ ==> R9

BCH_4Bit_CalcEx:
        STMDB  SP!, {R0, R3-R9, LR}

        CMP    R1, #0                   ; if (p_ecc == (void *)0) {
        BNE    BCH_4Bit_CalcEx_0

        MOV    R4, #1                   ;    *p_err =  ECC_ERR_NULL_PTR;
        STRH   R4, [R3]
        LDMIA  SP!, {R0, R3-R9, LR}     ;     return;
        BX     LR                       ; }


BCH_4Bit_CalcEx_0:
        CMP    R2, #0                   ; if (p_ecc_chk == (CPU_INT08U *)0) {
        BNE    BCH_4Bit_CalcEx_1

        MOV    R4, #1                   ;    *p_err =  ECC_ERR_NULL_PTR;
        STRH   R4, [R3]
        LDMIA  SP!, {R0, R3-R9, LR}     ;     return;
        BX     LR                       ; }


BCH_4Bit_CalcEx_1:
        CMP    R0, #0                   ; if (p_buf == (CPU_INT08U *)0) {
        BNE    BCH_4Bit_CalcEx_2

        MOV    R4, #1                   ;    *p_err =  ECC_ERR_NULL_PTR;
        STRH   R4, [R3]
        LDMIA  SP!, {R0, R3-R9, LR}     ;     return;
        BX     LR                       ; }


BCH_4Bit_CalcEx_2:
        LDR    R3, =BCH_4Bit_GenTbl

        MOV    R4, #0                   ; ecc0 = 0
        MOV    R5, #0                   ; ecc1 = 0

        MOV    R6, #512


BCH_4Bit_CalcEx_Loop1:
        LDRB   R7, [R0], #+1            ; data = *p_buf_08++

        EOR    R9,  R7,  R5, LSR #16    ; R9   = ((ecc1 >> 16) & 0x00FF) ^ data
        AND    R9,  R9,  #0xFF

        ADD    R9,  R3,  R9, LSL #3     ; R9   = p_gen_tbl + (R9 * 8)
        LDM    R9, {R7-R8}

        MOV    R9,  R4,  LSR #24        ; R9   =  (ecc0 >> 24)
        ORR    R5,  R9,  R5, LSL #8     ; R5   =  (ecc0 >> 24) | (ecc1 << 8)
        EOR    R5,  R8,  R5             ; ecc1 = ((ecc0 >> 24) | (ecc1 << 8)) ^ p_gen_tbl_entry[1]

        EOR    R4,  R7,  R4, LSL #8     ; ecc0 =                 (ecc0 << 8)  ^ p_gen_tbl_entry[0]

        SUBS   R6,  R6, #1

        BNE    BCH_4Bit_CalcEx_Loop1


        MOV    R6, #7
BCH_4Bit_CalcEx_Loop2:
        LDRB   R7, [R1], #+1            ; data = *p_buf_08++

        EOR    R9,  R7,  R5, LSR #16    ; R9   = ((ecc1 >> 16) & 0x00FF) ^ data
        AND    R9,  R9,  #0xFF

        ADD    R9,  R3,  R9, LSL #3     ; R9   = p_gen_tbl + (R9 * 8)
        LDM    R9, {R7-R8}

        MOV    R9,  R4,  LSR #24        ; R9   =  (ecc0 >> 24)
        ORR    R5,  R9,  R5, LSL #8     ; R5   =  (ecc0 >> 24) | (ecc1 << 8)
        EOR    R5,  R8,  R5             ; ecc1 = ((ecc0 >> 24) | (ecc1 << 8)) ^ p_gen_tbl_entry[1]

        EOR    R4,  R7,  R4, LSL #8     ; ecc0 =                 (ecc0 << 8)  ^ p_gen_tbl_entry[0]

        SUBS   R6,  R6, #1

        BNE    BCH_4Bit_CalcEx_Loop2


        STRB   R4, [R2, #+6]
        MOV    R4, R4, LSR #8
        STRB   R4, [R2, #+5]
        MOV    R4, R4, LSR #8
        STRB   R4, [R2, #+4]
        MOV    R4, R4, LSR #8
        STRB   R4, [R2, #+3]

        STRB   R5, [R2, #+2]
        MOV    R5, R5, LSR #8
        STRB   R5, [R2, #+1]
        MOV    R5, R5, LSR #8
        STRB   R5, [R2, #+0]


        LDMIA  SP!, {R0, R3-R9, LR}
        BX     LR

        END
