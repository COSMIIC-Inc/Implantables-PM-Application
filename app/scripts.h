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
#ifndef SCRIPTS_H
#define SCRIPTS_H

#include "applicfg.h"

#define SCRIPTS_BASE_ADDRESS                    0x00030000 // note that extra room is included for the radio bootloader (8096 bytes + program size);
                                                   // begins with sector 9 (64Kb)
                                                   //Scripts start in sector 10 (8KB)
#define SCRIPT_SIZE                             2048
#define LONG_SCRIPT_SIZE                        7680
#define MAX_NUMBER_SCRIPTS                      25
#define MAX_NUMBER_CHILD_SCRIPTS                10
#define MAX_QUEUED_SCRIPTS                      10
#define ROUND_ROBIN_SKIP_QUEUE                  5 //don't add round robin scripts to queue if there are already 5 scripts queued 

#define ROUND_ROBIN_SCRIPT                      0x80 //must be greater than MAX_NUMBER_SCRIPTS. 

//#define GLOBAL_CONSTANTS_TABLE_ADDRESS          0x0003C800
#define GLOBAL_VAR_TABLE_SIZE                   400

#define SECTOR_SIZE             1024
#define RECORD_SIZE             24
#define MAX_FREQUENCY           31
#define SAVE_RESTORE_ADDRESS    0x0000 
#define SAVE_RESTORE_BUFF_SIZE  1024 //2*NV_BIN_PAGE_SIZE

#define SCRIPT_DELAY_TIME_MAX   10000 //JML changed from 150
#define SAVE_TO_FLASH_COUNTER   1000

// /////////////////////////////////////
//        Memory in RAM partitioning
// /////////////////////////////////////

#define RAM_LOAD_FLASH          0x4000

/* ----------------- APPLICATION GLOBALS ------------------ */
	
static CPU_INT16U blockCounter;
extern CPU_INT08U targetHSNode;
extern CPU_INT08U targetHSChannel;
extern CPU_INT08U globalVariables[GLOBAL_VAR_TABLE_SIZE];
extern CPU_INT16U globalVarOffset[MAX_NUMBER_SCRIPTS + 1];
extern CPU_INT08U commandByte;
extern OS_Q ScriptScheduler_Q;

/*-------- PROTOTYPES ---------- */
void Scripts_Init(void);
CPU_INT08U Start_Script(UNS8 scriptNumber);
CPU_INT08U Stop_Script(UNS8 scriptNumber);
CPU_INT08U RunOnceScript(UNS8 scriptNumber, UNS8 timed);
CPU_INT08U RunScriptImmediate(UNS8 scriptNumber);
void Scripts_Enabled( void);
void Scripts_Disabled( void );
void RunScriptTask(void);
CPU_INT08U ReadLocalFlashMemory(void);
CPU_INT08U ReadRemoteFlashMemory(void);
CPU_INT08U ReadRemoteRAMMemory(void);
CPU_INT08U WriteLocalFlashMemory(void);    
CPU_INT08U WriteRemoteFlashMemory(void);
CPU_INT08U WriteRemoteRAMMemory(void);
CPU_INT08U ReadLocalRAMMemory(void);
void RunHighSpeedCollection(CPU_INT08U skipTime);
void EraseScriptSegment(CPU_INT08U scriptNumber);
UNS8 LoadScriptToFlash(UNS8 scriptPointer, UNS8 * data, UNS16 dataLen, UNS8 counter);
void WriteRadioConfig( void );
CPU_INT08U SaveValues ( void );
CPU_INT08U RestoreValues ( void );
void ResetToODDefault(void);
void Reset_Module(void);
void RunHighSpeedPassThru(UNS8 Param1);
CPU_INT08U LoadGlobalVarTable (CPU_INT08U scriptPointer);
CPU_INT08U AbortAllScripts( CPU_BOOLEAN abortInterpreter);
CPU_INT08U RunScriptMultiple(CPU_INT32U decodeValue);  
CPU_INT08U RunScriptMultiple(CPU_INT32U decodeValue);  
CPU_INT08U GetNodeTable(CPU_INT08U * nodeTable);
CPU_INT08U ClearLogfile(UNS8 logFileNumber);
CPU_INT08U AddScriptToQueue(CPU_INT08U ScriptPointer);
void EnableTPDOs(UNS8 mode);
void ReadMemoryWithIncrement(UNS8 memSelect);
UNS16 calculateScriptCRC16(UNS8 scriptPointer);
UNS16 readScriptCRC(UNS8 scriptPointer);
UNS16 readScriptLength(UNS8 scriptPointer);
UNS8 readScriptID(UNS8 scriptPointer);
UNS16 readScriptRev(UNS8 scriptPointer);

#endif