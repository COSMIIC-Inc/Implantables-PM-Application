// Doxygen
/*!
** @file   scripts.h
** @author Coburn
** @date   5/14/2011
**
** @brief 
** @ingroup iotasks
**
*/
#ifndef SCRIPTINTERPRETER_H
#define SCRIPTINTERPRETER_H

#define SCRIPT_MAX_STRING SDO_MAX_LENGTH_TRANSFER

#define SCRIPT_ERR_NO_ERROR 0
#define SCRIPT_ERR_INVALID_OPCODE 1
#define SCRIPT_ERR_ARRAYINDEX 2
#define SCRIPT_ERR_ODSUBINDEX 3
//4 is unused
#define SCRIPT_ERR_RESULT_IS_IMMEDIATE 5
#define SCRIPT_ERR_RESULT_IS_CONSTANT 6
#define SCRIPT_ERR_TOO_MANY_STACK_VARIABLES 7
#define SCRIPT_ERR_DESTINATIONARRAY 8
#define SCRIPT_ERR_DESTINATIONSTRING 9
#define SCRIPT_ERR_JUMPOPERAND 10
#define SCRIPT_ERR_OPERAND_TYPE 11
#define SCRIPT_ERR_OPERAND_OUT_OF_RANGE 12
#define SCRIPT_ERR_SCALAR_TO_POINTER 13
#define SCRIPT_ERR_POINTER_TO_SCALAR 14
#define SCRIPT_ERR_INVALID_CHILD_SCRIPT 15
//16 is unused
#define SCRIPT_ERR_DIVIDEBYZERO 17
#define SCRIPT_ERR_GETNETWORKDATA 18
#define SCRIPT_ERR_SETNETWORKDATA 19
#define SCRIPT_ERR_MAX_STRING 20
#define SCRIPT_ERR_OPERAND_TYPE_MISMATCH 21
#define SCRIPT_ERR_READ_SCRIPT_CONTROL 22
#define SCRIPT_ERR_WRITE_SCRIPT_CONTROL 23
#define SCRIPT_ERR_RESET_GLOBALS 24
#define SCRIPT_ERR_ABORTED 25
#define SCRIPT_ERR_EXIT_DEBUG 26
#define SCRIPT_ZERO_POINTER 27
#define SCRIPT_INVALID_POINTER 28


#include "applicfg.h"


/* ----------------- APPLICATION GLOBALS ------------------ */
static OS_SEM  Script_Sem;
static CPU_INT16U blockCounter;


/*-------- PROTOTYPES ---------- */
extern CPU_INT08U RunScriptInterpreter( CPU_INT08U scriptPointer, CPU_INT08U * childScriptPointer);
void StartScriptDebug( CPU_INT08U scriptPointer, CPU_INT08U isPDOStarted );
void StopScriptDebug( void );
void SingleStepScriptDebug( void );
CPU_INT32U FindScriptAddress( CPU_INT08U scriptPointer );
CPU_INT08U FindScriptSector( CPU_INT32U scriptAddress );
#endif