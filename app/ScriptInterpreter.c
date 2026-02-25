
// Doxygen
/*!
** @file   ScriptInterpreter.c
** @author Coburn
** @date   5/31/2011
**
** @brief Modification of the script base address should be done only in FindScriptAddress().
** All other references to base address are done through this function.
** @ingroup iotasks
**
*/


#include <includes.h>
#include "sys.h"
#include "scripts.h"
#include "ObjDict.h"
#include "sysdep.h"
#include "data.h"
#include "gateway.h"
#include "cpuFlash.h"
#include "ScriptInterpreter.h"
#include "RMBootloader.h"
#include <math.h>
#include <stdio.h>

/******************************************************************************************************
*                                         Defines
*******************************************************************************************************/
#define MAX1            0xFF
#define MAX2            0xFFFF
#define MAX4            0xFFFFFFFF
#define BINARY_POINT    8
#define BYTESIZE        8


#define SCRIPT_STACK_BYTES 200
/******************************************************************************************************
*                                         Local Prototypes
*******************************************************************************************************/



CPU_INT32U IntNegToPos( CPU_INT32U operandVarW, CPU_INT08U signOfType, CPU_BOOLEAN * isNegative);
UNS8 FindScriptPointer( UNS8 scriptID );
double PowerOfTen(int num);
void ResultsVarTypeSaturate( CPU_INT32U * operandVar, CPU_INT08U varType, CPU_BOOLEAN overFlowOpcode, CPU_BOOLEAN saturateIsNeg );
CPU_BOOLEAN getOperand(CPU_INT08U opScopeType, CPU_INT08U* pSourceVar, CPU_INT32U* pOpVar, CPU_INT08S* pOpSignedType, CPU_INT08U* pOpVarSize, CPU_INT08U* pOpPointerType);
//CPU_BOOLEAN getResultOperandSize(CPU_INT08U opScopeType, CPU_INT08U* pOpVarSize);
void setOperand(CPU_INT08U opScopeType, CPU_INT08U* pResultVar, CPU_INT32U opVar, CPU_INT08U len);
CPU_INT08U getNetworkOperand(CPU_INT08U opScopeType, CPU_INT08U *netAddress, CPU_INT32U *pOpVar, CPU_INT08S *pOpSignedType, CPU_INT08U *pOpVarSize , CPU_INT08U *pOpPointerType);
CPU_INT08U setNetworkOperand(CPU_INT08U opScopeType, CPU_INT08U *netAddress, CPU_INT32U opVar, CPU_INT08U opVarSize, CPU_BOOLEAN isPointer );
CPU_INT08U WriteNMTCmd( CPU_INT32U node, CPU_INT32U command, CPU_INT32U param1, CPU_INT32U param2 );
double getElementAsDouble(CPU_INT08S opSignedType, CPU_INT32U opVar, CPU_INT16U element);  
CPU_INT32U getElementAsUint32(CPU_INT08S opSignedType, CPU_INT32U opVar, CPU_INT16U element, CPU_BOOLEAN* isNeg);
/*******************************************************************************************************
*                                         Globals
********************************************************************************************************/
CPU_INT08U currentScriptDebug = 0;
//0 null
//1 bool
//2 uint8
//3 uint16
//4 uint32
//5 int8
//6 int16
//7 int32
//8 str
//9 unused
//A bytearray
//B fixedpoint

#define OPCODE_NOP      0      //
#define OPCODE_MOV      1      //

#define OPCODE_NMT0     2      //
#define OPCODE_NMT1     3      //
#define OPCODE_NMT2     4      //

#define OPCODE_CATMOV   5      //
#define OPCODE_CATMOV0  80      //
#define OPCODE_CATMOVCR 81      //

#define OPCODE_ITS       6      //
#define OPCODE_UTS       7      //
#define OPCODE_ITS0      8      //
#define OPCODE_UTS0      9      //

#define OPCODE_ADD      10      //
#define OPCODE_ADDS     75      //
#define OPCODE_SUB      11      //
#define OPCODE_SUBS     76      //
#define OPCODE_MUL      12      //
#define OPCODE_MULS     77      //
#define OPCODE_DIV      13      //
//NO OPCODE_DIVS
#define OPCODE_DIFF     14      //
//NO OPCODE_DIFFS
#define OPCODE_INC      15      //
#define OPCODE_INCS     78      //
#define OPCODE_DEC      16      //
#define OPCODE_DECS     79      //

#define OPCODE_MAX      17      //
#define OPCODE_MIN      18      //

#define OPCODE_SRGT     19      //
#define OPCODE_SLFT     20      //
#define OPCODE_ABS      21      //

#define OPCODE_BITON    22      //
#define OPCODE_BITOFF   23      //

#define OPCODE_ITQ      24      //
#define OPCODE_UTQ      25      //
#define OPCODE_ADDQ     26      //
#define OPCODE_SUBQ     27      //
#define OPCODE_MULQ     28      //
#define OPCODE_DIVQ     29      //
#define OPCODE_SQRTQ    30      //
#define OPCODE_QTI      31      //
#define OPCODE_QTU      32      //
#define OPCODE_QTS      33      //
//should add INCQ and DECQ

#define OPCODE_CHS      34      //
#define OPCODE_SIL      35      //
#define OPCODE_SIR      36      //

#define OPCODE_AND      37      //
#define OPCODE_OR       38      //
#define OPCODE_MODD     39      //
#define OPCODE_XOR      40      //
#define OPCODE_COMP     41      //

#define OPCODE_SUBSTR   42      //

#define OPCODE_SIN      43
#define OPCODE_COS      44
#define OPCODE_TAN      45
#define OPCODE_ASIN     46
#define OPCODE_ACOS     47      
#define OPCODE_ATAN     48
#define OPCODE_ATAN2    49

#define OPCODE_SIGN     50
//51-59 are available

#define OPCODE_BLT      60      //
#define OPCODE_BGT      61      //
#define OPCODE_BEQ      62      //
#define OPCODE_BNE      63      //
#define OPCODE_BGTE     64      //
#define OPCODE_BLTE     65      //
#define OPCODE_BNZ      66      //
#define OPCODE_BZ       67      //
#define OPCODE_GOTO     68      //
#define OPCODE_BBITON   69      //
#define OPCODE_BBITOFF  72      //

#define OPCODE_TDEL     70      //
#define OPCODE_GNS      71      //

#define OPCODE_BITSET   73      //
#define OPCODE_BITCNT   74      //

//75-79 used for saturate 
//80-81 used for 0 and CR CATMOV
//82-90 available
#define OPCODE_BITCPY     88
#define OPCODE_FILE_CLOSE 89
#define OPCODE_STARTSCPT  90      //
#define OPCODE_STOPSCPT   91      //
#define OPCODE_RUNONCE    92      //
#define OPCODE_RUNIMM     93      //
#define OPCODE_RUNNEXT    94      //
#define OPCODE_RUNMULT    95      //
#define OPCODE_RESETGLOBALS   96      //

#define OPCODE_NODESCAN     97

//98-99 available 

#define OPCODE_INTERPOL     100      //
#define OPCODE_MAVG         101      //
#define OPCODE_IIR          102      //

//103 available
#define OPCODE_FIFO         104

#define OPCODE_VECMOV       105

#define OPCODE_VECMAX       106
#define OPCODE_VECMAXI      107
#define OPCODE_VECMIN       108
#define OPCODE_VECMINI      109
#define OPCODE_VECMED       110
#define OPCODE_VECMEDI      111
#define OPCODE_VECMEAN      112
#define OPCODE_VECSUM       113
#define OPCODE_VECPROD      114
#define OPCODE_VECMAG       115
#define OPCODE_VECMAG2      116

//116- 120 available 
#define OPCODE_VECADD       121
#define OPCODE_VECSUB       122
#define OPCODE_VECMUL       123
#define OPCODE_VECDIV       124

#define OPCODE_VECDOT       125



#define OPCODE_EXIT     255      //


CPU_INT08U sizeOfVarTypes[12] = { 0, 1, 1, 2, 4, 1, 2, 4, 0, 1, 0, 0};
CPU_INT08S varSignedTypes[12] = { 0, 1, 1, 2, 4, -1, -2, -4, 0, 1, 0, 8};

/*
*********************************************************************************************************
*                                             Script_Interpreter()
*
* Description : the script interpreter
*
* Argument(s) : Script pointer (note start at 1 based index; ControlWord from OD 0x1F51 ); ControlWord;
*               NextScriptPointer
*
* Return(s)   : none.
*
* Remarks:  The structure of the download file from the assembler tables and returns the download image.
*            BEGIN HEADER
*               byte 0 - 1 => total size of the download image (LB - HB).
*               byte 2 - 3 => pointer to global table (ie relative address from start of download image.) 
*               byte 4 - 5 => pointer to stack table.
*               byte 6 - 7 => pointer to constants table. 
*               byte 8 - 9 =>  pointer to script ID
*             END HEADER
*             BEGIN SCRIPT BODY
*               byte 10    => size of first operation (ie pointer to next operation)
*               byte 11    => first opcode.
*               byte 12    => number of result operands (high nibble), number of source operands (low nibble)
*               byte 13... => operand1
*               ...
*             END SCRIPT BODY
*             GLOBAL VAR INIT
*             STACK VAR INIT
*             CONSTANT VAR
*               last byte  => script ID
*        
*
*********************************************************************************************************
*/
CPU_INT08U RunScriptInterpreter( CPU_INT08U scriptPointer, CPU_INT08U *pChildScriptPointer)
{
  
  
  CPU_INT16U i, j, k, opIndex;// indices for loops
  CPU_INT32U startOfScriptAddress;
  CPU_INT32U constantsVarTableAddress;
  CPU_INT32U stackInitVarTableAddress;
  CPU_INT32U stackVarTableAddress;
  CPU_INT32U globalInitVarTableAddress;
  CPU_INT32U globalVarTableAddress;
  
  CPU_INT08U scriptOpCodeValue;
  CPU_INT32U currentOperationAddress;
  CPU_INT32U nextOperationAddress;
  CPU_INT08U currentOperandSize;
  CPU_INT32U currentOperandAddress;
  CPU_INT32U nextOperandAddress;
  
  CPU_INT32U varAddress; 
  CPU_INT08U operandScopeType; 
  
  CPU_INT16U sizeOfGlobalVarTable;
  CPU_INT16U sizeOfStackVarTable;
  CPU_INT16U sizeOfConstantsVarTable;
  
  CPU_INT08U numberOfOperands;
  CPU_INT08U numberOfResultsOperands;

  CPU_INT08U networkAddress[7];
  
  CPU_INT08U tempResultsStr[SCRIPT_MAX_STRING];
  CPU_INT08U tempResultsPointer;
  CPU_INT08U nextbyte;
    
  CPU_INT08U tempNode[ACTIVE_NODE_COUNT];
  
  CPU_INT08U stackVariables[SCRIPT_STACK_BYTES];
  
  //Source Operands             //init all to 0 for each OPCODE
  CPU_INT32U operandVar[5];
  CPU_INT08U operandVarSize[5];  //in Bytes
  CPU_INT08S operandSignedType[5]; 
  CPU_INT08U operandPointerType[5]; //0=not a pointer, 1=Array or bytearray pointer, 2=string pointer
  
  //Destination Operand         //init all to 0 for each OPCODE
  CPU_INT32U resultOperandVar; 
  CPU_INT08U resultOperandVarSize; //in Bytes.  Size of the result operand.  Size must be >= resultVarSize if pointer type
  CPU_INT08S resultOperandSignedType;
  CPU_INT08U resultOperandPointerType;
  
  //Temporary Result before getting pushed to Destination Operand  //init all to 0 for each OPCODE
  CPU_INT32U resultVar;  
  CPU_INT08U resultVarSize; //in Bytes
    //This is the size of the result calculated from the operands for array operations and may not be the size of the result operand 
  CPU_INT08U resultPointerType;
  
  CPU_INT32S jumpBytes = 0 ;              //init to 0 for each OPCODE

  OS_ERR err;
  volatile OS_TICK intervalTime = 0;

  CPU_BOOLEAN jumpBack;                  //init FALSE for each OPCODE
  CPU_BOOLEAN jumpFlag;                  //init FALSE for each OPCODE
  CPU_BOOLEAN saturateOpcode;            //init FALSE for each OPCODE
  CPU_BOOLEAN overFlowOpcode;            //init FALSE for each OPCODE
  CPU_BOOLEAN saturateIsNeg;             //init FALSE for each OPCODE
  
  CPU_BOOLEAN jumpOperandFlag;           //init FALSE for each OPERAND - the operand is a jump position, but branch may not be taken
  CPU_BOOLEAN networkOperandFlag;        //init FALSE for each OPERAND
  CPU_BOOLEAN multipleSubIndicesFlag;    //init FALSE for each OPERAND
  CPU_BOOLEAN arrayOperandFlag;          //init FALSE for each OPERAND
  CPU_BOOLEAN operandPairNetwork;        //init FALSE for each OPERAND
  CPU_BOOLEAN operandPairArray;          //init FALSE for each OPERAND
  CPU_BOOLEAN isResult;                  //init FALSE for each OPERAND
  
  CPU_BOOLEAN isImmediate;               //init FALSE for each OPERAND PAIR ELEMENT (SUB OPERAND)
  CPU_BOOLEAN networkErrorContinueFlag;  //init FALSE for each OPCODE


  
  ScriptDebug_CurrentExecutionNumber = 0;
  ScriptDebug_currentOpcode = 0;
  ScriptDebug_currentOpAddress = 0;
  ScriptDebug_statusByte = 0;
  
  //JML: should these be initialized on each pass of OPCODE, OPERAND, or OPERAND PAIR loops?
  CPU_INT08U indexOffset = 0;
  CPU_INT08U networkError;
  CPU_INT16U numArrayElements;
  CPU_INT08U bytesPerElement;

  CPU_INT08U tempErr = 0;
  
  if (scriptPointer == 0)
    return SCRIPT_ZERO_POINTER;
  if (scriptPointer > MAX_NUMBER_SCRIPTS)
    return SCRIPT_INVALID_POINTER;
  
  // decode header information - Address in name refers to the absolute address (or direct) (ie 32 bit CPU address to a single byte)
  // Ptr in name refers to the offset value (or indirect) from a start of a table.
  startOfScriptAddress = FindScriptAddress( scriptPointer ); // initialize start of script  
  currentOperationAddress = startOfScriptAddress + 10; // initialize first address
  

  stackInitVarTableAddress = startOfScriptAddress +  *(CPU_INT08U * )(startOfScriptAddress + 4) + *(CPU_INT08U * )(startOfScriptAddress + 5) * 256;
  sizeOfStackVarTable = (*(CPU_INT08U * )(startOfScriptAddress + 6) + *(CPU_INT08U * )(startOfScriptAddress + 7) * 256) \
    - ((*(CPU_INT08U * )(startOfScriptAddress + 4) + *(CPU_INT08U * )(startOfScriptAddress + 5) * 256)); // size of StackVarTable
  globalInitVarTableAddress = startOfScriptAddress +  *(CPU_INT08U * )(startOfScriptAddress + 2) + *(CPU_INT08U * )(startOfScriptAddress + 3) * 256;
  sizeOfGlobalVarTable = (*(CPU_INT08U * )(startOfScriptAddress + 4) + *(CPU_INT08U * )(startOfScriptAddress + 5) * 256) \
    - ((*(CPU_INT08U * )(startOfScriptAddress + 2) + *(CPU_INT08U * )(startOfScriptAddress + 3) * 256)); // size of GlobalVarTable
  
  
  // identify constants table address
  constantsVarTableAddress = startOfScriptAddress +  *(CPU_INT08U * )(startOfScriptAddress + 6) + *(CPU_INT08U * )(startOfScriptAddress + 7) * 256;
  sizeOfConstantsVarTable = (*(CPU_INT08U * )(startOfScriptAddress + 8) + *(CPU_INT08U * )(startOfScriptAddress + 9) * 256) \
    - ((*(CPU_INT08U * )(startOfScriptAddress + 6) + *(CPU_INT08U * )(startOfScriptAddress + 7) * 256)); // size of ConstantsVarTable
  
  // load stack table
  if (sizeOfStackVarTable <= sizeof(stackVariables))
  {
    memcpy(&stackVariables[0], (CPU_INT08U * )stackInitVarTableAddress, sizeOfStackVarTable*sizeof(CPU_INT08U));
  }
  else
  {
    Scripts_Disabled();
    return SCRIPT_ERR_TOO_MANY_STACK_VARIABLES;
  }
  
  stackVarTableAddress  = (CPU_INT32U) &stackVariables[0];
  globalVarTableAddress = (CPU_INT32U) &globalVariables[globalVarOffset[scriptPointer]]; 
  
  ScriptDebug_varTableSize_SG[0] = (CPU_INT08U)(sizeOfStackVarTable & 0xFF);
  ScriptDebug_varTableSize_SG[1] = (CPU_INT08U)(sizeOfStackVarTable >> 8);
  ScriptDebug_varTableSize_SG[2] = (CPU_INT08U)(sizeOfGlobalVarTable & 0xFF);
  ScriptDebug_varTableSize_SG[3] = (CPU_INT08U)(sizeOfGlobalVarTable >> 8);
  ScriptDebug_varTableSize_SG[4] = (CPU_INT08U)(sizeOfConstantsVarTable & 0xFF);
  ScriptDebug_varTableSize_SG[5] = (CPU_INT08U)(sizeOfConstantsVarTable >> 8);
  ScriptDebug_varTableSize_SG[6] = (CPU_INT08U)(constantsVarTableAddress & 0xFF);
  ScriptDebug_varTableSize_SG[7] = (CPU_INT08U)(constantsVarTableAddress >> 8);
  ScriptDebug_varTableSize_SG[8] = (CPU_INT08U)(constantsVarTableAddress >> 16);
  ScriptDebug_varTableSize_SG[9] = (CPU_INT08U)(constantsVarTableAddress >> 24);
                                                                                                       
  // initialize debug table
  if ((ScriptDebug_controlByte & 0x01) == 0x01 )
  {
    ScriptDebug_currentOpcode = 0x00;
    ScriptDebug_currentOpAddress = currentOperationAddress;
    ScriptDebug_currentOpVar0 = 0x00;
    ScriptDebug_currentOpVar1 = 0x00;
    ScriptDebug_currentOpVar2 = 0x00;
    ScriptDebug_currentOpVar3 = 0x00;
    ScriptDebug_currentOpVar4 = 0x00;
    ScriptDebug_currentResultVar = 0x00;
    ScriptDebug_statusByte = 0;
    ScriptDebug_JumpValue = 0;
    
    memset(ScriptDebug_stackVars, 0, sizeof(ScriptDebug_stackVars));
    memset(ScriptDebug_globalVars, 0, sizeof(ScriptDebug_globalVars));
    
   
    // global vars copy - load the first pass of the global vars table
    if ( (globalVarOffset[scriptPointer + 1] - globalVarOffset[scriptPointer]) <= sizeof(ScriptDebug_globalVars))
    {
      for (i = 0; i < globalVarOffset[scriptPointer + 1] - globalVarOffset[scriptPointer]; i++)
        ScriptDebug_globalVars[i] = globalVariables[i + globalVarOffset[scriptPointer]];
    }
    else
    {
      for (i = 0; i < sizeof(ScriptDebug_globalVars); i++)
        ScriptDebug_globalVars[i] = globalVariables[i + globalVarOffset[scriptPointer]];       
    }
    // stack vars copy - from stack init values
    if ( sizeOfStackVarTable <= sizeof(ScriptDebug_stackVars))
    {
      for (i = 0; i < sizeOfStackVarTable; i++)
        ScriptDebug_stackVars[i] = stackVariables[i];
    }
    else 
    {
      for (i = 0; i < sizeof(ScriptDebug_stackVars); i++)
        ScriptDebug_stackVars[i] = stackVariables[i];
    }
  }
  
  // check for turning on timer and turning off scheduler
  if ( (ScriptDebug_controlByte & 0x08) == 0x08)
  {
    ScriptDebug_timerResult = 0;
    intervalTime = GetTimer1Count();
  }
  
// ---------------------------------------------------------------------------//
//            START OPCODE LOOP
// loop through all operations in script until we've reached the end of script
//----------------------------------------------------------------------------//
  while ( TRUE ) // break only if ScripOpCodeValue =0xFF -JML Maybe should break if exceeded possible script length
  {    
    // initialize vars on each pass of interpreter

    scriptOpCodeValue = *(CPU_INT08U * )(currentOperationAddress + 1); 
    
    if(scriptOpCodeValue == 0xFF)
    {
      break;
    }
      
    resultVar = 0;
    resultVarSize = 0;
    resultPointerType = 0;

    networkErrorContinueFlag = FALSE;
      
    indexOffset = 0;

    jumpFlag = FALSE;
    jumpBack = FALSE;
    jumpBytes = 0;
    saturateOpcode = FALSE;
    saturateIsNeg = FALSE;
    overFlowOpcode = FALSE;
    memset( tempResultsStr, '\0', sizeof(tempResultsStr));
    
    memset (operandSignedType, 0, sizeof(operandSignedType));
    resultOperandSignedType = 0;
    memset (operandVarSize, 0, sizeof(operandVarSize));
    resultOperandVarSize = 0;
    memset (operandVar, 0, sizeof(operandVar));
    resultOperandVar = 0;
    memset (operandPointerType, 0, sizeof(operandPointerType));
    resultOperandPointerType = 0;
    
   
    /*************************************************** Debug control ******************************************************/
     //Bit 0 = 1 Run Debug (single step); 0 not in debug (has to be in run mode first to go to the interpreter)
     //Bit 1 = 0 Advance single step; 1 resets and waits for next advance (ie 3 causes it to wait. writing 1 causes it to run.
    
    // check for abort bit
    if ((ScriptDebug_controlByte & 0x80) == 0x80)
    {
      ScriptDebug_controlByte = 0;
      return 25;
    }
    
    if ((ScriptDebug_controlByte & 0x01) == 0x01 && (ScriptDebug_controlByte & 0x02) == 0x02)
    {
      OSTimeDlyHMSM(0, 0, 0, 100,
                  OS_OPT_TIME_HMSM_STRICT,
                  &err);
      continue;    // jumps to end of interpreter if debug on and single step waiting
    }
    
    if ((ScriptDebug_controlByte & 0x04) == 0x04) // reset -> escape from debug mode
    {
      scriptOpCodeValue = 0xFF;
      ScriptDebug_controlByte = 0x00;
      return 0;
    }
    ScriptDebug_JumpValue = 0;  // reset here so PC can read value while on hold.
    /********************************************** End debug control *************************************/
    
    
    //
    
    nextOperationAddress = *(CPU_INT08U * )currentOperationAddress + currentOperationAddress; // find next address ( current address plus value stored at current address)
    numberOfOperands = (*(CPU_INT08U * )(currentOperationAddress + 2)) & 0x0F;          //low nibble
    numberOfResultsOperands = (*(CPU_INT08U * )(currentOperationAddress + 2)) >> 4;     //high nibble
    currentOperandAddress = currentOperationAddress + 3; // first operand


    //-----------------------------------------------------------------------//
    //                 START OPERAND LOOP
    // loop through all the operands for the current operation and copy them 
    // to local variables or save pointers to them
    //-----------------------------------------------------------------------//
    for (opIndex = 0; opIndex < numberOfOperands + numberOfResultsOperands; opIndex++)
    {
      networkOperandFlag = FALSE;
      multipleSubIndicesFlag = FALSE;
      arrayOperandFlag = FALSE;
      jumpOperandFlag = FALSE;
      operandPairNetwork = FALSE;
      operandPairArray = FALSE;
      isResult =FALSE;
      
      memset (networkAddress, 0, sizeof(networkAddress));
      
      if (opIndex  >= numberOfOperands)
        isResult = TRUE;
      
      do //the do...while loop runs twice for operand pairs, otherwise just once
      {	  
        isImmediate =FALSE;
        
        currentOperandSize = *(CPU_INT08U* )currentOperandAddress; 
        operandScopeType   = *(CPU_INT08U* )(currentOperandAddress + 1);
        
        nextOperandAddress = currentOperandSize + currentOperandAddress;
        
        if(operandPairArray) //this will only be true the second time through the do...while loop
        {
          bytesPerElement = sizeOfVarTypes[operandScopeType & 0x0F];
          
          if (arrayOperandFlag) //copy entire array  (only set true second time through do...while)
          {
            indexOffset = 0;
          }
          else //copy element of array
          {
            //the current operand value is the array index.  It will be overwritten by the value at the array index
            if(!isResult)                  
              indexOffset = operandVar[opIndex];
            else
              indexOffset = resultOperandVar;
            
            if(indexOffset >= numArrayElements)
            {
              //error: index out of bounds
              return SCRIPT_ERR_ARRAYINDEX;
            }
          }
          operandPairArray = FALSE; 
        }
        else
        {
          indexOffset = 0;
        }
        
        if(operandScopeType & 0x40) //Network Modifier
        {
          //subindex specified indirectly or multiple sub indices (i.e., networkPairArray == TRUE)
          //first time through do...while loop
          if(operandScopeType & 0x30 || operandScopeType & 0x80) 
          {
            operandPairNetwork = TRUE;
            if(operandScopeType & 0x80)
              multipleSubIndicesFlag = TRUE;
          }
          //subindex has immediate scope, either initially specified as immediate,(i.e., networkPairArray == FALSE)
          //or already retrieved subindex. second time through do...while(i.e., networkPairArray == TRUE)
          else 
          {
            memcpy(networkAddress, (CPU_INT08U* )(currentOperandAddress + 2), 6*sizeof(CPU_INT08U));
            networkAddress[6] = 0;
            
            if(operandPairNetwork)
            {
              //switch subindex with previous operand in operand pair
              if(operandVar[opIndex] > 0xFF) //invalid subindex
              {
                //error: subindex must be <256
                return SCRIPT_ERR_ODSUBINDEX;
              }
              else 
              {
                if(multipleSubIndicesFlag)
                {
                  //array and network modifier: reading/writing multiple subindices
                  networkAddress[6] = networkAddress[5]; //set number of subindices to read/write	
                }
                if(!isResult)                  
                  networkAddress[5] = (CPU_INT08U) operandVar[opIndex]; 
                else
                  networkAddress[5] = (CPU_INT08U) resultOperandVar; 
                //NOTE: operandVar[opIndex], resultOperandVar currently contains the subindex, but will be replaced 
                //by the data or a pointer to the data stored at that subindex after call to GetNetworkData
              }
              
              operandPairNetwork = FALSE;
              
            }
            networkOperandFlag = TRUE;				              
            
            if(!isResult)
            {
              //GET NETWORK DATA: JML at this point we don't know the size of the result that we're pushing the data into.  
              
              networkError = getNetworkOperand(operandScopeType, networkAddress, &operandVar[opIndex], \
                                            &operandSignedType[opIndex], &operandVarSize[opIndex], &operandPointerType[opIndex]);
              
                
                if (networkError && !(Control_SystemControl & 0x80)) 
                {
                  // error: Could not get data from Network 
                  return SCRIPT_ERR_GETNETWORKDATA; 
                }
                else
                {
                  if(networkError)
                  {
                    RADIO_SDO_Script_Failures++;  // increment error
                  
                    networkErrorContinueFlag = TRUE;
                    break; //exit do...while
                  }
                }
            }
            else
            {
              //set the expected size to the scalar datatype specified.  If the address contains an array, 
              //the actual size will be a multiple of resultOperandVarSize
              resultOperandVarSize = sizeOfVarTypes[operandScopeType & 0x0F];
            }
            currentOperandAddress = nextOperandAddress;
            
            break; //exit do...while
          }
        }
        else if(operandScopeType & 0x80) //Array Modifier
        {
          numArrayElements = (*(CPU_INT08U* )(nextOperandAddress + 4)) + (*(CPU_INT08U* )(nextOperandAddress + 5) << 8);
          operandPairArray = TRUE;
        }
        
        switch ((operandScopeType >> 4) & 0x03)
        {
          //Get address to first byte of variable table depending on variable scope
        case 0: //immediate (literal)
          {
            if(isResult && currentOperandSize == 6 && operandScopeType == 0x06)
            {
              jumpOperandFlag = TRUE;
            }
            else if(isResult && !operandPairArray && !operandPairNetwork )
            {
              //error: cannot have an immediate scope result.  An array index or OD subindex
              //in a result can be specified as an immediate.  Branch statements (operandsize == 6)
              //also use a immediate result.
              return SCRIPT_ERR_RESULT_IS_IMMEDIATE;
            }
            //point to variable contained within operand (in Flash)
            varAddress = currentOperandAddress + 2;
            isImmediate = TRUE;
            break;
          }
        case 1: //constant
          {
            if(isResult && !operandPairArray && !operandPairNetwork)
            {
              //error: cannot have a constant scope result
              return SCRIPT_ERR_RESULT_IS_CONSTANT;
            }
            //point to variable stored in the Constants Variable Table at the end of the script (in Flash)
            varAddress = constantsVarTableAddress;
            break;
          }
        case 2: //stack
          {
            //point to variable stored in Stack Variables (in stack RAM, reused by different scripts)
            varAddress = stackVarTableAddress;
            break;
          }
        case 3: //global
          {
            //point to variable stored in Global Variables for this script (in global RAM)
            varAddress = globalVarTableAddress;
            break;
          }
        default:
          {
            //error in Scope switch
            break;
          }
        }
        
        if (jumpOperandFlag)
          break; //break from operand do...while
        
        //Adjust address by pointer into table and any offsets for array indices
        if(!isImmediate)
        {
          varAddress +=(*(CPU_INT08U * )(currentOperandAddress + 2)      ) + \
                       (*(CPU_INT08U * )(currentOperandAddress + 3) <<  8) + \
                        indexOffset*bytesPerElement;
          //an immediate type can be an array, but not an element of an array, so indexOffset is always zero
        }
                
        if(!isResult) //copy data or pointer from operand to local variable to use in interpreter
        {		
          if(arrayOperandFlag)
          {
            //Copy entire array: in this case we only need pointer to first element.  All variable types
            //are equivalent
            operandVar[opIndex] = varAddress;
            operandVarSize[opIndex] = numArrayElements*bytesPerElement; //size in bytes (not elements)
            operandSignedType[opIndex] =  varSignedTypes[operandScopeType & 0x0F];
            //JML TODO: check for non-numeric types (e.g. no arrays of strings)
          }
          else
          {	
            arrayOperandFlag = getOperand(operandScopeType, (CPU_INT08U*) varAddress, &operandVar[opIndex], \
              &operandSignedType[opIndex], &operandVarSize[opIndex], &operandPointerType[opIndex]);
          }
        }
        else 
        {
          if(arrayOperandFlag)
          {
            resultOperandVarSize = numArrayElements*bytesPerElement; //size in bytes
            resultOperandSignedType =  varSignedTypes[operandScopeType & 0x0F];
          }
          else
          {
            arrayOperandFlag = getOperand(operandScopeType, (CPU_INT08U*) varAddress, &resultOperandVar, \
              &resultOperandSignedType, &resultOperandVarSize, &resultOperandPointerType);
          }
        }
        
        currentOperandAddress = nextOperandAddress;
        
      } while(operandPairArray || operandPairNetwork); //End of do...while for operand pairs
      
      if(networkErrorContinueFlag) 
        break; //break from operand loop
        
    } 
    //-----------------------------------------------------------------------//
    //                END OPERAND LOOP
    //-----------------------------------------------------------------------//
        
    // check for abort bit
    if ((ScriptDebug_controlByte & 0x80) == 0x80)
    {
      ScriptDebug_controlByte = 0;
      return 25;
    }
    
    if(networkErrorContinueFlag){
        currentOperationAddress = nextOperationAddress; 
        continue; //jump to next OpCode
    }
    
//----------------------------------------------------------------------------// 
//                           START OPCODE INTERPRETER 
//----------------------------------------------------------------------------//
    switch (scriptOpCodeValue)
    {
    /* No Operation */
    case OPCODE_NOP:
      {
        
        break;
        
      } 
     /*Move */
    case OPCODE_MOV:
      {
        resultVar = operandVar[0];
        resultVarSize = operandVarSize[0];
        resultPointerType = operandPointerType[0]; 
        break;
        
      }
      /* NMT command - 0 parameters elements */  
    case OPCODE_NMT0:
      {
        WriteNMTCmd(operandVar[0], operandVar[1], 0, 0);
        break;
      }
      /* NMT command - 1 parameters elements */ 
    case OPCODE_NMT1:
      {
        WriteNMTCmd(operandVar[0], operandVar[1], operandVar[2], 0);
        break;
      }
      /* NMT command - 2 parameters elements */ 
    case OPCODE_NMT2:
      {
        WriteNMTCmd(operandVar[0], operandVar[1], operandVar[2], operandVar[3]);        
        break; 
      } 
      /* string concatenation */
    case OPCODE_CATMOV: 
    case OPCODE_CATMOV0: // with null termination
    case OPCODE_CATMOVCR: // with CR-LF
      {     
        tempResultsPointer = 0;

        for (k = 0; k < numberOfOperands; k++)
        {
            // copy string characters to tempResultsStr
            for (j = 0; j < operandVarSize[k]; j++)
            {
              if ( operandPointerType[k] ) 
              {
                nextbyte = * (CPU_INT08U *)(operandVar[k] + j);
                
                //if a string, break from for loop if null charcter found
                if(nextbyte == 0 && operandPointerType[k]==2 ) 
                  break; 
                tempResultsStr[j + tempResultsPointer] = nextbyte; 
              }
              else
              {
                tempResultsStr[j + tempResultsPointer] = (CPU_INT08U)(operandVar[k]>>(j*8)); 
              }
            }                    
            tempResultsPointer += j;
        }
        

        if(scriptOpCodeValue == OPCODE_CATMOV0) // CATMOV with null termination
        {
          //add null termination
          tempResultsStr[tempResultsPointer++] = 0x00;
        }
        else if(scriptOpCodeValue == OPCODE_CATMOVCR) // CATMOV with CR-LF
        {
          //add CR-LF termination
          tempResultsStr[tempResultsPointer++] = 0x0D;
          tempResultsStr[tempResultsPointer++] = 0x0A;
        }
        
        resultVar = (CPU_INT32U)(&tempResultsStr[0]);
        resultVarSize = tempResultsPointer;
        resultPointerType = 2;
        break; 
      }
      
      case OPCODE_SUBSTR: // substring
        {
          tempResultsPointer = 0;
          CPU_INT16U j = 0;
          
          // operand0 must be a pointer (ether array or string), and operand1 and operand2 must be non-pointer type
          if ( operandPointerType[0] == 0 || operandPointerType[1] != 0 || operandPointerType[2] != 0) 
            return SCRIPT_ERR_OPERAND_TYPE;
          
          // copy string characters to working buffer
          for (j = operandVar[1]; j < operandVar[1] + operandVar[2]; j++)
          {            
              tempResultsStr[j - operandVar[1]] = * (CPU_INT08U *)(operandVar[0] + j); // copy the values - size header byte is not present          
          }  
          
          tempResultsPointer = operandVar[2]; 
          
          //add null termination
          tempResultsStr[tempResultsPointer++] = 0x00; 
          resultVar = (CPU_INT32U)(&tempResultsStr[0]);
          resultVarSize = tempResultsPointer;
          resultPointerType = 2;
          break; 
        } 
    // sprintf returns the length of the string written (not including the null terminal).
    //  C uses an asterisk in the position of the field width specifier to indicate to sprintf that 
    //  it will find the variable that contains the value of the field width as the first parameter.
      
    case OPCODE_ITS: // Int to String 
    case OPCODE_ITS0: // Int to String with null termination
      {       
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_INT32U  tempOp0 = 0;
        
        if ( operandPointerType[0] ) // operand is a pointer
            return SCRIPT_ERR_OPERAND_TYPE;

        
        // give a positve value back
        if (operandSignedType[0] > 0)
          tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);   
        else
          tempOp0 = operandVar[0];
        
        // printf returns the sting length plus the null terminator
        if (isNeg0)
        {
          tempResultsPointer = sprintf((CPU_CHAR *)tempResultsStr, "%*d", operandVar[1], (CPU_INT32S)(tempOp0 * -1)); 
        }
        else
        {
          tempResultsPointer = sprintf((CPU_CHAR *)tempResultsStr, "%*d", operandVar[1], operandVar[0]);        
        }
        
        if(scriptOpCodeValue == OPCODE_ITS0) //Int to String (ITS0)
        {
          //add null termination
          tempResultsStr[tempResultsPointer++] = 0x00;
        }
        resultVar = (CPU_INT32U)(&tempResultsStr[0]); 
        resultVarSize = tempResultsPointer;
        resultPointerType = 2;
        
        if ( resultVarSize > SCRIPT_MAX_STRING )
        {
          return SCRIPT_ERR_MAX_STRING;
        }
        
        break;
      }
      
    case OPCODE_UTS: // Uint to String 
    case OPCODE_UTS0: // Uint to String with null termination
      {        
         if ( operandPointerType[0] ) // operand is a pointer
            return SCRIPT_ERR_OPERAND_TYPE;
            
        tempResultsPointer = sprintf((CPU_CHAR *)tempResultsStr, "%*u", operandVar[1], operandVar[0]); 
        
        if(scriptOpCodeValue == OPCODE_UTS0) //Uint to String (UTS0)
        {
          //add null termination
          tempResultsStr[tempResultsPointer++] = 0x00;
        }
        resultVar = (CPU_INT32U)(&tempResultsStr[0]); 
        resultVarSize = tempResultsPointer;
        resultPointerType = 2;
        
        
        if ( resultVarSize > SCRIPT_MAX_STRING )
        {
          return SCRIPT_ERR_MAX_STRING;
        }
        
        break;
      }
    
    case OPCODE_ADD: // add
    case OPCODE_ADDS: // add saturate
      {   
        CPU_INT32U tempOp;        
        CPU_BOOLEAN isNeg;
        CPU_INT08U idx = 0;
        CPU_INT64U posSum = 0;
        CPU_INT64U negSum = 0;
        
        //sum of 5 UINT32 can exceed UINT32 but cannot exceed UINT64, so saturate can be completed after total sum
               
        for (idx = 0; idx < numberOfOperands; idx++)
        { 
          if ( operandPointerType[idx] ) // operand is a pointer
            return SCRIPT_ERR_OPERAND_TYPE;
            
          // normalize to 32 bit representation (positive)
          tempOp = IntNegToPos(operandVar[idx], operandSignedType[idx], &isNeg); 
          
          if (isNeg)
          {
            negSum += (CPU_INT64U)tempOp;
          }
          else
            posSum += (CPU_INT64U)tempOp;     
        }
        
        if(posSum >= negSum)
        {
            posSum -= negSum;
            negSum = 0;
        }
        else
        {
          negSum -= posSum;
          posSum = 0;
          saturateIsNeg = TRUE;
        }
          
        if(scriptOpCodeValue == OPCODE_ADDS) //saturate
        {
          if(posSum > MAX4) posSum = MAX4; //prevent overflow
          if(negSum > MAX4) negSum = MAX4;     
          saturateOpcode = TRUE;
        }

        if(negSum)
          resultVar = ~(CPU_INT32U)negSum + 1;
        else
          resultVar = (CPU_INT32U)posSum;
        break;
      }
    case OPCODE_SUB: // subtract
    case OPCODE_SUBS: // subtract saturate
      {
        CPU_INT32U tempOp;
        CPU_BOOLEAN isNeg;
        CPU_INT64U posSum = 0;
        CPU_INT64U negSum = 0;
        
        //sum of 2 UINT32 can exceed UINT32 but cannot exceed UINT64, so saturate can be completed after total sum
        
        if ( operandPointerType[0] || operandPointerType[1]) // operand is a pointer
            return SCRIPT_ERR_OPERAND_TYPE;
            
        // normalize to 32 bit representation (positive)
        tempOp = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg);
        if (isNeg)
          negSum += (CPU_INT64U)tempOp;
        else
          posSum += (CPU_INT64U)tempOp; 
        
        tempOp = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg);
        if (isNeg)
          posSum += (CPU_INT64U)tempOp; //subtract negative becomes positive
        else
          negSum += (CPU_INT64U)tempOp;        
       
         if(posSum >= negSum)
        {
            posSum -= negSum;
            negSum = 0;
        }
        else
        {
          negSum -= posSum;
          posSum = 0;
          saturateIsNeg = TRUE;
        }
          
        if(scriptOpCodeValue == OPCODE_SUBS) //saturate
        {
          if(posSum > MAX4) posSum = MAX4; //prevent overflow
          if(negSum > MAX4) negSum = MAX4;     
          saturateOpcode = TRUE;
        }

        if(negSum)
          resultVar = ~(CPU_INT32U)negSum + 1;
        else
          resultVar = (CPU_INT32U)posSum;
          
        break;
      }
    case OPCODE_MUL: // multiply
    case OPCODE_MULS: // multiply saturate
      {
        CPU_INT32U tempOp;        
        CPU_BOOLEAN isNeg;
        CPU_INT08U idx = 0;
        CPU_INT64U prod = 1;
        
        //multiply of 5 UINT32 can exceed UINT64, so saturate must be implemented at each multiply
       
        for (idx = 0; idx < numberOfOperands; idx++)
        { 
          if ( operandPointerType[idx] ) // operand is a pointer
            return SCRIPT_ERR_OPERAND_TYPE;
            
          // normalize to 32 bit representation (positive)
          tempOp = IntNegToPos(operandVar[idx], operandSignedType[idx], &isNeg); 
          
          saturateIsNeg ^= isNeg;
          prod *= (CPU_INT64U)tempOp;
          
          if(scriptOpCodeValue == OPCODE_MULS) 
            if(prod > MAX4) prod = MAX4;  //prevent overflow
        }
        
        if(saturateIsNeg)
          resultVar = ~(CPU_INT32U)prod + 1;
        else
          resultVar = (CPU_INT32U)prod;
                   
        if(scriptOpCodeValue == OPCODE_MULS)
        {
          saturateOpcode = TRUE;
        }
        
        break;
      }
    case OPCODE_DIV: // divide
      {
        CPU_INT32U tempOp0;
        CPU_INT32U tempOp1;
        CPU_BOOLEAN isNeg0;
        CPU_BOOLEAN isNeg1;
        
        //divide of 2 UINT32 will not exceed UINT32
        if ( operandPointerType[0] || operandPointerType[1]) // operand is a pointer
            return SCRIPT_ERR_OPERAND_TYPE;
            
        if (operandVar[1] == 0)
        {
          return SCRIPT_ERR_DIVIDEBYZERO;
        }
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
               
        if ( isNeg0 ^ isNeg1 )
        {
          resultVar = ~(tempOp0 / tempOp1) + 1 ;
          saturateIsNeg = TRUE;
        }
        else
        {
          resultVar = tempOp0 / tempOp1;         
        }
        break;
      }
    case OPCODE_DIFF: // DIFF
      {
        CPU_INT32U tempOp0;
        CPU_INT32U tempOp1;
        CPU_BOOLEAN isNeg0;
        CPU_BOOLEAN isNeg1;
        
        //diff of 2 UINT32 will not exceed UINT32
        
        if ( operandPointerType[0] || operandPointerType[1]) // operand is a pointer
            return SCRIPT_ERR_OPERAND_TYPE;
            
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if ( isNeg0 ^ isNeg1 )
        {
          resultVar = tempOp0 + tempOp1;
        }
        else 
        {
          resultVar = abs(tempOp0 - tempOp1);         
        }
        break;
      }
    case OPCODE_INC: // Increment
      if ( operandPointerType[0]) // operand is a pointer
           return SCRIPT_ERR_OPERAND_TYPE;
      
      resultVar = operandVar[0] + 1; 
      break;
    case OPCODE_INCS: // Increment saturate
      {
        CPU_INT32U tempOp;
        CPU_BOOLEAN isNeg;
        
        if ( operandPointerType[0]) // operand is a pointer
           return SCRIPT_ERR_OPERAND_TYPE;
        
        tempOp = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg);

        if(isNeg)
        {
          saturateIsNeg = TRUE;
          resultVar = ~tempOp + 2;
        }
        else
        {
          if(tempOp == MAX4)
            resultVar = tempOp; //don't allow overflow
          else
            resultVar = tempOp + 1;
        }
          
        saturateOpcode = TRUE;
          
        break;
      }
    case OPCODE_DEC: // Decrement
      if ( operandPointerType[0]) // operand is a pointer
          return SCRIPT_ERR_OPERAND_TYPE;
            
        resultVar = operandVar[0] - 1;   
        break;
    case OPCODE_DECS: // Decrement saturate
      {
        CPU_INT32U tempOp;
        CPU_BOOLEAN isNeg;
        
        if ( operandPointerType[0]) // operand is a pointer
           return SCRIPT_ERR_OPERAND_TYPE;
        
        tempOp = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg);
        
        if(isNeg)
        {
          saturateIsNeg = TRUE;
          if(tempOp == 0x80000000)
            resultVar = 0x80000000;
          else
             resultVar = ~tempOp;
        }
        else
        {
          if(tempOp == 0)
          {
            if(operandSignedType[0] < 0 ) 
              resultVar = 0;
            else 
            {
              resultVar = MAX4;
              saturateIsNeg = TRUE;
            }
          }
          else
            resultVar = tempOp - 1;
        }
        saturateOpcode = TRUE;
        break;
      }
    case OPCODE_MAX: // Maximum
      {        
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if ( isNeg0 && isNeg1 ) // reverse logic for negatives
        {
          if (tempOp0 > tempOp1)
            resultVar = operandVar[1];
          else
            resultVar = operandVar[0];
        }
        else if ( isNeg0 && !isNeg1 ) // op 0 is negative
          resultVar = operandVar[1];
        else if ( !isNeg0 && isNeg1 )
          resultVar = operandVar[0];
        else if ( tempOp0 > tempOp1 ) // only both positive left
          resultVar = operandVar[0];
        else
          resultVar = operandVar[1];
        
        break;
      }
    case OPCODE_MIN: // Minimum
      {
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if ( isNeg0 && isNeg1 ) // reverse logic for negatives
        {
          if (tempOp0 < tempOp1)
            resultVar = operandVar[1];
          else
            resultVar = operandVar[0];
        }
        else if ( isNeg0 && !isNeg1 ) // op 0 is negative
          resultVar = operandVar[0];
        else if ( !isNeg0 && isNeg1 )
          resultVar = operandVar[1];
        else if ( tempOp0 > tempOp1 ) // only both positive left
          resultVar = operandVar[1];
        else
          resultVar = operandVar[0];
        break;
      }
    case OPCODE_SRGT: // Shift right ignoring sign
      {
        resultVar = operandVar[0] >> operandVar[1];       
        break;
      }
    case OPCODE_SLFT: // shift left ignoring sign
      {
        resultVar = operandVar[0] << operandVar[1];
        break;
      }
    case OPCODE_ABS: // Absolute Value
      {
        CPU_BOOLEAN isNeg0 = FALSE;
        // give a positve value back
        if (operandSignedType[0] > 0)
          resultVar = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);   
        else
          resultVar = operandVar[0];
        break;
      }
    case OPCODE_BITON:
      {
        operandVar[0] |= 1 << operandVar[1];
        resultVar = operandVar[0];
        break;
      }
    case OPCODE_BITOFF:
      {
        operandVar[0] &= ~(1 << operandVar[1]);
        resultVar = operandVar[0];
        break;
      }
    case OPCODE_ITQ:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        if (isNeg0) // is negative
        {
          // normalize to 32 bit, then shift.
          resultVar = (CPU_INT32U)((~tempOp0 + 1 ) << BINARY_POINT);
        }
        else
        {
          resultVar = operandVar[0] << BINARY_POINT; 
        }
       
        break;
      }
    case OPCODE_UTQ:
      {
        resultVar = operandVar[0] << BINARY_POINT;
        break;
      }
    case OPCODE_ADDQ:
      {
        // normalize to FP
        if (operandSignedType[0] != 8 ) 
        {
          operandVar[0] = operandVar[0] << BINARY_POINT;
        }
        if (operandSignedType[1] != 8 ) 
        {
          operandVar[1] = operandVar[1] << BINARY_POINT;
        }
        // calculate results in FP
        resultVar = operandVar[0] + operandVar[1];
        
        // check for range limits for 24bit Q.
        if ( (CPU_INT32S)resultVar < -8388608 )
          resultVar = 0xFFFFFF;
        else if ((CPU_INT32S)resultVar > 8388607)
          resultVar =  8388607;
        break;
      }
    case OPCODE_SUBQ:
      {
        // normalize to FP
        if (operandSignedType[0] != 8 ) 
        {
          operandVar[0] = operandVar[0] << BINARY_POINT;
        }
        if (operandSignedType[1] != 8 ) 
        {
          operandVar[1] = operandVar[1] << BINARY_POINT;
        }
        // calculate results in FP
        resultVar = operandVar[0] - operandVar[1];
        
        // check for range limits for 24bit Q.
        if ( (CPU_INT32S)resultVar < -8388608 )
          resultVar = 0xFFFFFF;
        else if ((CPU_INT32S)resultVar > 8388607)
          resultVar =  8388607;
        break;
      }
    case OPCODE_MULQ:
      {
        // normalize to FP
        if (operandSignedType[0] != 8 ) 
        {
          operandVar[0] = operandVar[0] << BINARY_POINT;
        }
        if (operandSignedType[1] != 8 ) 
        {
          operandVar[1] = operandVar[1] << BINARY_POINT;
        }
        // calculate results in FP
        resultVar = (operandVar[0] >> BINARY_POINT) * operandVar[1];         
        
        // check for range limits for 24bit Q.
        if ( (CPU_INT32S)resultVar < -8388608 )
          resultVar = 0xFFFFFF;
        else if ((CPU_INT32S)resultVar > 8388607)
          resultVar =  8388607;
        
        break;
      }
    case OPCODE_DIVQ:
      {
        // normalize to FP
        if (operandSignedType[0] != 8 ) 
        {
          operandVar[0] = operandVar[0] << BINARY_POINT;
        }
        if (operandSignedType[1] != 8 ) 
        {
          operandVar[1] = operandVar[1] << BINARY_POINT;
        }
        // calculate results in FP
        resultVar = (operandVar[0] << BINARY_POINT) / operandVar[1];    
        
        // check for range limits for 24bit Q.
        if ( (CPU_INT32S)resultVar < -8388608 )
          resultVar = 0xFFFFFF;
        else if ((CPU_INT32S)resultVar > 8388607)
          resultVar =  8388607;
           
        break;
      }
    case OPCODE_SQRTQ:
      {
        double rst = 0.0;    
        CPU_INT08S i, max = 8;	// to define maximum digit         
	double z = 0;
        double ji = 1.0;
        
        if (operandSignedType[0] == 8)
        {
          z = (double)(operandVar[0] / 256.0);
        }
        else
        {
          z = (double)(operandVar[0]);
        }
	           
	    for(i = max ; i > 0 ; i--)
            {
		        // value must be bigger then 0
              if(z - (( 2 * rst ) + ( ji * PowerOfTen(i)))*( ji * PowerOfTen(i)) >= 0)
              {
                while( z - (( 2 * rst ) + ( ji * PowerOfTen(i)))*( ji * PowerOfTen(i)) >= 0)
                {
                    ji++;
                    if(ji >= 10) 
                      break;				
                }
                ji--; //correct the extra value by minus one to j
                z -= (( 2 * rst ) + ( ji * PowerOfTen(i)))*( ji * PowerOfTen(i)); //find value of z

		rst += ji * PowerOfTen(i);	// find sum of a
			
                j = 1.0;
              }		

	    }

	    for(i = 0 ; i >= 0 - max ; i--)
            {
              if(z - (( 2 * rst ) + ( ji * PowerOfTen(i)))*( ji * PowerOfTen(i)) >= 0)
              {
                while( z - (( 2 * rst ) + ( ji * PowerOfTen(i)))*( ji * PowerOfTen(i)) >= 0)
                {
                    ji++;
                    if(ji >= 10) 
                      break;				
                }
                ji--;
                z -= (( 2 * rst ) + ( ji * PowerOfTen(i)))*( ji * PowerOfTen(i)); //find value of z

                rst += ji * PowerOfTen(i);	// find sum of a			
                ji = 1.0;
              }
            }
	        // find the number on each digit
	       
        
        
        resultVar = (CPU_INT32U)(rst * 256);
        break;
      }
    case OPCODE_QTI:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        if (isNeg0) // is negative
        {
          // normalize to 32 bit, then shift.
          resultVar = (CPU_INT32U)((~tempOp0 + 1 ) >> BINARY_POINT);
        }
        else
        {
          resultVar = operandVar[0] >> BINARY_POINT; 
        }
        break;
      }
    case OPCODE_QTU:
      {
        resultVar = operandVar[0] >> BINARY_POINT;
        break;
      }
    case OPCODE_QTS:
      {
        if (operandSignedType[0] == 8)
        {
          float tempFloat = (CPU_INT16U)(operandVar[0]) / 256.0;
          resultVarSize = sprintf((CPU_CHAR *)tempResultsStr, "%*f", operandVar[1], tempFloat) + 1;
          resultVar = (CPU_INT32U)(&tempResultsStr[0]);
          resultPointerType = 2;
        }        
        break;
      }
    case OPCODE_CHS: //change sign - only valid with signed int
      {
        CPU_INT32U tempOp0 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        if (operandSignedType[0] > 0 )
        {
          tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
          if (isNeg0)
            resultVar = tempOp0;
          else
            resultVar = (CPU_INT32U)(~tempOp0 + 1 );
        }
        else
          resultVar = operandVar[0];
        break;
      }
    case OPCODE_SIL:
      {       
        if (operandSignedType[0] == 1 && (operandVar[0] & 0x80 == 0x80))
        {
            resultVar = operandVar[0] << operandVar[1];
            resultVar |= (1 << 7);
        }
        else if (operandSignedType[0] == 2 && (operandVar[0] & 0x8000 == 0x8000))
        {
            resultVar = operandVar[0] << operandVar[1];
            resultVar |= (1 << 15);
        }
        else if (operandSignedType[0] == 4 && (operandVar[0] & 0x80000000 == 0x80000000))
        {
            resultVar = operandVar[0] << operandVar[1];
            resultVar |= (1 << 31);
        }
        else
          resultVar = operandVar[0] << operandVar[1];
        break;
      }
    case OPCODE_SIR:
      {
        if (operandSignedType[0] == 1 && (operandVar[0] & 0x80 == 0x80))
        {
            resultVar = operandVar[0] >> operandVar[1];
            resultVar |= (1 << 7);
        }
        else if (operandSignedType[0] == 2 && (operandVar[0] & 0x8000 == 0x8000))
        {
            resultVar = operandVar[0] >> operandVar[1];
            resultVar |= (1 << 15);
        }
        else if (operandSignedType[0] == 4 && (operandVar[0] & 0x80000000 == 0x80000000))
        {
            resultVar = operandVar[0] >> operandVar[1];
            resultVar |= (1 << 31);
        }
        else
          resultVar = operandVar[0] >> operandVar[1];
        break;
      }
    case OPCODE_AND:
      {
        resultVar = operandVar[0] & operandVar[1]; 
        break;
      }
    case OPCODE_OR:
      {
        resultVar = operandVar[0] | operandVar[1];
        break;
      }
    case OPCODE_MODD:
      {
        resultVar = operandVar[0] % operandVar[1];  
        break;
      }
    case OPCODE_XOR:
      {
        resultVar = operandVar[0] ^ operandVar[1];
        break;
      }
    case OPCODE_COMP:
      {
        resultVar = ~operandVar[0];
        break;
      }
    case OPCODE_BLT:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        ScriptDebug_JumpValue = 1;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if (operandSignedType[0] == 8 ^ operandSignedType[1] == 8)
        {
          if (operandSignedType[0] == 8) // first is FP so convert the second
          {
            tempOp0 = tempOp0 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 > tempOp1)
              {
                jumpFlag = TRUE;             
              }
              else
              {
                jumpFlag = FALSE;
              }
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
            {
              jumpFlag = TRUE;
              
            }
            else if ( !isNeg0 && isNeg1 )
            {
              jumpFlag = FALSE;
            }
            else if ( tempOp0 < tempOp1 ) // only both positive left
            {
              jumpFlag = TRUE;
              
            }
            else
            {
              jumpFlag = FALSE;
            }
          }
          else // second is FP, so convert the first
          {
            tempOp1 = tempOp1 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 > tempOp1)
                jumpFlag = TRUE;
              else
              {
                jumpFlag = FALSE;               
              }
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
            {
              jumpFlag = TRUE;
            }
            else if ( !isNeg0 && isNeg1 )
              jumpFlag = FALSE;
            else if ( tempOp0 < tempOp1 ) // only both positive left
            {
              jumpFlag = TRUE;
            }
            else
              jumpFlag = FALSE;
          }
        }
        else // either both FP or both not FP
        {
          if ( isNeg0 && isNeg1 ) // reverse logic for negatives
          {
            if (tempOp0 > tempOp1)
              jumpFlag = TRUE;
            else
            {
              jumpFlag = FALSE;
            }
          }
          else if ( isNeg0 && !isNeg1 ) // op 0 is negative
          {
            jumpFlag = TRUE;
          }
          else if ( !isNeg0 && isNeg1 )
            jumpFlag = FALSE;
          else if ( tempOp0 < tempOp1 ) // only both positive left
          {
            jumpFlag = TRUE;
          }
          else
            jumpFlag = FALSE;
        }
        
        break;
      }
    case OPCODE_BGT:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        ScriptDebug_JumpValue = 1;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if (operandSignedType[0] == 8 ^ operandSignedType[1] == 8)
        {
          if (operandSignedType[0] == 8) // first is FP so convert the second
          {
            tempOp0 = tempOp0 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 < tempOp1)
                jumpFlag = TRUE;
              else
              {
                jumpFlag = FALSE;
              }
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
            {
              jumpFlag = TRUE;
            }
            else if ( !isNeg0 && isNeg1 )
              jumpFlag = FALSE;
            else if ( tempOp0 > tempOp1 ) // only both positive left
            {
              jumpFlag = TRUE;
            }
            else
              jumpFlag = FALSE;
          }
          else // second is FP, so convert the first
          {
            tempOp1 = tempOp1 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 < tempOp1)
                jumpFlag = TRUE;
              else
              {
                jumpFlag = FALSE;
              }
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
              jumpFlag = TRUE;
            else if ( !isNeg0 && isNeg1 )
            {
              jumpFlag = FALSE;
            }
            else if ( tempOp0 > tempOp1 ) // only both positive left
            {
              jumpFlag = TRUE;
            }
            else
              jumpFlag = FALSE;
          }
        }
        else // either both FP or both not FP
        {
          if ( isNeg0 && isNeg1 ) // reverse logic for negatives
          {
            if (tempOp0 < tempOp1)
              jumpFlag = TRUE;
            else
            {
              jumpFlag = FALSE;
            }
          }
          else if ( isNeg0 && !isNeg1 ) // op 0 is negative
          {
            jumpFlag = FALSE;
          }
          else if ( !isNeg0 && isNeg1 )
            jumpFlag = TRUE;
          else if ( tempOp0 > tempOp1 ) // only both positive left
          {
            jumpFlag = TRUE;
          }
          else
            jumpFlag = FALSE;
        }        
        break;
      }
    case OPCODE_BEQ:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        ScriptDebug_JumpValue = 1;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if (isNeg0 != isNeg1) // if signs don't match, => break.
          break;
        
        if (operandSignedType[0] == 8 ^ operandSignedType[1] == 8)
        {
          if (operandSignedType[0] == 8) // first is FP so convert the second
          {
            tempOp0 = tempOp0 << BINARY_POINT;
            
            if (tempOp0 == tempOp1)
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;         
          }
          else // second is FP, so convert the first
          {
            tempOp1 = tempOp1 << BINARY_POINT;
           
            if (tempOp0 == tempOp1)
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;   
          }
        }
        else // either both FP or both not FP
        {
          if (tempOp0 == tempOp1)
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE; 
        }        
        break;
      }
    case OPCODE_BNE:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        ScriptDebug_JumpValue = 1;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if (operandSignedType[0] == 8 ^ operandSignedType[1] == 8)
        {
          if (operandSignedType[0] == 8) // first is FP so convert the second
          {
            tempOp0 = tempOp0 << BINARY_POINT;
            if (( (isNeg0 && isNeg1) || (!isNeg0 && !isNeg1)) && (tempOp0 == tempOp1)) // signs are the same and values == each other
            {                                                                          // if signs are different, then NE.
                jumpFlag = FALSE;
            }
            else
            {
                jumpFlag = TRUE;
            }
          }
          else // second is FP, so convert the first
          {
            tempOp1 = tempOp1 << BINARY_POINT;
            if (( (isNeg0 && isNeg1) || (!isNeg0 && !isNeg1)) && (tempOp0 == tempOp1)) // signs are the same and values == each other
            {                                                                          // if signs are different, then NE.
                jumpFlag = FALSE;
            }
            else
            {
                jumpFlag = TRUE;
            }
          }
        }
        else // either both FP or both not FP
        {
          if (( (isNeg0 && isNeg1) || (!isNeg0 && !isNeg1)) && (tempOp0 == tempOp1)) // signs are the same and values == each other
            {                                                                          // if signs are different, then NE.
                jumpFlag = FALSE;
            }
            else
            {
                jumpFlag = TRUE;
            }
        }        
        break;
      }
    case OPCODE_BGTE:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        ScriptDebug_JumpValue = 1;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if (operandSignedType[0] == 8 ^ operandSignedType[1] == 8)
        {
          if (operandSignedType[0] == 8) // first is FP so convert the second
          {
            tempOp0 = tempOp0 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 <= tempOp1)
                jumpFlag = TRUE;
              else
                jumpFlag = FALSE;
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
              jumpFlag = TRUE;
            else if ( !isNeg0 && isNeg1 )
              jumpFlag = FALSE;
            else if ( tempOp0 >= tempOp1 ) // only both positive left
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;
          }
          else // second is FP, so convert the first
          {
            tempOp1 = tempOp1 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 <= tempOp1)
                jumpFlag = TRUE;
              else
                jumpFlag = FALSE;
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
              jumpFlag = TRUE;
            else if ( !isNeg0 && isNeg1 )
              jumpFlag = FALSE;
            else if ( tempOp0 >= tempOp1 ) // only both positive left
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;
          }
        }
        else // either both FP or both not FP
        {
          if ( isNeg0 && isNeg1 ) // reverse logic for negatives
          {
            if (tempOp0 <= tempOp1)
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;
          }
          else if ( isNeg0 && !isNeg1 ) // op 0 is negative
            jumpFlag = FALSE;
          else if ( !isNeg0 && isNeg1 )
            jumpFlag = TRUE;
          else if ( tempOp0 >= tempOp1 ) // only both positive left
            jumpFlag = TRUE;
          else
            jumpFlag = FALSE;
        }        
        break;
      }
    case OPCODE_BLTE:
      {
        CPU_INT32U tempOp0 = 0;
        CPU_INT32U tempOp1 = 0;
        CPU_BOOLEAN isNeg0 = FALSE;
        CPU_BOOLEAN isNeg1 = FALSE;
        ScriptDebug_JumpValue = 1;
        
        tempOp0 = IntNegToPos(operandVar[0], operandSignedType[0], &isNeg0);
        tempOp1 = IntNegToPos(operandVar[1], operandSignedType[1], &isNeg1);
        
        if (operandSignedType[0] == 8 ^ operandSignedType[1] == 8)
        {
          if (operandSignedType[0] == 8) // first is FP so convert the second
          {
            tempOp0 = tempOp0 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 >= tempOp1)
                jumpFlag = TRUE;
              else
                jumpFlag = FALSE;
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
              jumpFlag = TRUE;
            else if ( !isNeg0 && isNeg1 )
              jumpFlag = FALSE;
            else if ( tempOp0 <= tempOp1 ) // only both positive left
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;
          }
          else // second is FP, so convert the first
          {
            tempOp1 = tempOp1 << BINARY_POINT;
            if ( isNeg0 && isNeg1 ) // reverse logic for negatives
            {
              if (tempOp0 >= tempOp1)
                jumpFlag = TRUE;
              else
                jumpFlag = FALSE;
            }
            else if ( isNeg0 && !isNeg1 ) // op 0 is negative
              jumpFlag = TRUE;
            else if ( !isNeg0 && isNeg1 )
              jumpFlag = FALSE;
            else if ( tempOp0 <= tempOp1 ) // only both positive left
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;
          }
        }
        else // either both FP or both not FP
        {
          if ( isNeg0 && isNeg1 ) // reverse logic for negatives
          {
            if (tempOp0 >= tempOp1)
              jumpFlag = TRUE;
            else
              jumpFlag = FALSE;
          }
          else if ( isNeg0 && !isNeg1 ) // op 0 is negative
            jumpFlag = TRUE;
          else if ( !isNeg0 && isNeg1 )
            jumpFlag = FALSE;
          else if ( tempOp0 <= tempOp1 ) // only both positive left
            jumpFlag = TRUE;
          else
            jumpFlag = FALSE;
        }        
        break;
      }
    case OPCODE_BNZ:
      {
        ScriptDebug_JumpValue = 1;
        if ( operandVar[0] != 0 )
          jumpFlag = TRUE;
        
        break;
      }
    case OPCODE_BZ:
      {
        ScriptDebug_JumpValue = 1;
        if ( operandVar[0] == 0 )
          jumpFlag = TRUE;
        
        break;
      }
    case OPCODE_GOTO:
      {
        ScriptDebug_JumpValue = 1;
        jumpFlag = TRUE;
        break;
      }
    case OPCODE_BBITON:
      {
        ScriptDebug_JumpValue = 1;
        
        CPU_INT32U mask = 0;
        mask |= 1 << operandVar[1];
        if ( (operandVar[0] & mask) > 0)
        {
          jumpFlag = TRUE;
        }
        break;
      }
    case OPCODE_TDEL: // time delay
      {
         OSTimeDlyHMSM(0, 0, operandVar[1], operandVar[0],
                  OS_OPT_TIME_HMSM_STRICT,
                  &err);
        break;
      }
    case OPCODE_GNS: // - get node status
      {        
       
      
        if (getNodeId(&ObjDict_Data) == operandVar[0])  // read the value of the local node (CT)
        {
          resultVar = getState(&ObjDict_Data);
          resultVarSize = 1;
          break;
        }
        
        GetNodeTable( tempNode); 
        if (operandVar[0] == 0 ) // request all nodes
        {          
          resultVar = (CPU_INT32U)&tempNode[0];
          resultVarSize = ACTIVE_NODE_COUNT;
          resultPointerType = 1; 
        }
        else // request a single node value
        {         
          resultVar = tempNode[operandVar[0]];
          resultVarSize = 1;
        }
        break;     
      }
    case OPCODE_BBITOFF:
      {
        ScriptDebug_JumpValue = 1;
        
        CPU_INT32U mask1 = 0; 
        mask1 |= 1 << operandVar[1];
        if ( (operandVar[0] & mask1) == 0)
        {
          jumpFlag = TRUE;
        }
        break;
      }
    case OPCODE_BITSET:
      {
        CPU_INT32U target = operandVar[2]; 
        if (operandVar[0] == 0)
        {
          target &= ~(1 << operandVar[1]);
        }
        else
        {
          target |= ( 1 << operandVar[1]);
        }
        resultVar = target;
        break;
      }
      
    case OPCODE_BITCNT: // bit count
      {
        unsigned int count = 0;
        while (operandVar[0] > 0) 
        {           
            if ((operandVar[0] & 1) == 1)     // check lower bit
                count++;
            
            operandVar[0] >>= 1; 
        }
        resultVar = count;
        break;
      }
    
    case OPCODE_BITCPY: //Copy select bits from one variable to another or convert bits to/from arrays
      {
            //BITCPY  Source    SourceStart    ResultStart     Length  (Result) => Result 
            //           0          1               2             3        4        
            //SourceStart, ResultStart, and Length must be scalars
            //Source and Result cannot both be Vectors (use VECMOV instead) 
            CPU_INT32U sum = 0;
            CPU_INT08U numElements = 0;
              
            if (operandPointerType[1] != 0 || operandPointerType[2] != 0 || operandPointerType[3] != 0 )
            {
                return SCRIPT_ERR_OPERAND_TYPE;  //source/result Start, and length must be scalars
            }
            if ((operandPointerType[0] != 0) && (operandPointerType[4] != 0))
            {
                return SCRIPT_ERR_OPERAND_TYPE;  //Source and Result cannot both be pointer types
            }
            if ((operandPointerType[0] == 0) && (operandPointerType[4] == 0))
            {
                //Source and Result are both scalars: bit to bit mapping
                //Error checking on length
                if(operandVar[3]==0)
                {
                    resultVar = operandVar[4];  //result unchanged
                }
                else
                {
                    if(operandVar[1]+operandVar[3]>operandVarSize[0]*8 || operandVar[2]+operandVar[3]>operandVarSize[4]*8)
                    {
                        return SCRIPT_ERR_OPERAND_OUT_OF_RANGE;
                    }
                    else
                    {
                        //bits before ResultStart are unchanged
                        for (j=0; j<operandVar[2]; j++) 
                        {
                            sum += operandVar[4] & (1 << j);
                        }

                        //bits between ResultStart and ResultStart+Length are mapped from Source
                        //SourceStart. Index is relative to Source so we need to shift bits up/down 
                        //to Result bit position
                        for (j=operandVar[1]; j<operandVar[1]+operandVar[3]; j++) 
                        {
                            if (operandVar[2] > operandVar[1])
                                sum += (operandVar[0] & (1 << j)) << (operandVar[2]-operandVar[1]);
                            else if (operandVar[2] < operandVar[1])
                                sum += (operandVar[0] & (1 << j)) >> (operandVar[1]-operandVar[2]);
                            else
                                sum += (operandVar[0] & (1 << j));
                        }

                        
                        //bits after ResultStart+Length are unchanged
                        for (j=operandVar[2]+operandVar[3]; j<operandVarSize[4]*8; j++)
                        {
                            sum += operandVar[4] & (1 << j);
                        }
                        resultVar = sum;
                    }
                }
            }
            else if ((operandPointerType[0] == 0) && (operandPointerType[4] != 0))
            {
                //Source is Scalar, Result is Vector.  bits map to 1s and 0s in Vector
                
                if(operandVarSize[4] > sizeof(tempResultsStr))
                {
                    return SCRIPT_ERR_DESTINATIONARRAY;
                }
                
                if(operandSignedType[4])
                   bytesPerElement = abs(operandSignedType[4]);
                else
                  bytesPerElement = 1;
                
                numElements = operandVarSize[4] / bytesPerElement;
                if(operandVar[1]+operandVar[3]>operandVarSize[0]*8 || operandVar[2]+operandVar[3]>numElements)
                {
                    return SCRIPT_ERR_OPERAND_OUT_OF_RANGE;
                }
                
                //Copy bytes from Operand 4 (Result) into Temporary byte space
                memcpy(tempResultsStr, (CPU_INT08U*) operandVar[4], operandVarSize[4]);
                
                //bits between ResultStart and ResultStart+Length are mapped from Source
                //SourceStart. Index is 0 to length.
                CPU_INT08U bitval;
                for (j=0; j<operandVar[3]; j++) 
                {
                    //Compare Source bit SourceStart+j
                    bitval = (operandVar[0] & (1 << (j+operandVar[1])))>0; 
                    //Put into array elements
                    switch(bytesPerElement)
                    {
                        case 1: tempResultsStr[operandVar[2]+j] = bitval; 
                                break;
                        case 2: tempResultsStr[operandVar[2]+j*2  ] = bitval; 
                                tempResultsStr[operandVar[2]+j*2+1] = 0; 
                                break;
                        case 4: tempResultsStr[operandVar[2]+j*4  ] = bitval; 
                                tempResultsStr[operandVar[2]+j*4+1] = 0; 
                                tempResultsStr[operandVar[2]+j*4+2] = 0; 
                                tempResultsStr[operandVar[2]+j*4+3] = 0; 
                                break;
                        default:
                            return SCRIPT_ERR_OPERAND_TYPE;
                    }
                }

                resultVar = (CPU_INT32U) tempResultsStr; //Pointer
                resultPointerType = 1;
                resultVarSize = operandVarSize[4]; 
            }
            else
            {
                //Source is Vector, Result is Scalar.  Non zero Vector values map to 1 bits
                if(operandVarSize[0] > sizeof(tempResultsStr))
                {
                    return SCRIPT_ERR_OPERAND_OUT_OF_RANGE ;
                }
                
                if(operandSignedType[0])
                   bytesPerElement = abs(operandSignedType[0]);
                else
                  bytesPerElement = 1;
                
                numElements = operandVarSize[0] / bytesPerElement;
                if(operandVar[1]+operandVar[3]>numElements || operandVar[2]+operandVar[3]>operandVarSize[4]*8)
                {
                    return SCRIPT_ERR_OPERAND_OUT_OF_RANGE;
                }
                
                if(operandVar[3]==0)
                {
                    resultVar = operandVar[4];  //result unchanged
                }
                else
                {          
                    //bits before ResultStart are unchanged
                    for (j=0; j<operandVar[2]; j++) 
                    {
                        sum += operandVar[4] & (1 << j);
                    }
                    
                    //bits between ResultStart and ResultStart+Length are mapped from tempResultsStr
                    CPU_BOOLEAN isNegOp;
                    for (j=0; j<operandVar[3]; j++) 
                    {
                        if(getElementAsUint32(operandSignedType[0], operandVar[0], operandVar[1]+j, &isNegOp)>0)
                          sum += (1 << (j+operandVar[2]));
                    }
                
                    //bits after ResultStart+Length are unchanged
                    for (j=operandVar[2]+operandVar[3]; j<operandVarSize[4]*8; j++)
                    {
                        sum += operandVar[4] & (1 << j);
                    }
                    resultVar = sum;
                }               
            }
        break;
      }
    // operand is the SCRIPT_POINTER of the target  
    case OPCODE_STARTSCPT: // run script
      {
        tempErr = Start_Script((CPU_INT08U)operandVar[0]);
        if (tempErr > 0)
        {
          return tempErr;
        }
        else
          break;
      }
    case OPCODE_STOPSCPT: // stop script
      {
         tempErr = Stop_Script((CPU_INT08U)operandVar[0]);
         if (tempErr > 0)
         {
          return tempErr;
         }
         else
          break;
      }
    case OPCODE_RUNONCE: // run once script
      {
        tempErr = RunOnceScript((CPU_INT08U)operandVar[0], 0);
        if (tempErr > 0)
        { 
          return tempErr;
        }
        else
          break;
      }
    case OPCODE_RUNIMM: // run immediate
      {
        tempErr = RunScriptImmediate((CPU_INT08U)operandVar[0]);
       
        break;
      }
    case OPCODE_RUNNEXT: // run next
      {              
        CPU_INT08U pControlWord[4];
        CPU_INT32U varsizeS = 0;
        CPU_INT32U abortCodeS = 0;
        UNS8 typeS = 0;
        
        if(operandVar[0]>MAX_NUMBER_SCRIPTS)
          return SCRIPT_ERR_INVALID_CHILD_SCRIPT;
        
        *pChildScriptPointer = (CPU_INT08U) operandVar[0];  // set script number 
        abortCodeS = readLocalDict( &ObjDict_Data, 0x1F51, *pChildScriptPointer, &pControlWord[0], &varsizeS, &typeS, 0);
        if (abortCodeS != 0 || pControlWord[1] == 0)
        {
          *pChildScriptPointer = 0;
          return SCRIPT_ERR_INVALID_CHILD_SCRIPT;
        }        
        break;
      }
    case OPCODE_RUNMULT: // run multiple
      {              
        tempErr = RunScriptMultiple((CPU_INT32U)operandVar[0]);    
        break;
      }
    case OPCODE_RESETGLOBALS: // Reset Globals
      {              
        memcpy((CPU_INT08U*)globalVarTableAddress, (CPU_INT08U*)globalInitVarTableAddress, sizeOfGlobalVarTable);
        //if (LoadGlobalVarTable(operandVar[0]))  
        //{
        //  ScriptDebug_statusByte = SCRIPT_ERR_RESETGLOBALS_FAILED;
        //  return SCRIPT_ERR_RESETGLOBALS_FAILED;
        //}
        break;
      }
    case  OPCODE_NODESCAN:
      {
        if(operandVar[0]<1 || operandVar[0]>127)
           return SCRIPT_ERR_OPERAND_OUT_OF_RANGE;
        
        resultVar = scanBootloaderNode((CPU_INT08U)operandVar[0]);
        break;
      }
    case OPCODE_INTERPOL:
      {
        // operandVar[0] xValue to interpolate
        // operandVar[1] -> pointer to xValue array (should be monotonically increasing!)
        // operandVar[2] -> pointer to yValue array
        
        //check that first operand is scalar
        if (operandPointerType[0] != 0 || operandPointerType[1] != 1 || operandPointerType[2] != 1 )
        {
          return SCRIPT_ERR_OPERAND_TYPE;  //First operand must be scalar, others must be pointers
        }
        
        CPU_INT08U numElementsX, numElementsY;
        
        if(operandSignedType[1])
           numElementsX = operandVarSize[1] / abs(operandSignedType[1]);
        else
          numElementsX = operandVarSize[1];
        
        if(operandSignedType[2])
           numElementsY = operandVarSize[2] / abs(operandSignedType[2]);
        else
          numElementsY = operandVarSize[2];
        
        if (numElementsX != numElementsY || operandSignedType[1] == 8 || operandSignedType[2] == 8) //No FP support
        {
          return SCRIPT_ERR_OPERAND_TYPE_MISMATCH;
        }
        
        double tempX1, tempX2, tempY1, tempY2, tempX, tempY;

        tempX1 = getElementAsDouble( operandSignedType[1], operandVar[1], 0);
        tempX2 = getElementAsDouble( operandSignedType[1], operandVar[1], numElementsX-1); 
        switch ( operandSignedType[0])
        {
          case 1: tempX = (double) ((CPU_INT08S) operandVar[0]);
          break;
          case 2: tempX = (double) ((CPU_INT16S) operandVar[0]);
          break;
          case 4: tempX = (double) ((CPU_INT32S) operandVar[0]);
          break;
          default:
            tempX = (double) operandVar[0];
        }
        
           
        // test for boundary conditions
        if (tempX <= tempX1)
        {
          tempY1 = getElementAsDouble( operandSignedType[2], operandVar[2], 0);
          tempY = tempY1;  
        }
        else if (tempX >= tempX2) 
        {
          tempY2 = getElementAsDouble( operandSignedType[2], operandVar[2], numElementsY-1); 
          tempY = tempY2; 
        }    
        else // interpolate
        {        
          for (k = 0; k < numElementsX-1; k++)
          {           
            tempX1 = getElementAsDouble( operandSignedType[1], operandVar[1], k);
            tempX2 = getElementAsDouble( operandSignedType[1], operandVar[1], k+1); 
            
            if (  tempX >= tempX1 && tempX < tempX2)
            {
              tempY1 = getElementAsDouble( operandSignedType[2], operandVar[2], k);
              tempY2 = getElementAsDouble( operandSignedType[2], operandVar[2], k + 1);
              tempY =  tempY1 + ( tempX - tempX1  ) * (  tempY2 - tempY1  ) / (  tempX2 - tempX1 );     
              break; //from for loop
            }  
          }
        }
        
        if (tempY < 0)
          resultVar = ~(CPU_INT32U)(-tempY) + 1;
        else 
          resultVar = (CPU_INT32U) tempY;
        
        break;
      }
    case OPCODE_MAVG: //moving average
      {
        
       
        break;
      }
    case OPCODE_IIR: // IIR filter
      {
        
       
        break;
      }
    case OPCODE_VECMOV:
    {
      // operandVar[0]: source array (pointer type)
      // operandVar[1]: source starting element 
      // operandVar[2]: dest starting element
      // operandVar[3]: number of elements
      //resultOperand: destination array
        
        //check that first operand is scalar
        if (operandPointerType[0] == 0 || operandPointerType[1] != 0 || operandPointerType[2] != 0 || operandPointerType[3] != 0 )//|| !resultOperandPointerType)
        {
          return SCRIPT_ERR_OPERAND_TYPE;  //First operand must be pointer, others must be scalars
        }
        
        CPU_INT16U numElementsSrc, numElementsDest;
        CPU_INT08U bytesPerElementSrc, bytesPerElementDest;
        
        if(operandSignedType[0])
           bytesPerElementSrc = abs(operandSignedType[0]);         
        else
          bytesPerElementSrc = 1;
        
        numElementsSrc = operandVarSize[0] / bytesPerElement;
        
        if(resultOperandSignedType)
           bytesPerElementDest = abs(resultOperandSignedType);
        else
          bytesPerElementDest = 1;
        
        if(bytesPerElementSrc != bytesPerElementDest)
          return SCRIPT_ERR_OPERAND_TYPE_MISMATCH;
        
        if(networkOperandFlag && networkAddress[6]>1)
          numElementsDest = networkAddress[6]; //number of subindices
        else  
          numElementsDest = resultOperandVarSize / bytesPerElementDest;
        
        if (operandVar[3] + operandVar[1] > numElementsSrc)
        {
          return SCRIPT_ERR_OPERAND_TYPE_MISMATCH; //Num elements and/or starting index too high for source array
        }
        if (operandVar[3] + operandVar[2] > numElementsDest)
        {
          return SCRIPT_ERR_DESTINATIONARRAY; //Num elements and/or starting index too high for source array
        }
        

        resultPointerType = 1;
        resultVarSize = operandVar[3]*bytesPerElementSrc;
        
        if(networkOperandFlag && networkAddress[6]>1)
        {
            networkAddress[5]+=operandVar[2]; //destination (subindex)
            networkAddress[6] = operandVar[3]; //number of subindices = num elemnts
        }
        else
        {
          varAddress+=operandVar[2]*bytesPerElementSrc;
        }
        resultVar = operandVar[0] + operandVar[1]*bytesPerElementSrc;
        
      break;
    }
    case OPCODE_FIFO: //only works with arrays, not subindices
      {   
        CPU_INT08U bytesPerElement;
        CPU_INT16U numElements, i;
        
         if (operandPointerType[0] || !resultOperandPointerType)
        {
          return SCRIPT_ERR_OPERAND_TYPE;  //First operand must be scalar, result must be pointer
        }
        
        if(resultOperandSignedType)
           bytesPerElement = abs(resultOperandSignedType);
        else
          bytesPerElement = 1;
        
        numElements = resultOperandVarSize / bytesPerElement;
        
        for(i=numElements-1; i>0; i--)
        {
            switch(bytesPerElement)
            {
              //JML note: this is different form other operands because we are actually 
              //already changing the output values here
              case 1: 
                  *(CPU_INT08U*)(varAddress+i)=*(CPU_INT08U*)(varAddress+i-1);
                  break;
              case 2:
                  *(CPU_INT08U*)(varAddress+i*2+1)=*(CPU_INT08U*)(varAddress+i*2-1);
                  *(CPU_INT08U*)(varAddress+i*2  )=*(CPU_INT08U*)(varAddress+i*2-2);
                  break;
              case 4:
                  *(CPU_INT08U*)(varAddress+i*4+3)=*(CPU_INT08U*)(varAddress+i*4-1);
                  *(CPU_INT08U*)(varAddress+i*4+2)=*(CPU_INT08U*)(varAddress+i*4-2);
                  *(CPU_INT08U*)(varAddress+i*4+1)=*(CPU_INT08U*)(varAddress+i*4-3);
                  *(CPU_INT08U*)(varAddress+i*4  )=*(CPU_INT08U*)(varAddress+i*4-4);
                  break;
              default:
                return SCRIPT_ERR_OPERAND_TYPE;
                
            }//end switch
        }//end while
        
        
        CPU_INT32U newEntry = (CPU_INT32U) operandVar[0]; //cast to unsigned 
        
        switch(bytesPerElement)
        {
          case 1:
              *(CPU_INT08U*)varAddress = (CPU_INT08U) newEntry;
              break;
          case 2:
              *(CPU_INT08U*)(varAddress    ) = (CPU_INT08U)  newEntry;
              *(CPU_INT08U*)(varAddress + 1) = (CPU_INT08U) (newEntry >> 8);
              break;
          case 4:
              *(CPU_INT08U*)(varAddress    ) = (CPU_INT08U)  newEntry;
              *(CPU_INT08U*)(varAddress + 1) = (CPU_INT08U) (newEntry >> 8);
              *(CPU_INT08U*)(varAddress + 2) = (CPU_INT08U) (newEntry >>16);
              *(CPU_INT08U*)(varAddress + 3) = (CPU_INT08U) (newEntry >>24);
              break;
          default:
            return SCRIPT_ERR_OPERAND_TYPE;
              
        }
        //JML note: this is a bit strange because we already set varAddress, but output section will reset 
        //varAddress based on resultVar
        resultVar = varAddress; 
        resultPointerType = 1;
        resultVarSize = numElements*bytesPerElement;
        break;
     }
    case OPCODE_VECMAX:  
    case OPCODE_VECMAXI: 
     {
       CPU_INT16U numElements, i, iMax;
       CPU_INT08U bytesPerElement;
       CPU_INT32U max = MAX4, tempOp;
       CPU_BOOLEAN isNegMax = TRUE, isNegOp;
       
        if (operandPointerType[0] == 0)
        { 
            return SCRIPT_ERR_OPERAND_TYPE;  //Operand must be pointer
        }
        
        if(operandSignedType[0])
           bytesPerElement = abs(operandSignedType[0]);         
        else
          bytesPerElement = 1;
        
        numElements = operandVarSize[0] / bytesPerElement;
        
        for(i =0; i<numElements; i++)
        {
          tempOp = getElementAsUint32(operandSignedType[0], operandVar[0], i, &isNegOp);
       
          if ( isNegMax && isNegOp && max > tempOp) // tempOp less negative
          {
            max = tempOp;
            iMax = i;
          }
          else if ( isNegMax && !isNegOp ) // tempOp nonnegative
          {
            max = tempOp;
            isNegMax = FALSE;
            iMax = i;
          }
          else if (!isNegMax && !isNegOp && tempOp > max ) //  tempOp more positive
          {
            max = tempOp; 
            iMax = i;
          }
        }//end for loop
     
        if (scriptOpCodeValue == OPCODE_VECMAXI)
        {
          resultVar = iMax; //return index of maximum
        }
        else
        {
          if(isNegMax)
            resultVar = ~max + 1;
          else
            resultVar = max; //return maximum
        }
        
        break;
     }
    case OPCODE_VECMIN:  
    case OPCODE_VECMINI: 
     {
       CPU_INT16U numElements, i, iMin;
       CPU_INT08U bytesPerElement;
       CPU_INT32U min = MAX4, tempOp;
       CPU_BOOLEAN isNegMin = FALSE, isNegOp;
       
        if (operandPointerType[0] == 0)
        { 
            return SCRIPT_ERR_OPERAND_TYPE;  //Operand must be pointer
        }
        
        if(operandSignedType[0])
           bytesPerElement = abs(operandSignedType[0]);         
        else
          bytesPerElement = 1;
        
        numElements = operandVarSize[0] / bytesPerElement;
        
        for(i =0; i<numElements; i++)
        {
          tempOp = getElementAsUint32(operandSignedType[0], operandVar[0], i, &isNegOp);
       
          if ( isNegMin && isNegOp && min < tempOp) // tempOp more negative
          {
            min = tempOp;
            iMin = i;
          }
          else if ( !isNegMin && isNegOp ) // tempOp negative
          {
            min = tempOp;
            isNegMin = TRUE;
            iMin = i;
          }
          else if (!isNegMin && !isNegOp && tempOp < min ) //  tempOp less positive
          {
            min = tempOp; 
            iMin = i;
          }
        }//end for loop
     
        if (scriptOpCodeValue == OPCODE_VECMINI)
        {
          resultVar = iMin; //return index of maximum
        }
        else
        {
          if(isNegMin)
            resultVar = ~min + 1;
          else
            resultVar = min; //return maximum
        }
        
        break;
     }  
    case OPCODE_VECMED: //median
    case OPCODE_VECMEDI: //median index
      {
        //JML Note: This expects an odd number of elements.  
        //If an even number are used, the lower of two middle elements is output.
        //i.e. this does not take the average of the two middle elements.
        //If there are multiple indices with the same median value, the first matching index 
        //will output
       CPU_INT16U numElements, i, j, cntLo, cntHi, diff, mindiff;
       CPU_INT08U bytesPerElement;
       CPU_INT32U tempOp0, tempOp1;
       CPU_BOOLEAN isNegOp0, isNegOp1;
       
        if (operandPointerType[0] == 0)
        { 
            return SCRIPT_ERR_OPERAND_TYPE;  //Operand must be pointer
        }
        
        if(operandSignedType[0])
           bytesPerElement = abs(operandSignedType[0]);         
        else
          bytesPerElement = 1;
        
        numElements = operandVarSize[0] / bytesPerElement;
        
        for(i =0; i<numElements; i++)
        {
          cntLo = 0;
          cntHi = 0;
          for(j=0; j< numElements; j++)
          {
            if(i==j)
              continue;
            
            tempOp0 = getElementAsUint32(operandSignedType[0], operandVar[0], i, &isNegOp0);
            tempOp1 = getElementAsUint32(operandSignedType[0], operandVar[0], j, &isNegOp1);
         
            //increment count if tempOp0 < tempOp1, separate counter if equal
            if ( ( isNegOp0 &&  isNegOp1 && tempOp0 > tempOp1) || // tempOp0 more negative
                 ( isNegOp0 && !isNegOp1                     ) || // tempOp0 is negative
                 (!isNegOp0 && !isNegOp1 && tempOp0 < tempOp1) )  // tempOp0 less positive
            {
              cntLo++;
            }
            else if (! ((isNegOp0 && isNegOp1 || !isNegOp0 && !isNegOp1) && tempOp0 == tempOp1))
            {
              cntHi++;
            }
        }//end "j" for loop
        
        diff = abs(cntHi - cntLo);
        if(diff<mindiff) 
        {
          mindiff=diff;
          
          if (scriptOpCodeValue == OPCODE_VECMEDI)
          {
            resultVar = i;
          }
          else
          {
            if(isNegOp0)
               resultVar = ~tempOp0 + 1;
             else
               resultVar = tempOp0;
          }
          if(diff==0) //found the middle value.
            break; //from "i" for loop
        }
      }//end "i" for loop
        break;
      }
    case OPCODE_VECSUM: //sum of elements
    case OPCODE_VECMEAN:  //mean of elements
      {
       CPU_INT16U numElements, i;
       CPU_INT08U bytesPerElement;
       CPU_INT32U tempOp;
       CPU_INT64U posSum = 0, negSum = 0;
       CPU_BOOLEAN isNegOp;
       
        if (operandPointerType[0] == 0)
        { 
            return SCRIPT_ERR_OPERAND_TYPE;  //Operand must be pointer
        }
        
        if(operandSignedType[0])
           bytesPerElement = abs(operandSignedType[0]);         
        else
          bytesPerElement = 1;
        
        numElements = operandVarSize[0] / bytesPerElement;
        
        for(i =0; i<numElements; i++)
        {
          tempOp = getElementAsUint32(operandSignedType[0], operandVar[0], i, &isNegOp);
          if (isNegOp)
          {
            negSum += (CPU_INT64U)tempOp;
          }
          else
            posSum += (CPU_INT64U)tempOp;     
        }
        
        if(posSum >= negSum)
        {
            posSum -= negSum;
            negSum = 0;
        }
        else
        {
          negSum -= posSum;
          posSum = 0;
        }
        
        if (scriptOpCodeValue == OPCODE_VECSUM)
        {
          if(posSum > MAX4) posSum = MAX4; //prevent overflow
          if(negSum > MAX4) negSum = MAX4;  
          
           if(negSum)
             resultVar = ~(CPU_INT32U)negSum + 1;
           else
             resultVar = (CPU_INT32U)posSum;
        }
        else //for MEAN, divide by number of elements (can't overflow 32bit after division)
        {
          if(negSum)
             resultVar = ~(CPU_INT32U)(negSum/numElements) + 1;
           else
             resultVar = (CPU_INT32U)(posSum/numElements);
        }
        
        break;
      } 
   case OPCODE_VECPROD:  //product of elements
      {
       CPU_INT16U numElements, i;
       CPU_INT08U bytesPerElement;
       CPU_INT32U tempOp;
       CPU_INT64U prod = 1;
       CPU_BOOLEAN isNegProd = FALSE, isNegOp;
       
        if (operandPointerType[0] == 0)
        { 
            return SCRIPT_ERR_OPERAND_TYPE;  //Operand must be pointer
        }
        
        if(operandSignedType[0])
           bytesPerElement = abs(operandSignedType[0]);         
        else
          bytesPerElement = 1;
        
        numElements = operandVarSize[0] / bytesPerElement;
        
        //product of multiple UINT32s can exceed UINT64, so saturate must be implemented at each multiply
        for(i =0; i<numElements; i++)
        {
          tempOp = getElementAsUint32(operandSignedType[0], operandVar[0], i, &isNegOp);
          
          isNegProd ^= isNegOp;
          prod *= (CPU_INT64U)tempOp;
          
          if(prod > MAX4) prod = MAX4;  //prevent overflow
        }
        
        if(isNegProd)
          resultVar = ~(CPU_INT32U)prod + 1;
        else
          resultVar = (CPU_INT32U)prod;

        break;
      }
    case OPCODE_VECMAG:   //magnitude
    case OPCODE_VECMAG2:  //sum of squares (magnitude squared)
      {
       CPU_INT16U numElements, i;
       CPU_INT08U bytesPerElement;
       CPU_INT32U tempOp;
       CPU_INT64U sum = 0;
       CPU_BOOLEAN isNeg;
       
        if (operandPointerType[0] == 0)
        { 
            return SCRIPT_ERR_OPERAND_TYPE;  //Operand must be pointer
        }
        
        if(operandSignedType[0])
           bytesPerElement = abs(operandSignedType[0]);         
        else
          bytesPerElement = 1;
        
        numElements = operandVarSize[0] / bytesPerElement;
        
        for(i =0; i<numElements; i++)
        {
          tempOp = getElementAsUint32(operandSignedType[0], operandVar[0], i, &isNeg);
          
          sum += (CPU_INT64U)tempOp*tempOp;
        }
        
        if(sum > MAX4) sum = MAX4; //prevent overflow
        
        if (scriptOpCodeValue == OPCODE_VECMAG)
          resultVar = (CPU_INT32U) sqrt(sum);
        else
          resultVar = (CPU_INT32U) sum;

        break;
      }   
    case OPCODE_VECADD: //these do not work with Network Operand 
    case OPCODE_VECSUB:
    case OPCODE_VECMUL:
    case OPCODE_VECDIV:  
      {
       CPU_INT16U numElementsOp1, numElementsOp2, numElementsDest, i;
       CPU_INT08U bytesPerElementOp1, bytesPerElementOp2, bytesPerElementDest;
       CPU_INT32U tempOp1, tempOp2, tempRes;
       CPU_INT64U tempResult;
       CPU_BOOLEAN isNegOp1, isNegOp2, isNegResult;
              
        if (operandPointerType[0] == 0 || !resultOperandPointerType)
        { 
            return SCRIPT_ERR_OPERAND_TYPE;  //First operand and result must be pointer
        }

        
        if(operandSignedType[0])
           bytesPerElementOp1 = abs(operandSignedType[0]);         
        else
          bytesPerElementOp1 = 1;
        
        numElementsOp1 = operandVarSize[0] / bytesPerElementOp1;
        
        if (operandPointerType[1] == 0) //second operand is scalar
        { 
            numElementsOp2 = 0;
        }
        else //second operand is vector
        {
          if(operandSignedType[1])
             bytesPerElementOp2 = abs(operandSignedType[1]);         
          else
            bytesPerElementOp2 = 1;
          
          numElementsOp2 = operandVarSize[1] / bytesPerElementOp2;
          
          if(numElementsOp1 != numElementsOp2)
            return SCRIPT_ERR_OPERAND_TYPE_MISMATCH;
          
        }
        
        if(resultOperandSignedType)
           bytesPerElementDest = abs(resultOperandSignedType);
        else
          bytesPerElementDest = 1;
               
        numElementsDest = resultOperandVarSize / bytesPerElementDest;
        
        if(numElementsOp1 != numElementsDest)
            return SCRIPT_ERR_OPERAND_TYPE_MISMATCH;
        
        for( i=0; i<numElementsOp1; i++)
        {
          tempOp1 = getElementAsUint32(operandSignedType[0], operandVar[0], i, &isNegOp1);
          if(numElementsOp2) //vector operand2
          {
            tempOp2 = getElementAsUint32(operandSignedType[1], operandVar[1], i, &isNegOp2);
            if(scriptOpCodeValue == OPCODE_VECSUB) //flip sign if subtracting instead of adding
              isNegOp2=!isNegOp2;
          }
          else if(i==0)  //scalar operand2: stays the same while operand1 elemnt iterates
          {
            tempOp2 = IntNegToPos(operandVar[1], operandSignedType[1], &isNegOp2);            
            if(scriptOpCodeValue == OPCODE_VECSUB) //flip sign if subtracting instead of adding
              isNegOp2=!isNegOp2;
          }
          
          if(scriptOpCodeValue == OPCODE_VECSUB || scriptOpCodeValue == OPCODE_VECADD)
          {
               
            if(isNegOp1 && isNegOp2) //both negative
            {
              tempResult = tempOp1 + tempOp2;
              isNegResult = TRUE;
            }
            else if(isNegOp1 && !isNegOp2) //Op1 negative
            {
              if(tempOp1 > tempOp2)
              {
                tempResult = tempOp1 - tempOp2;
                isNegResult = TRUE;
              }
              else
              {
                tempResult = tempOp2 - tempOp1;
                isNegResult = FALSE;
              }
            }
            else if(!isNegOp1 && isNegOp2) //Op2 negative
            {
              if(tempOp1 > tempOp2)
              {
                tempResult = tempOp1 - tempOp2;
                isNegResult = FALSE;
              }
              else
              {
                tempResult = tempOp2 - tempOp1;
                isNegResult = TRUE;
              }
            }
            else //both positive
            {
              tempResult = tempOp1 + tempOp2;
              isNegResult = FALSE;
            }
          }
          else if (scriptOpCodeValue == OPCODE_VECMUL) 
          {
            tempResult = tempOp1 * tempOp2;
            isNegResult = isNegOp1^isNegOp2;
          }
          else //OPCODE_VECDIV
          {
            tempResult = tempOp1 / tempOp2;
            isNegResult = isNegOp1^isNegOp2;
          }
          if(tempResult > MAX4) tempResult = MAX4; //prevent overflow
          
          if(isNegResult)
           tempRes = ~(CPU_INT32U)tempResult+1;
          else
           tempRes = (CPU_INT32U)tempResult;

         //JML note: this is different form other operands because we are actually 
         //already changing the output values here
          setOperand(operandScopeType, (CPU_INT08U*)(varAddress+i*bytesPerElementDest), tempRes, bytesPerElementDest);
//            switch(bytesPerElementDest)
//            {
//              /
//              case 1: 
//                  *(CPU_INT08U*)(varAddress+i)=(CPU_INT08U)tempRes;
//                  break;
//              case 2:
//                  *(CPU_INT08U*)(varAddress+i*2  )=(CPU_INT08U) tempRes;
//                  *(CPU_INT08U*)(varAddress+i*2+1)=(CPU_INT08U)(tempRes>>8);
//                  break;
//              case 4:
//                  *(CPU_INT08U*)(varAddress+i*4  )=(CPU_INT08U) tempRes;
//                  *(CPU_INT08U*)(varAddress+i*4+1)=(CPU_INT08U)(tempRes>> 8);                  
//                  *(CPU_INT08U*)(varAddress+i*4+2)=(CPU_INT08U)(tempRes>>16);
//                  *(CPU_INT08U*)(varAddress+i*4+3)=(CPU_INT08U)(tempRes>>24);
//                  break;
//              default:
//                return SCRIPT_ERR_OPERAND_TYPE;
//                
//            }//end switch
        }//end for "i"
        resultVar = varAddress;
        resultPointerType = 1;
        resultVarSize = numElementsDest*bytesPerElementDest;
        break;
        
      }
    case OPCODE_EXIT: // exit
      {
        break;
      }
    default:
      {
        return SCRIPT_ERR_INVALID_OPCODE;
      }
      
    } 
    
//----------------------------------------------------------------------------// 
//                         END OPCODE INTERPRETER 
//----------------------------------------------------------------------------//

   // process results operand -- resultsOperandAddress is relative to script            
    if (numberOfResultsOperands > 0)
    {
      //already got size and type in original operand loop
      //resultsOperandSize      = currentOperandSize;  
      //resultsOperandScopeType = operandScopeType;
      //networkAddress and varAddress valid for result
      if (jumpOperandFlag ) 
      {
        //jumps don't assign any result so resultVar and resultVarSize may be undefined
        //currentOperandAddress has already been set to next operand position (assuming no jump)
        //However: varAddress = (previous) currentOperandAddress + 2
        //The jump is calculated from the currentOperationAddress (not Operand).  This is still
        //valid until the end of the OpCode loop
        
        if(jumpFlag)
        {
          // set flag for script debug
         ScriptDebug_JumpValue = 2;
         
          jumpBytes = (*(CPU_INT08U * )(varAddress)) +  (*(CPU_INT08U * )(varAddress + 1) << 8);
          jumpBack = *(CPU_INT08U * )(varAddress + 3); // 1 for backwards, 0 for forwards
          
          if (jumpBack)
            nextOperationAddress = currentOperationAddress - jumpBytes;
          else
            nextOperationAddress = currentOperationAddress + jumpBytes;
        }
        //otherwise, do nothing.  nextOperationAddress has already been set in the case of no jump
      }
      else if(networkOperandFlag)
      { 
        //For bytearrays, strings, and arrays we assume that resultVarSize is correct and will match the network size
        //for scalars, we use the size specified in the Result Operand in case the result is being cast to a different type
        if(resultPointerType)
          networkError = setNetworkOperand(operandScopeType, networkAddress, resultVar, resultVarSize, TRUE);  
        else
          networkError = setNetworkOperand(operandScopeType, networkAddress, resultVar, resultOperandVarSize, FALSE); 
        

          //return SCRIPT_ERR_SETNETWORKDATA; 
        if (networkError && !(Control_SystemControl & 0x80)) 
        {
          // error: Could not set data to Network 
          return SCRIPT_ERR_SETNETWORKDATA; 
        }
        else
        {
          if(networkError)
          {
            RADIO_SDO_Script_Failures++;  // increment error
            currentOperationAddress = nextOperationAddress; 
            continue; //skip to next OpCode
          }
        }
      }
      else if(arrayOperandFlag)
      {	
        if(!resultPointerType)
        {
          //error: source is scalar but result is pointer
          return SCRIPT_ERR_SCALAR_TO_POINTER;
        }
        if(resultVarSize > resultOperandVarSize)
        {
          //error: source array is larger than destination 
          return SCRIPT_ERR_DESTINATIONARRAY;
        }
        memcpy((CPU_INT08U* ) varAddress, (CPU_INT08U* )resultVar, resultVarSize*sizeof(CPU_INT08U));

        //if the destination array is bigger than the result array, set the remaining bytes to zeros
        //if (resultOperandVarSize > resultVarSize)
        //{
        //  memset((CPU_INT08U* )(varAddress + resultVarSize), 0, resultOperandVarSize-resultVarSize);
        //}
      }
     
      else
      {
        if(resultPointerType) 
        {
          if(!resultOperandPointerType) //error: source result is pointer but resultOperand is scalar
            return SCRIPT_ERR_POINTER_TO_SCALAR;
          if(resultVarSize > resultOperandVarSize)  //error: source array is larger than destination 
            return SCRIPT_ERR_DESTINATIONARRAY;
        }
        else //scalar result
        {
          if(resultOperandPointerType) //error: source result is scalar but resultOperand is pointer
            return SCRIPT_ERR_SCALAR_TO_POINTER;
        }
       
        if(saturateOpcode)
          ResultsVarTypeSaturate( &resultVar, operandScopeType & 0x0F, overFlowOpcode, saturateIsNeg );
        
        setOperand(operandScopeType, (CPU_INT08U*) varAddress, resultVar, resultOperandVarSize);
        
      }
    }
    
    if(resultPointerType) //JML added to improve script debugging
      resultVar = varAddress;
    
    // check for abort bit
    if ((ScriptDebug_controlByte & 0x80) == 0x80)
    {
      ScriptDebug_controlByte = 0;
      return SCRIPT_ERR_ABORTED;
    }
    
    // debug control
    if ((ScriptDebug_controlByte & 0x01) == 0x01 )
    {
      ScriptDebug_CurrentExecutionNumber++;
      ScriptDebug_controlByte |= 0x02;  // reset wait -> sets it to 3
      ScriptDebug_currentOpVar0 = operandVar[0];
      ScriptDebug_currentOpVar1 = operandVar[1];
      ScriptDebug_currentOpVar2 = operandVar[2];
      ScriptDebug_currentOpVar3 = operandVar[3];
      ScriptDebug_currentOpVar4 = operandVar[4];
      ScriptDebug_currentResultVar = resultVar;
      ScriptDebug_currentOpcode = scriptOpCodeValue;
      ScriptDebug_currentOpAddress = currentOperationAddress;
      
      UINT8 copyBytes;
      UINT8 remBytes;  
      
      //copy Stack Table (set unused bytes to 0) 
      copyBytes = sizeof(ScriptDebug_stackVars);
      if(ScriptDebug_stackIndex >= sizeOfStackVarTable) //starting stack index out of range
      {
        copyBytes = 0;
      }
      else if(ScriptDebug_stackIndex + sizeof(ScriptDebug_stackVars) > sizeOfStackVarTable) //ending stack index out of range
      {
        copyBytes = sizeOfStackVarTable - ScriptDebug_stackIndex; //number of bytes still in range
      }
      remBytes = sizeof(ScriptDebug_stackVars) - copyBytes; //remainder bytes
      
      if(copyBytes > 0)
        memcpy(&ScriptDebug_stackVars[0], &stackVariables[ScriptDebug_stackIndex], copyBytes);
      if(remBytes > 0)
        memset(&ScriptDebug_stackVars[copyBytes], 0, remBytes);

      //copy Global Table (set unused bytes to 0) - note: sizeGlobalVarTable is only size available to current script pointer   
      copyBytes = sizeof(ScriptDebug_globalVars);
      if(ScriptDebug_globalIndex >= sizeOfGlobalVarTable)  //starting stack index out of range
      {
        copyBytes = 0;
      }
      else if(ScriptDebug_globalIndex + sizeof(ScriptDebug_globalVars) > sizeOfGlobalVarTable) //ending stack index out of range
      {
        copyBytes = sizeOfGlobalVarTable - ScriptDebug_globalIndex; //number of bytes still in range
      }

      remBytes = sizeof(ScriptDebug_globalVars) - copyBytes; //remainder bytes
      
      if(copyBytes > 0)
        memcpy(&ScriptDebug_globalVars[0], &globalVariables[globalVarOffset[scriptPointer]+ScriptDebug_globalIndex], copyBytes);
      if(remBytes > 0)
        memset(&ScriptDebug_globalVars[copyBytes], 0, remBytes);
      
        
//      // stack vars copy
//      if ( sizeOfStackVarTable <= sizeof(ScriptDebug_stackVars))
//      {
//        for (i = 0; i < sizeOfStackVarTable; i++)
//          ScriptDebug_stackVars[i] = stackVariables[i];
//      }
//      else  //JML TODO: allow shift of starting index so that any stack variable can be monitored, not just first 32
//      {
//          for (i = 0; i < sizeof(ScriptDebug_stackVars); i++)
//            ScriptDebug_stackVars[i] = stackVariables[i];
//      }
//      // global vars copy
//      if ( (globalVarOffset[scriptPointer + 1] - globalVarOffset[scriptPointer]) <= sizeof(ScriptDebug_globalVars))
//      {
//        for (i = 0; i < globalVarOffset[scriptPointer + 1] - globalVarOffset[scriptPointer]; i++)
//          ScriptDebug_globalVars[i] = globalVariables[i + globalVarOffset[scriptPointer]];
//      }
//      else //JML TODO: allow shift of starting index so that any global variable can be monitored, not just first 32
//      {
//        for (i = 0; i < sizeof(ScriptDebug_globalVars); i++)
//          ScriptDebug_globalVars[i] = globalVariables[i + globalVarOffset[scriptPointer]];       
//      }
    } //end debug control

    
    currentOperationAddress = nextOperationAddress; 
    
  } 
//-----------------------------------------------------------------------//
//                 END OPCODE LOOP
//-----------------------------------------------------------------------//
  
  // reset debug -- except for current opcode
  if ((ScriptDebug_controlByte & 0x01) == 0x01 || (scriptOpCodeValue == 0xFF))
  {
    ScriptDebug_currentOpAddress = currentOperationAddress;
    ScriptDebug_currentOpcode = scriptOpCodeValue;
    ScriptDebug_currentOpVar0 = 0x00;
    ScriptDebug_currentOpVar1 = 0x00;
    ScriptDebug_currentOpVar2 = 0x00;
    ScriptDebug_currentOpVar3 = 0x00;
    ScriptDebug_currentOpVar4 = 0x00;
    ScriptDebug_currentResultVar = 0x00;
    ScriptDebug_statusByte = 0;
    ScriptDebug_CurrentExecutionNumber++;
  }
  
  if ( (ScriptDebug_controlByte & 0x08) == 0x08)
  {
    CPU_INT32U getTime = (CPU_INT32U)GetTimer1Count();
    
    if (getTime >= intervalTime) 
      ScriptDebug_timerResult = getTime - (CPU_INT32U)intervalTime;
    else                                    // rollover
      ScriptDebug_timerResult = ~getTime + 1 + (CPU_INT32U)intervalTime;
    
    ScriptDebug_controlByte &= ~( 1 << 3 ); // clears timer bit    
  }
  return SCRIPT_ERR_NO_ERROR;
  
} // end function


void setOperand(CPU_INT08U opScopeType, CPU_INT08U* pResultVar, CPU_INT32U opVar, CPU_INT08U len)
{ 
      
  switch (opScopeType & 0x0F)
  {
  case 2: //8 bit signed
    {
      *pResultVar = (CPU_INT08U)opVar;
      break;
    }
  case 3: //16 bit signed
    {
      *(pResultVar  ) = (CPU_INT08U) (opVar     ); 
      *(pResultVar+1) = (CPU_INT08U) (opVar >> 8);
      break;
    }
  case 4: //32 bit signed
    {
      *(pResultVar  ) = (CPU_INT08U) (opVar      ); 
      *(pResultVar + 1) = (CPU_INT08U) (opVar >>  8);
      *(pResultVar + 2) = (CPU_INT08U) (opVar >> 16);
      *(pResultVar + 3) = (CPU_INT08U) (opVar >> 24);
      break;
    }
  case 5: //8 bit unsigned
    {
      *pResultVar = (CPU_INT08U)opVar;
      break;
    }
  case 6: //16 bit unsigned
    {
      
      *(pResultVar    ) = (CPU_INT08U) (opVar     ); 
      *(pResultVar + 1) = (CPU_INT08U) (opVar >> 8);
      break;
    }
  case 7: //32 bit unsigned
    {
      *(pResultVar    ) = (CPU_INT08U) (opVar      ); 
      *(pResultVar + 1) = (CPU_INT08U) (opVar >>  8);
      *(pResultVar + 2) = (CPU_INT08U) (opVar >> 16);
      *(pResultVar + 3) = (CPU_INT08U) (opVar >> 24);
      break;
    }
  case 8:  //string
  case 10: //byte array
    {
      //Strings and Byte arrays are handled identically
      //first byte is length (not including self). remaining bytes are string or byte array
      *pResultVar = len*sizeof(CPU_INT08U);
      memcpy(pResultVar + 1, (CPU_INT08U* )opVar, len*sizeof(CPU_INT08U)); 
      break;
    }
  case 11: //fixedpoint
    {
      *(pResultVar    ) = (CPU_INT08U) (opVar      ); 
      *(pResultVar + 1) = (CPU_INT08U) (opVar >>  8);
      *(pResultVar + 2) = (CPU_INT08U) (opVar >> 16);
      *(pResultVar + 3) = (CPU_INT08U) (opVar >> 24);
      break;
    }
  default:
    {
      //not a valid variable type
      break;
    }
  }
}


CPU_BOOLEAN getOperand(CPU_INT08U opScopeType, CPU_INT08U* pSourceVar, CPU_INT32U* pOpVar, CPU_INT08S* pOpSignedType, CPU_INT08U* pOpVarSize, CPU_INT08U* pOpPointerType)
{
  CPU_BOOLEAN isImmediate = (opScopeType & 0x30) == 0;
  CPU_BOOLEAN isArray = FALSE;
  
  
  switch (opScopeType & 0x0F)
  {
  case 2: //8 bit signed
    {
      *pOpVar = *pSourceVar;
      *pOpSignedType = 1;
      *pOpVarSize = 1;
      *pOpPointerType = 0;
      break;
    }
  case 3: //16 bit signed
    {
      *pOpVar = (*(pSourceVar    )      ) + 
                (*(pSourceVar + 1) <<  8);
      *pOpSignedType = 2;
      *pOpVarSize = 2;
      *pOpPointerType = 0;
      break;
    }
  case 4: //32 bit signed
    {
      *pOpVar = (*(pSourceVar    )      ) + \
                (*(pSourceVar + 1) <<  8) + \
                (*(pSourceVar + 2) << 16) + \
                (*(pSourceVar + 3) << 24);	 
      *pOpSignedType = 4;
      *pOpVarSize = 4;
      *pOpPointerType = 0;
      break;
    }
  case 5: //8 bit unsigned
    {
      *pOpVar = *pSourceVar;
      *pOpVarSize = 1;
      *pOpSignedType = -1;
      *pOpPointerType = 0;
      break;
    }
  case 6: //16 bit unsigned
    {
      
      *pOpVar = (*(pSourceVar    )      )+ \
                (*(pSourceVar + 1) <<  8);
      
      
      if( isImmediate && (opScopeType & 0x80) && (*pOpVar == 0xFFFE) )
      {
        isArray = TRUE;
        *pOpPointerType = 1;
        //*pOpSignedType will get set outside of this function
        //*pOpVarSize will get set outside of this function
      }
      else
      {
        *pOpPointerType = 0;
        *pOpSignedType = -2;
        *pOpVarSize = 2;
      }
      break;
    }
  case 7: //32 bit unsigned
    {
      *pOpVar = (*(pSourceVar    )      ) + \
                (*(pSourceVar + 1) <<  8) + \
                (*(pSourceVar + 2) << 16) + \
                (*(pSourceVar + 3) << 24);
      *pOpSignedType = -4;
      *pOpVarSize = 4;
      *pOpPointerType = 0;
      break;
    }
  case 8:  //string
      //Strings and Byte arrays are handled identically, except that strings are assigned a special pointer type
      //for non-Immedaiates, size is first byte, and content starts in second byte.  
      //For Immediates, content starts immediately and size is 2 bytes fewer than the
      //than operand download size that is stored 2 bytes ahead of content
    if(isImmediate)
      {
        *pOpVar  = (CPU_INT32U) pSourceVar;  
        *pOpVarSize = (*(pSourceVar - 2)) - 2;     				
      }
      else
      {
        *pOpVar  = (CPU_INT32U) (pSourceVar + 1);  
        *pOpVarSize = *pSourceVar;    
      }
      *pOpPointerType = 2;
      //operandSignedType: no signed type (not used for numerical operations)
      break;
  case 10: //byte array
    {
      if(isImmediate)
      {
        *pOpVar  = (CPU_INT32U) pSourceVar;  
        *pOpVarSize = (*(pSourceVar - 2)) - 2;     				
      }
      else
      {
        *pOpVar  = (CPU_INT32U) (pSourceVar + 1);  
        *pOpVarSize = *pSourceVar;    
      }
      *pOpPointerType = 1;
      //operandSignedType: no signed type (not used for numerical operations)
      break;
    }
  case 11: //fixedpoint
    {
      *pOpVar = (*(pSourceVar    )      ) + \
                (*(pSourceVar + 1) <<  8) + \
                (*(pSourceVar + 2) << 16) + \
                (*(pSourceVar + 3) << 24);
      *pOpSignedType = 8;
      *pOpVarSize = 4;
      *pOpPointerType = 0;
      break;
    }
  default:
    {
      //not a valid variable type
      break;
    }
  }
  return isArray;
}


                                                          
/*
*********************************************************************************************************
*                                             IntNegToPos()
*
* Description : Converts a negative int to a normalized positive int
*
* Argument(s) : operandVar[i], operandSignedType, * isNegative
*
* Return(s)   : CPU_INT32U
*
*
*********************************************************************************************************
*/
CPU_INT32U IntNegToPos( CPU_INT32U operandVarW, CPU_INT08U signOfType, CPU_BOOLEAN * isNegative)
{
  CPU_INT32U answer = 0;
  
  if (signOfType == 1 && (operandVarW & 0x80) == 0x80)
  {          
    answer = MAX1 - operandVarW + 1;
    *isNegative = TRUE;
  }
  else if (signOfType == 2 && (operandVarW & 0x8000) == 0x8000)
  {          
    answer = MAX2  - operandVarW + 1;
    *isNegative = TRUE;
  }
  else if (signOfType == 4 && (operandVarW & 0x80000000) == 0x80000000)
  {          
    answer = MAX4  - operandVarW + 1;
    *isNegative = TRUE;
  }
  else if (signOfType == 8 && (operandVarW & 0x80000000) == 0x80000000) // fixed point (Q = 16.8 ==> to 24.8 or 32 bit)
  {          
    answer = MAX4  - operandVarW + 1;
    *isNegative = TRUE;
  }
  else
  {
    answer = operandVarW;
    *isNegative = FALSE; // make sure default is specified
  }
  
  return answer;
  
}    
/*
*********************************************************************************************************
*                                             ResultsVarTypeSaturate()
*
* Description : converts result to saturated value
*
* Argument(s) : results, signedType0, signedType1
*
* Return(s)   : CPU_INT32U
*
*
*********************************************************************************************************
*/
void ResultsVarTypeSaturate( CPU_INT32U * resultVar, CPU_INT08U varType, CPU_BOOLEAN overFlowOpcode, CPU_BOOLEAN saturateIsNeg )
{
  // normalize
  CPU_INT32U tempResult = *resultVar;

  switch (varType)
  {
    
    case 2: // Int8
      {
        if ( tempResult > 0xFF )
        {
          if(saturateIsNeg)
            *resultVar = 0x80;
          else
            *resultVar = 0x7F;
        }
        else if(tempResult > 0x7F & !saturateIsNeg)
          *resultVar = 0x7F;
        break;
      }
    case 3:  // Int16
      {
        if ( tempResult > 0xFFFF)
        {
          if(saturateIsNeg)
            *resultVar = 0x8000;
          else
            *resultVar = 0x7FFF;
        }
        else if(tempResult > 0x7FFF & !saturateIsNeg)
          *resultVar = 0x7FFF;
        break;
      }
    case 4:  // Int32
      {
        if(tempResult > 0x7FFFFFFF & !saturateIsNeg)
          *resultVar = 0x7FFFFFFF;
        break;
      }
    case 5: //uint8
      {
        if (tempResult > 0xFF)
        {
          if(saturateIsNeg)
            *resultVar = 0x00;
          else
            *resultVar = 0xFF;
        }
        break;
      }
    case 6://uint16
      {
        if (tempResult > 0xFFFF)
        {
           if(saturateIsNeg)
            *resultVar = 0x0000;
           else
            *resultVar = 0xFFFF;
        }
        break;
      }
    case 7: //uint32
      {
        //nothing to do
        break;
      }
  default :
    {
      //bad var type
      break;
    }
  
  }
}
/*
*********************************************************************************************************
*                                             getNetworkOperand()
*
* Description : Get data from Object Dictionary - local or remote
*
* Argument(s) : return data and len (reference) - len in bytes
*
* Return(s)   : none.
*
*
*********************************************************************************************************
*/
CPU_INT08U getNetworkOperand(CPU_INT08U opScopeType, CPU_INT08U *netAddress, CPU_INT32U *pOpVar, CPU_INT08S *pOpSignedType, CPU_INT08U *pOpVarSize , CPU_INT08U *pOpPointerType)
{
  
  unsigned long abortCode = 0;
  CPU_INT08U node = netAddress[2];
  CPU_INT16U index = netAddress[3] + (netAddress[4] << 8);
  CPU_INT08U subIndex = netAddress[5];
  CPU_INT08U numSubIndices = netAddress[6];
  CPU_INT08U size = 0;
  CPU_INT08U type = 0;

  CPU_INT32U sizeLocal = 0;
  //JML: the network data must be declared static in in order to allocate the space.  
  //Otherwise, when assigning a pointer to it, the data may be overwritten before it is copied.
  static CPU_INT08U data[MAX_GTWY_PKT_DATA] = {0};
  
  // define PKT header
  PACKET_HEADER  pkt;
  pkt.hbIndex = netAddress[4];
  pkt.lbIndex = netAddress[3];
  pkt.networkId = netAddress[1];
  pkt.nodeId = netAddress[2];
  pkt.subIndex = netAddress[5];
  pkt.dataLen = netAddress[6];
  
  
  //Radio or PM network
  if (netAddress[0] == 0x02) 
  {
    if (node == CT_NODE_ADDRESS) //read from CT not allowed from PM
    {
      abortCode = 9; //invalid Node setting
    }
    //PM local
    else if (node == getNodeId(&ObjDict_Data)) 
    {
      if(numSubIndices > 1)
      {
        GetBlockOD (&pkt, data, &size);
      }
      else
      {
        //JML: could use getLocalOD here for consistency
        abortCode = readLocalDict( &ObjDict_Data, index, subIndex, data,  &sizeLocal, &type, 0); 
        size = (CPU_INT08U) sizeLocal;
        
        if( type !=  (opScopeType & 0x0F) )
        {
          //mismatched designated type and read type.  Error here?
        } 
      }  
    }
    //PM Network (CAN)
    else
    {
      if(numSubIndices > 1)
      {
        pkt.protoCtrl = 0x30; //SDO block read
      }
      else
      {
        pkt.protoCtrl = 0x24; //SDO read
      }
      
      WaitUntilCANGatewayAvailable();
      
      if(runCANGateway(&pkt, data, &size))
      {
        //error response
        abortCode = 6;
      }
      else
      {       
        abortCode = 0;
      }
  
      MakeCANGatewayAvailable();    
    }
  }
  
  //PM Logging
  else if (netAddress[0] == 0x04)  
  {
    //Read from File here: not yet implemented
    //must set "size" and "data"
  }
  
  //not a valid port
  else
  {
    abortCode = 8;
  }
 
   if(abortCode == 0)
   {
         //JML: at this point we don't know whether the data coming back is a scalar or an array.  All we know is 
        //the designated type (e.g. UNS8).  If the number of bytes exceeds the type specified, assume it is an array
    
        if (size > sizeOfVarTypes[opScopeType & 0x0F] || numSubIndices > 1) // pointer type
        {
            *pOpVar = (CPU_INT32U)(&data[0]); // pass back the pointer instead of value
            
            if((opScopeType & 0x0F) == 0x08) //string
            {
              *pOpPointerType = 2;
            }
            else
            {
              *pOpPointerType = 1;
            }
            *pOpSignedType =  varSignedTypes[opScopeType & 0x0F];  
        }
        else // scalar type
        {
           memcpy(pOpVar , data, size*sizeof(CPU_INT08U) );
          *pOpPointerType = 0;
          *pOpSignedType =  varSignedTypes[opScopeType & 0x0F];
          
        }
        *pOpVarSize = size;
    }
    
  
  return (CPU_INT08U)abortCode;
}
   
/*
*********************************************************************************************************
*                                             SetNetworkData()
*
* Description : write to local and remote OD
*
* Argument(s) : Write data and use len (not a reference) - len in bytes, varConf provides type info
*
* Return(s)   : none.
*
*
*********************************************************************************************************
*/

CPU_INT08U setNetworkOperand(CPU_INT08U opScopeType, CPU_INT08U *netAddress, CPU_INT32U opVar,  CPU_INT08U opVarSize, CPU_BOOLEAN isPointer )
{
  unsigned long abortCode = 0;
  CPU_INT08U node = netAddress[2];
  CPU_INT16U index = netAddress[3] + (netAddress[4] << 8 );
  CPU_INT08U subIndex = netAddress[5];
  CPU_INT08U numSubIndices = netAddress[6];
  CPU_INT08U rxLen = 0;

  CPU_INT08U data[4];
  CPU_INT32U size = (CPU_INT32U)opVarSize;
  CPU_INT08U bytesPerSubindex;

  //Create a buffer that will hold the data that will be transmitted plus the packet header information
  CPU_INT08U buffer[SDO_MAX_LENGTH_TRANSFER+SIZE_PACKET_HEADER]; //max data length + length of packet header not including first data byte
  PACKET_HEADER* pkt = (PACKET_HEADER *)&buffer;
  pkt->networkId = netAddress[1];
  pkt->nodeId = netAddress[2];
  pkt->hbIndex = netAddress[4];
  pkt->lbIndex = netAddress[3];
  pkt->subIndex = netAddress[5];
  pkt->dataLen = opVarSize;
  
  
  if (isPointer)
  {
    if(opVarSize > SDO_MAX_LENGTH_TRANSFER)
      return 3;
    
    //copy all bytes of array, starting at address pointed to by opVar to packet
    memcpy(&pkt->data, (CPU_INT08U*)opVar, opVarSize); 
  }
  else //scalar
  {
    data[0] = (CPU_INT08U) opVar;
    data[1] = (CPU_INT08U)(opVar >> 8) ;
    data[2] = (CPU_INT08U)(opVar >> 16) ;
    data[3] = (CPU_INT08U)(opVar >> 24) ;
    
    //copy the data bytes contained in opVar to packet
    memcpy(&pkt->data, data, 4);
  }
  
  //Radio or PM network
  if (netAddress[0] == 0x02) 
  {
    //write to CT not allowed from PM
    if (node == CT_NODE_ADDRESS) 
    {
      abortCode = 9; //invalid Node setting
    }
    
    //PM local
    else if (node == getNodeId(&ObjDict_Data)) 
    {
      if(numSubIndices > 1)
      {
        if(numSubIndices > 0x3F)
        {
          return 33; //error: too many subindices
        }
          
        bytesPerSubindex = sizeOfVarTypes[opScopeType & 0x0F];
        switch(bytesPerSubindex)
        {
          case 1: pkt->dataLen = 0; 
                  break;
          case 2: pkt->dataLen = 1<<6; 
                  break;
          case 4: pkt->dataLen = 2<<6; 
                  break;
          default:  return 34; //error: invalid subindex length
        }
        pkt->dataLen |= numSubIndices;
        
        SetBlockOD (pkt, data, &rxLen); //data and rxLen contain response which is never used
      }
      else
      {
        //JML: could use setLocalOD here for consistency
        if (isPointer)
          abortCode = writeLocalDict( &ObjDict_Data, index, subIndex, (CPU_INT32U *)opVar, &size, 1); 
        else
          abortCode = writeLocalDict( &ObjDict_Data, index, subIndex, data, &size, 1); 
      }
    }
    
    //PM Network (CAN)
    else
    {
      if(numSubIndices > 1)
      {
        if(numSubIndices > 0x3F)
        {
          return 33; //error: too many subindices
        }
        pkt->protoCtrl = 0xB0; //SDO block write
      }
      else
      {
        pkt->protoCtrl = 0xA4; //SDO write
      }
      
      WaitUntilCANGatewayAvailable();
      
      if(runCANGateway(pkt, data, &rxLen)) // data and rxLen contain response and are unused below.  
      {
        abortCode = 6;
      }
      else
      {       
        abortCode = 0;
      }
      
      MakeCANGatewayAvailable();
    }
  }
    
  //PM logging
  else if (netAddress[0] == 0x04 && netAddress[2] == getNodeId(&ObjDict_Data ))  
  {   
       abortCode = WriteRecordToFile( pkt );         
  }
  
  else
  {
    abortCode = 31;
  }
  
  // SDO write error
  return abortCode;
    
}


/*
*********************************************************************************************************
*                                             StartScriptDebug( )
*
* Description : run sequence to start debugging
*
* Argument(s) : none
*
* Return(s)   : none.
*
*
*********************************************************************************************************
*/
void StartScriptDebug( CPU_INT08U scriptPointer, CPU_INT08U isButtonStarted )
{
  unsigned long abortCode = 0;
  CPU_INT08U type;
  OS_ERR err;
  CPU_INT32U varsize = 0;
  CPU_INT08U controlWord[4];
  
  ScriptDebug_controlByte = 0x03; // set debug mode
  ScriptDebug_statusByte = 0;
  ScriptDebug_CurrentExecutionNumber = 0;
  
  abortCode = readLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, controlWord, &varsize, &type, 0);
  if (abortCode)
  {
    ScriptDebug_statusByte = SCRIPT_ERR_READ_SCRIPT_CONTROL; 
    ScriptDebug_controlByte = 0x00;
    return;
  }
  
  if (controlWord[1] > 0 && isButtonStarted == 0) // check for valid script id
  {
    
    controlWord[0] |= (1 << 0); // set bit 0 to run - starts the interpreter
    controlWord[3] = 0;         // clear any errors
    
    varsize = 4;
    // now write it back with the run bit set to invoke the interpreter
    abortCode = writeLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, &controlWord[0], &varsize, 0);
    
    currentScriptDebug = scriptPointer;
  }
  else if (controlWord[1] > 0 && isButtonStarted == 1) // start from PDO
  {
     varsize = 4;
     controlWord[3] = 0;         // clear any errors
     controlWord[0] &= ~(1 << 0);// turn off the run bit - clear it just to be safe
     
    // now write it back 
    abortCode = writeLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, &controlWord[0], &varsize, 0);
    
    currentScriptDebug = scriptPointer;
  }
  else
  {
    ScriptDebug_controlByte |= (1 << 2); // escapes from debug mode and sets next opcode to 0xFF                                         
    // in case it actually scanned.
    ScriptDebug_statusByte = SCRIPT_ERR_EXIT_DEBUG;
    
    OSTimeDlyHMSM(0, 0, 0, 100,
                  OS_OPT_TIME_HMSM_STRICT, // give it time to finish executing.
                  &err);
    
    ScriptDebug_controlByte = 0x00;
    
  }
  
  if (abortCode)
  {
    ScriptDebug_statusByte = SCRIPT_ERR_WRITE_SCRIPT_CONTROL; 
    ScriptDebug_controlByte = 0x00;
  }
}

/*
*********************************************************************************************************
*                                             StopScriptDebug( )
*
* Description : run sequence to stop debugging
*
* Argument(s) : none
*
* Return(s)   : none.
*
*
*********************************************************************************************************
*/
void StopScriptDebug( void )
{
  CPU_INT32U abortCode = 0;
  CPU_INT08U type = 0;
  OS_ERR err;
  CPU_INT32U varsize = 0;
  CPU_INT08U controlWord[4];
  
  if (currentScriptDebug == 0)
    return;
  
  abortCode = readLocalDict( &ObjDict_Data, 0x1F51, currentScriptDebug, controlWord, &varsize, &type, 0);
  if (abortCode)
  {
    ScriptDebug_statusByte = SCRIPT_ERR_READ_SCRIPT_CONTROL; 
    return;
  }
   
  controlWord[0] &= ~( 1 << 0 ); // reset the run bit
  abortCode = writeLocalDict( &ObjDict_Data, 0x1F51, currentScriptDebug, controlWord, &varsize, 0);
  
  ScriptDebug_controlByte = 0x04; // escapes from debug mode and returns control to task
  
  OSTimeDlyHMSM(0, 0, 0, 500,
                  OS_OPT_TIME_HMSM_STRICT, // give it time to finish executing.
                  &err);
  
  currentScriptDebug = 0;
  
  if (abortCode)
    ScriptDebug_statusByte = SCRIPT_ERR_WRITE_SCRIPT_CONTROL;
    
}
/*
*********************************************************************************************************
*                                             SingleStepScriptDebug( )
*
* Description : clear single step bit -- debugging
*
* Argument(s) : none
*
* Return(s)   : none.
*
*
*********************************************************************************************************
*/
void SingleStepScriptDebug( void )
{
  ScriptDebug_controlByte &= ~(1 << 1);  
}

/*
*********************************************************************************************************
*                                             FindScriptSector( )
*
* Description : returns sector for the specified address.
* Argument(s) : Script Pointer
*
* Return(s)   : Script Address
*
*
*********************************************************************************************************
*/
CPU_INT08U FindScriptSector( CPU_INT32U scriptAddress )
{
  CPU_INT08U sector, sectorIndex;
  
  for ( sector = SECTOR_MIN; sector <= SECTOR_MAX; sector++) 
  { 
      sectorIndex = sector - SECTOR_MIN;
      if (scriptAddress >= sectorAddresses[sectorIndex] && scriptAddress < sectorAddresses[sectorIndex+1])  
      { 
        return sector; 
      } 
   } 
  
    return 0; //address does not fall within sectors available to scripts
}

/*
*********************************************************************************************************
*                                             FindScriptAddress( )
*
* Description : returns address in flash memory where the script is located.
*
* Argument(s) : Script Pointer
*
* Return(s)   : Script Address
*
*
*********************************************************************************************************
*/
CPU_INT32U FindScriptAddress( CPU_INT08U scriptPointer )
{
  // scripts begin with sector 17
  CPU_INT32U tempAddress = 0;
  
  if (scriptPointer == 0)
    tempAddress = 0xFFFFFFFF;
  
  //since only last script is long, starting address only depends on SCRIPT_SIZE
  else if (scriptPointer <= MAX_NUMBER_SCRIPTS )  
    tempAddress = SCRIPTS_BASE_ADDRESS + (scriptPointer - 1) * SCRIPT_SIZE; 
  
  else
    tempAddress = 0xFFFFFFFF;
  
  return tempAddress;
}
/*
*********************************************************************************************************
*                                             FindScriptPointer( )
*
* Description : clear single step bit -- debugging
*
* Argument(s) : Script ID
*
* Return(s)   : ScriptPointer
*
*
*********************************************************************************************************
*/
UNS8 FindScriptPointer( UNS8 scriptID )
{
  UNS8 subIndexSize = 0;
  int i;
  UNS8 type = 0;
  UNS32 varsize = 0;
  UNS8 pControlWord[4];
 
  readLocalDict( &ObjDict_Data, 0x1F51, 0, &subIndexSize, &varsize, &type, 0);
  
  for (i = 1; i <= subIndexSize; i++)
  {
    type = 0;
    varsize = 0;        
    
    // set up control information
    readLocalDict( &ObjDict_Data, 0x1F51, i, &pControlWord[0], &varsize, &type, 0);
    
    // test for match
    if ( pControlWord[1] == scriptID)
    {
      return i;
    }      
          
  }
  return 0;
}
/*
*********************************************************************************************************
*                                             PowerOfTen( )
*
* Description : helper function for square root
*
* Argument(s) : Script ID
*
* Return(s)   : ScriptPointer
*
*
*********************************************************************************************************
*/          
double PowerOfTen(int num)
{
  double rst = 1.0;
  if(num >= 0)
  {
    for(int i = 0; i < num ; i++)
    {
       rst *= 10.0;
    }
   }
   else
   {
     for(int i = 0; i < (0 - num ); i++)
     {
                rst *= 0.1;
     }
    }
            
    return rst;
}

CPU_INT32U getElementAsUint32(CPU_INT08S opSignedType, CPU_INT32U opVar, CPU_INT16U element, CPU_BOOLEAN* isNeg)
{
  CPU_INT08U* pByte = (CPU_INT08U *) opVar;
  CPU_INT32U tempOp;
  
  switch (opSignedType)
  {
    case 0:
    case -1:
    case 1:
      {
        tempOp = (CPU_INT32U)*(pByte + element);
        break;
      }
    case -2:
    case 2:
      {
        tempOp =  (CPU_INT32U)(*(pByte + element*2    )) +
                 ((CPU_INT32U)(*(pByte + element*2 + 1)) <<8); 
        break;
      }
    case -4:
    case 4:
      {
         tempOp =  (CPU_INT32U)(*(pByte + element*4    )) +
                  ((CPU_INT32U)(*(pByte + element*4 + 1)) <<8) +
                  ((CPU_INT32U)(*(pByte + element*4 + 2)) <<16) +
                  ((CPU_INT32U)(*(pByte + element*4 + 3)) <<24);
        break;
      }
  }
  tempOp = IntNegToPos(tempOp, opSignedType, isNeg);
  return tempOp;
}
        
double getElementAsDouble(CPU_INT08S opSignedType, CPU_INT32U opVar, CPU_INT16U element) 
{
        CPU_INT08U* pByte = (CPU_INT08U *) opVar;
        switch (opSignedType)
        {
          case 0:  //assume byte  
          case -1:  
            return (double)*(pByte + element); 
          case 1:
            return (double)(CPU_INT08S)*(pByte + element); 
          case -2:
            return (double)( (CPU_INT16U)(*(pByte + element*2    )) +
                            ((CPU_INT16U)(*(pByte + element*2 + 1)) <<8) );
          case 2:  
             return (double)( (CPU_INT16S)( (CPU_INT16U)(*(pByte + element*2    )) +
                                           ((CPU_INT16U)(*(pByte + element*2 + 1)) <<8))  );
          case -4:
            return (double)( (CPU_INT32U)(*(pByte + element*4    )) +
                            ((CPU_INT32U)(*(pByte + element*4 + 1)) <<8) +
                            ((CPU_INT32U)(*(pByte + element*4 + 2)) <<16) +
                            ((CPU_INT32U)(*(pByte + element*4 + 3)) <<24) );
          case 4:
            return (double)( (CPU_INT32S) ( (CPU_INT32U)(*(pByte + element*4    )) +
                                           ((CPU_INT32U)(*(pByte + element*4 + 1)) <<8) +
                                           ((CPU_INT32U)(*(pByte + element*4 + 2)) <<16) +
                                           ((CPU_INT32U)(*(pByte + element*4 + 3)) <<24))  );
        default:
          return 0;
        }
}

/*
*********************************************************************************************************
*                                             WriteNMTCmd()
*
* Description : write to command processor
*
* Argument(s) : Write data and use len (not a reference) - len in bytes, varConf provides type info
*
* Return(s)   : none.
*
*
*********************************************************************************************************
*/
CPU_INT08U WriteNMTCmd( CPU_INT32U node, CPU_INT32U command, CPU_INT32U param1, CPU_INT32U param2 )
{
  CPU_INT08U data[3];
  data[0] = (CPU_INT08U)command;
  data[1] = (CPU_INT08U)param1;
  data[2] = (CPU_INT08U)param2;

    if (node == 0) // local and remote
    {
      ProcessNMTLocalStateChange(&ObjDict_Data, data);
      masterSendNMTstateChange(&ObjDict_Data, node, &data[0]); 
    }
    else if ( node == (CPU_INT08U)getNodeId(&ObjDict_Data )) // local only
    {
      ProcessNMTLocalStateChange(&ObjDict_Data, data );
    }
    else if ( node != getNodeId(&ObjDict_Data) ) // remote only 
    {
       masterSendNMTstateChange(&ObjDict_Data, node, &data[0]);  
    }
       
    return 0; // non zero error
}

