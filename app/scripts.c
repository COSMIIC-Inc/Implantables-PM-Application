
// Doxygen
/*!
** @file   scripts.c
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
#include "SPI_Memory.h"
#include "cc1101radio.h"
#include "cpuFlash.h"
#include "ScriptInterpreter.h"
#include "NMTmaster.h"
#include "File_Operations.h"
/******************************************************************************************************
*                                         Defines
*******************************************************************************************************/


#define NV_BIN_PAGE_SIZE		512
/*******************************************************************************************************
*                                         Globals
********************************************************************************************************/

CPU_INT08U targetHSNode = 0;
CPU_INT08U targetHSChannel = 0;
CPU_INT08U globalVariables[GLOBAL_VAR_TABLE_SIZE];
CPU_INT16U globalVarOffset[MAX_NUMBER_SCRIPTS + 1];
CPU_INT32U lastRequestedAddress = 0;
CPU_INT08U commandByte = 0;

CPU_INT08U scriptInit = FALSE;

/*
*********************************************************************************************************
*                                             InitScripts()
*
* Description : 
*
* Argument(s) : scriptNumber must be between 1 and 15
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
void Scripts_Init(void)
{
  
  if(LoadGlobalVarTable( 0 ))
  {
    Scripts_Disabled();
    ScriptDebug_statusByte = SCRIPT_ERR_RESET_GLOBALS;
  }
  else
  {
    Scripts_Enabled(); 
  }
  
    // check for valid execute time            
  if (Control_ScriptDelayTime == 0 || Control_ScriptDelayTime > SCRIPT_DELAY_TIME_MAX)
    Control_ScriptDelayTime = SCRIPT_DEFAULT_TIME ;
  
  AbortAllScripts(FALSE); //reset the run bits and error codes for all scripts, but don't abort interpreter
    
  scriptInit = TRUE;
  // now run startup script
  if (Control_StartUp_ScriptPointer  > 0 && Control_SystemControl && 0x0100 == 0x0100) // check for valid scriptID
  { 
      AddScriptToQueue( Control_StartUp_ScriptPointer );
  }
 }

CPU_INT08U AddScriptToQueue(CPU_INT08U scriptPtr)
{
  OS_ERR err;
  CPU_INT32U varsize = 0;
  CPU_INT08U type = 0;
  CPU_INT08U controlWord[4];
  static CPU_INT08U scriptQ = 0;

  
  if( !scriptInit )
    return 5; 
  if(!(Control_SystemControl & BIT4)) //If scripts are not enabled
    return 7;
    
  //RoundRobin Script Pointer will be determined by Script Task
  //Other script pointers should be vaidated here
  //roundrobinscripts won't be queued if there are not many slots left
  if (scriptPtr == ROUND_ROBIN_SCRIPT)
  {
    if(ScriptScheduler_Q.MsgQ.NbrEntries >=  ROUND_ROBIN_SKIP_QUEUE)
    {
      Status_ScriptSkipCounter++;
      return 4;
    }
  }
  else  //not a roundRobin Script
  {
    if (scriptPtr > MAX_NUMBER_SCRIPTS || scriptPtr == 0)
      return 1; //error: scriptPtr out of expected range
      
    readLocalDict( &ObjDict_Data, 0x1F51, scriptPtr, controlWord, &varsize, &type, 0);
    if (controlWord[1] == 0) 
      return 2; //error: no scriptID attached to scriptPtr
  }
  
  scriptQ = scriptPtr;

  //trigger Script Task to run next script in queue
  OSQPost(&ScriptScheduler_Q, &scriptQ, sizeof(scriptQ), OS_OPT_POST_FIFO, &err); 
  if (err == OS_ERR_MSG_POOL_EMPTY)
  {
    return 3; //error: too many scripts queued
  }
  
  if (err != OS_ERR_NONE)
  {
    return 6;
  }
  
  return 0;
}


 
  /*
  *********************************************************************************************************
  *                                             ClearLogFile()
  *
  * Description : This level of indirection is supported here to maintain parallelism with CT.
  *
  * Argument(s) : Log file number must be between 0 and MAX; 0 is all files
  *
  * Return(s)   : error code if no script.
  *
  *********************************************************************************************************
  */
 CPU_INT08U ClearLogfile(UNS8 logFileNumber)
 {  
   return FlushFile(logFileNumber);
 }
 
/*
*********************************************************************************************************
*                                             Start_Script()
*
* Description : starts round robin
*
* Argument(s) : scriptNumber must be between 1 and MAX_SCRIPT
*
* Return(s)   : error code if no script.
*
*********************************************************************************************************
*/
CPU_INT08U Start_Script(UNS8 scriptNumber)
{
  
  UNS8 type;
  UNS32 varsize = 1;
  UNS8 maxScript = 0;
  UNS8 pControlWord[4];
 
  /* check access is 0 */
  
  readLocalDict( &ObjDict_Data, 0x1F51, 0, &maxScript, &varsize, &type, 0);
  if (scriptNumber > 0 && scriptNumber <= maxScript)
  {   
    varsize = 4;
    readLocalDict( &ObjDict_Data, 0x1F51, scriptNumber, &pControlWord[0], &varsize, &type, 0);
    if (pControlWord[1] == 0) // if no script
      return 19;
    pControlWord[0] |= (1 << 1); // set bit 1 for continuous runnnig
    pControlWord[3] = 0;
    writeLocalDict( &ObjDict_Data, 0x1F51, scriptNumber, &pControlWord[0], &varsize, 0);
    
  }
  else
  {
    return 19;
  }
  return 0;
}

/*
*********************************************************************************************************
*                                             Stop_Script()
*
* Description : stops round robin
*
* Argument(s) : scriptNumber must be between 1 and 4
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U Stop_Script(UNS8 scriptNumber)
{
  UNS8 type;
  UNS32 varsize = 1;
  UNS8 maxScript = 0;
  UNS8 pControlWord[4];
  /* check access is 0 */
  
  readLocalDict( &ObjDict_Data, 0x1F51, 0, &maxScript, &varsize, &type, 0);
  if (scriptNumber > 0 && scriptNumber <= maxScript)
  {
    varsize = 4;
    readLocalDict( &ObjDict_Data, 0x1F51, scriptNumber, &pControlWord[0], &varsize, &type, 0);
    if (pControlWord[1] == 0) // if no script
      return 19;
    pControlWord[3] = 0;
    pControlWord[0] &= ~(1 << 1);
    writeLocalDict( &ObjDict_Data, 0x1F51, scriptNumber, &pControlWord[0], &varsize, 0);
    
  }
  else
  {
    return 19;
  }
   
 return 0;
}
/*
*********************************************************************************************************
*                                             RunOnceScript()
*
* Description : runs a script once.
*
* Argument(s) : scriptNumber must be between 1 and MAX_SCRIPT
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U RunOnceScript(UNS8 scriptNumber, UNS8 timed)
{
 
  UNS8 type;
  UNS32 varsize = 1;
  UNS8 maxScript = 0;
  UNS8 pControlWord[4];
  /* check access is 0 */
  
  if (timed)
  {
    ScriptDebug_controlByte |= 0x08;
    ScriptDebug_timerResult = 0;
  }
  
  readLocalDict( &ObjDict_Data, 0x1F51, 0, &maxScript, &varsize, &type, 0);
  if (scriptNumber > 0 && scriptNumber <= maxScript)
  {
    varsize = 4;
    readLocalDict( &ObjDict_Data, 0x1F51, scriptNumber, &pControlWord[0], &varsize, &type, 0);
    if (pControlWord[1] == 0) // if no script
      return 19;
    pControlWord[3] = 0;
    pControlWord[0] |= (1 << 0);
    writeLocalDict( &ObjDict_Data, 0x1F51, scriptNumber, &pControlWord[0], &varsize, 0);
    
  }
  else
  {
    return 19;
  }
   return 0;
}
/*
*********************************************************************************************************
*                                             AbortAllScripts()
*
* Description : stops all running scripts. also stops interpreter
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U AbortAllScripts( CPU_BOOLEAN abortInterpreter )
{
  CPU_INT08U i = 0;
  CPU_INT08U pControlWord[4];
  CPU_INT32U varsize;
  UNS8 type;
  CPU_INT32U abortCode = 0;
  
  for (i = 1; i <= MAX_NUMBER_SCRIPTS; i++)
  {
    type = 0;
    varsize = 0;
    abortCode = readLocalDict( &ObjDict_Data, 0x1F51, i, &pControlWord[0], &varsize, &type, 0);
    if ( abortCode == 0 && pControlWord[1] != 0) // check for script ID
    {
      // cancel run bits and clear error word
      pControlWord[0] = 0;
      pControlWord[3] = 0;
      writeLocalDict( &ObjDict_Data, 0x1F51, i, &pControlWord[0], &varsize, 0);
    }    
  }
    // abort interpreter
  if(abortInterpreter)
    ScriptDebug_controlByte |= 0x80;
  
  return 0;
}
/*
*********************************************************************************************************
*                                             RunScriptImmediate()
*
* Description : runs a script immediately.
*
* Argument(s) : scriptNumber must be between 1 and MAX_SCRIPT
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U RunScriptImmediate(UNS8 scriptNumber)
{
  
  
  
  return 0;
  
}

/*
*********************************************************************************************************
*                                             RunScriptMultiple()
*
* Description : decodes operand to start/stop script.
*
* Argument(s) : 32 bit unsigned operand
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U RunScriptMultiple(CPU_INT32U decodeValue)  
{

  UNS8 type, pointer;
  UNS32 varsize = 1;
  UNS8 maxScript = 0;
  UNS8 pControlWord[4];
 
  /* check access is 0 */
  
  readLocalDict( &ObjDict_Data, 0x1F51, 0, &maxScript, &varsize, &type, 0);
    
    for ( pointer = 1; pointer <= maxScript; pointer++)
    {
      varsize = 4;
      type = 0;
      readLocalDict( &ObjDict_Data, 0x1F51, pointer, &pControlWord[0], &varsize, &type, 0);
      if (pControlWord[1] != 0) // if script
      {
        if ( decodeValue & (1 << (pointer - 1)) )
        {
          pControlWord[0] |= (1 << 1); // set bit 1 for continuous running
        }
        else
        {
          pControlWord[0] &= ~(1 << 1); // reset bit 1 for stop
        }
        pControlWord[3] = 0;
        writeLocalDict( &ObjDict_Data, 0x1F51, pointer, &pControlWord[0], &varsize, 0);
      }
    }
  
  return 0;
}

/*
*********************************************************************************************************
*                                             Scripts_Enabled()
*
* Description : Sets the script enabled bit that allows the script task to call the interpreter.
*
* Argument(s) : 
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
void Scripts_Enabled( void )
{
  Control_SystemControl |= (1 << 4);
  //IO0CLR = BIT0; //JML DEBUG - See IOInit in app.c for debug usage
}

/*
*********************************************************************************************************
*                                             Scripts_Disabled()
*
* Description : resets the script enabled bit that stops the script task from calling the interpreter.
*
* Argument(s) : Script identifier or default (0x00)
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
void Scripts_Disabled( void )
{
  AbortAllScripts(FALSE); //reset the run bits and error codes for all scripts but don't abort interpreter
  Control_SystemControl &= ~(1 << 4);
  //IO0SET = BIT0; //JML DEBUG - See IOInit in app.c for debug usage
}
/*
*********************************************************************************************************
*                                             LoadGlobalVarTable()
*
* Description : Copies global var table from flash to operating memory to initialize. Note - script pointer is 
*               1 - indexed, so first entry in array is skipped.
*
* Argument(s) : Script Pointer - if zero, load everything
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U LoadGlobalVarTable (CPU_INT08U scriptPointer)
{
  CPU_INT16U sizeOfCurrentGlobalVarTable = 0;
  CPU_INT32U currentGlobalVarTableAddress = 0;
  CPU_INT08U pControlWord[4];
  CPU_INT08U i = 0; //index for number of scripts (<256)
  CPU_INT16U  k = 0; //index for variables within scripts (may be more than 256)
  CPU_INT32U abortCode = 0;
  CPU_INT32U startOfScriptAddress = 0;
  CPU_INT16U counter = 0; //(may be more than 256)
  UNS8 type = 0;
  UNS32 varsize = 0;
 
  // zero out offset table
  for (i = 0; i < MAX_NUMBER_SCRIPTS + 1; i++)
    globalVarOffset[i] = 0;
   
  Scripts_Disabled();
  for (i = 1; i < MAX_NUMBER_SCRIPTS; i++)
  {
    type = 0;
    varsize = 0;
    abortCode = readLocalDict( &ObjDict_Data, 0x1F51, i, &pControlWord[0], &varsize, &type, 0);
    if ( abortCode == 0 && pControlWord[1] != 0) // check for script ID
    {
      
      startOfScriptAddress = FindScriptAddress( i );
      
      sizeOfCurrentGlobalVarTable = (*(CPU_INT08U * )(startOfScriptAddress + 4) + *(CPU_INT08U * )(startOfScriptAddress + 5) * 256) \
      - ((*(CPU_INT08U * )(startOfScriptAddress + 2) + *(CPU_INT08U * )(startOfScriptAddress + 3) * 256)); // size of globalVarTable
      
      if (sizeOfCurrentGlobalVarTable > (GLOBAL_VAR_TABLE_SIZE - counter) ) // check for room
        return SCRIPT_ERR_RESET_GLOBALS;

      // pointer to start of next seqment (increment from previous by size)
      globalVarOffset[i + 1] = globalVarOffset[i] + sizeOfCurrentGlobalVarTable ;
      
      currentGlobalVarTableAddress = startOfScriptAddress +  *(CPU_INT08U * )(startOfScriptAddress + 2) + *(CPU_INT08U * )(startOfScriptAddress + 3) * 256;
      
      // now load table - don't load unless 0 or indicated scriptPointer  
      if (  scriptPointer == 0 || i == scriptPointer)  // if loading selected scriptpointer > 0
      {
        counter = globalVarOffset[i]; // initialize based on last instance     
        for ( k = 0; k < sizeOfCurrentGlobalVarTable; k++)
        {        
          globalVariables[counter] = *(CPU_INT08U * )(currentGlobalVarTableAddress + k); // address in NV memory - copy to RAM         
          counter++;
        }
      }
     
    }    
    else
    {
      globalVarOffset[i + 1] = globalVarOffset[i];
    }
  }
  Scripts_Enabled();
  return 0; // ok
}

/*
*********************************************************************************************************
*                                             RunScriptTask()
*
* Description : Runs the scheduler as a task in the OS. The scheduler uses a simple round robin - it iterates
*               through the task list (stored on OD 1F51 ) looking for the run bit (bit 0) to be set and the
*               frequency counter to match the frequency mask (stored as task entry in OD 1F51). Each entry
*               - referenced as the subindex - contains the ControlWord (a 32bit uint). Task base
*               addressing is determined by its OD subindex. The interpreter receives that information
*               in addition to the ControlWord runs the script and exits. It is important to note that
*               the assumption of the scheduler is that all scripts exit and get queued again.
*               Note that scripts that are used as single run (event scripts) will reset any frequency info.
*               
*               Bit 0 = 1 RunOnce - cleared automatically
*               Bit 1 = 1 Run continuous; 0 Halt
*               Bit 2 = 1 Reload global table (used with either run bit and first time for continuous.
*               Bit 3 - 7: not used
*               Bits 8 - 15 script ID
*               Bit 24 - 30 Error code  
*               Bit 31 1 = run once completed and successful execution
*
*               Script execution order can be modified by changing the values in the array 1F56.1
*
*              
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
void RunScriptTask(void)
{ 
  UNS32 intervalTime = 0;
  UNS32 getTime = 0;
  //UNS8 subIndexSize = 0;
  //int i,j;
  UNS8 type = 0;
  UNS32 varsize = 0;
  UNS8 controlWord[4];
  OS_ERR err;
  //UNS32 control = 0;
  UNS8 scriptErr = 0;
  void *p_msg;
  CPU_INT08U scriptPointer = 0;
  CPU_INT08U childScriptPointer = 0;
  CPU_INT08U childScriptCounter;
  static CPU_INT08U roundRobinIndex = 0;
  CPU_TS ts;
  CPU_BOOLEAN isRoundRobin;
  OS_MSG_SIZE msg_size;
  
  while ( TRUE ) 
  {
      memset(controlWord, 0, sizeof(controlWord)); //init controlWord JML...probably unnecessary
      
      // wait on Semaphore 
      //Note that this is a counting semaphore and may be nonzero before, 
      //i.e. several scripts may be queued
      BatteryControl_LowPowerStatus |= BIT5; //ScriptTask is pending
      p_msg = OSQPend(&ScriptScheduler_Q, 0, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err); 
      if (err == OS_ERR_NONE)
      {
        scriptPointer = ((CPU_INT08U*) p_msg)[0];
        if (scriptPointer == 0) 
        {
          asm("nop");
          continue;
        }
      }
      else
      {
        asm("nop");
        continue;
      }
        
      BatteryControl_LowPowerStatus &=~ BIT5; //ScriptTask is not pending
      
      
        
      
      // get start time (count)
      intervalTime = GetTimer1Count();
      
      isRoundRobin = (scriptPointer == ROUND_ROBIN_SCRIPT);
      
      //run the next script in the script queue
      if(isRoundRobin) //round robin script-script pointer not specified yet
      {
        // Note that ScriptOrder array is zero based but the scriptpointers are one based. 
        //Find next valid scriptPointer that is set to run
        while (roundRobinIndex < MAX_NUMBER_SCRIPTS)
        {
          scriptPointer = Script_Order[roundRobinIndex];
          
          if (scriptPointer  == 0 || scriptPointer > MAX_NUMBER_SCRIPTS) //not a valid script pointer 
          {
            roundRobinIndex++;
            continue; //go to next script in RoundRobin
          }

          // get controlWord for script
          type = 0;
          varsize = 0;     
          readLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, controlWord, &varsize, &type, 0);
          
          //script is not set to RUN or RUNONCE or script has no valid ID attached
          if ( (controlWord[0] & 0x03) == 0x00 || controlWord[1] == 0)   
          {
            roundRobinIndex++;
            continue; //go to next script in RoundRobin
          }
          else
          {
            //the script is valid: break from Round Robin while loop and execute script
            break;
          }
        }
        if (roundRobinIndex == MAX_NUMBER_SCRIPTS) //no active round robin scripts
        {
          roundRobinIndex = 0;
          scriptPointer = 0;
          //ShiftScriptQueue(); 
          continue; //back to Pend
        }
      }
      else //script from startup, PDO, or RTC alarm
      {
        //scriptPointer has  already been verified by AddScriptToQueue  
        //but we still need to initialize controlWord here
       
        type = 0;
        varsize = 0;     
        readLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, controlWord, &varsize, &type, 0);
      }
           
      //now run main script and child scripts
      childScriptCounter = MAX_NUMBER_CHILD_SCRIPTS;
      
      while (childScriptCounter--)
      {
        //If scripts are enabled, run interpreter.  note: scripts aren't queued if scripts are disabled, so this may not be necessary
        if(Control_SystemControl & BIT4) 
        {
          if(ScriptDebug_Indication & BIT0)  { IO0SET = BIT1; }//JML DEBUG - See IOInit in app.c for debug usage
          if(scriptPointer == 0)
            asm("nop");
          
          scriptErr = RunScriptInterpreter(scriptPointer, &childScriptPointer);
          
          if(ScriptDebug_Indication & BIT0) {  IO0CLR = BIT1; } //JML DEBUG - See IOInit in app.c for debug usage
        }
        
        controlWord[3] = scriptErr;
        
        if (controlWord[0] & BIT0) //Run Once
        {
          controlWord[0] = 0;  // reset start bit
          
          //For RUNONCE, if no error, bit is set in controlWord to show that it has completed
          if (scriptErr == 0)
           {
              controlWord[3] |= 1 << 7; //runonce successful
              varsize = 4;
              writeLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, controlWord, &varsize, 0);
           }
          
        }

        if (scriptErr > 0)
        {
          controlWord[0] = 0; // reset start bit
          ScriptDebug_statusByte = scriptErr;
          varsize = 4;
          writeLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, controlWord, &varsize, 0);
        }
        
        if(childScriptPointer == 0) //no more child scripts
          break;
        else
          scriptPointer = childScriptPointer; 
      } 

      

      
      
      // now calculate actual script+children script execution time
      getTime = (CPU_INT32U)GetTimer1Count();    
      if (getTime >= intervalTime) 
        Status_ScriptsScanTime = getTime - (CPU_INT32U)intervalTime;
      else                                    // rollover
        Status_ScriptsScanTime = ~getTime + 1 + (CPU_INT32U)intervalTime;
      
            

      if(isRoundRobin) 
      {
        roundRobinIndex++; //increment for next pend
        if(roundRobinIndex >= MAX_NUMBER_SCRIPTS)
        {
          roundRobinIndex = 0; //reset and wait for next Script Interval
        }
        else
        {
          // add some bandwidth for other tasks
          OSTimeDlyHMSM(0, 0, 0, 2,  OS_OPT_TIME_HMSM_STRICT, &err); 
      
          AddScriptToQueue(ROUND_ROBIN_SCRIPT); 
        }
      }
      
  } //end infinite while
} // end function



//bytes 0:1 of script download
UNS16 readScriptLength(UNS8 scriptPointer)
{
  if (scriptPointer > MAX_NUMBER_SCRIPTS || scriptPointer == 0)
       return 0;
  
   UNS32 scriptBaseAddress = FindScriptAddress( scriptPointer );
   UNS8 lenLB = *(UNS8*) scriptBaseAddress;
   UNS8 lenHB = *(UNS8*) (scriptBaseAddress + 1);
   UNS16 len = (UNS16)lenLB + ((UNS16)lenHB << 8);
   
    if(len == 0  || len > LONG_SCRIPT_SIZE || (scriptPointer < 25 && len > SCRIPT_SIZE))
     return 0;
   
   return(len);
}

//byte D-1 of script download where D = scriptlength 
UNS8 readScriptID(UNS8 scriptPointer)
{

   UNS16 len = readScriptLength(scriptPointer);
   if(len == 0)
     return 0;
   
  UNS32 scriptBaseAddress = FindScriptAddress( scriptPointer );
       
   return(*(UNS8*)(scriptBaseAddress + len - 1)); 
}

//bytes 8:9 of script download
UNS16 readScriptRev(UNS8 scriptPointer)
{
   UNS16 len = readScriptLength(scriptPointer);
   if(len == 0)
     return 0;
   
   UNS32 scriptBaseAddress = FindScriptAddress( scriptPointer );
   
   UNS8 revLB = *(UNS8*)(scriptBaseAddress + 8); 
   UNS8 revHB = *(UNS8*)(scriptBaseAddress + 9); 
   return(((UNS16)revLB + ((UNS16)revHB << 8)));
}

//bytes D:D+1 of script download where D = scriptlength 
UNS16 readScriptCRC(UNS8 scriptPointer)
{
   UNS16 len = readScriptLength(scriptPointer);
   if(len == 0)
     return 0;
   
   UNS32 scriptBaseAddress = FindScriptAddress( scriptPointer );
   UNS8 crcLB = *(UNS8*)(scriptBaseAddress + len); 
   UNS8 crcHB = *(UNS8*)(scriptBaseAddress + len + 1); 
   return(((UNS16)crcLB + ((UNS16)crcHB << 8)));
}


UNS16 calculateScriptCRC16(UNS8 scriptPointer)
{
   //This takes ~0.5ms/KB.  
   //Max 1ms for scripts 1-24, 3.75ms for script 25
   

   UNS16 x; 
   UNS16 crc = 0xFFFF; 

  IO0SET = BIT0; //JML Debug for timing measurment
    
   UNS16 len = readScriptLength( scriptPointer );
   if(len == 0)
     return 0;
   
   UNS32 scriptBaseAddress = FindScriptAddress( scriptPointer );
   UNS8 *data = (UNS8 *)scriptBaseAddress; //point to beginnning of script
    
   while(len--) 
   { 
      x = (crc >> 8) ^ *data++; 
      x ^= x>>4; 
      
      crc = (crc << 8) ^ (x << 12) ^ (x <<5) ^ x; 
   } 
   
   IO0CLR = BIT0; //JML Debug for timing measurment
   return crc; 
}

/*
*********************************************************************************************************
*                                             LoadScriptToFlash
*
* Description : Message Address is the method used to piece together multiple messages into a single 
*               download image. The image is assembled in scriptBuffer.
* 
* 
*
* Argument(s) : ScriptPointer, Counter
*
* Return(s)   : status.
*
*
*             : THIS CODE IS VERY DIFFERENT THAN CT FLASH DOWNLOAD. DO NOT REPLACE WITH IT!
*********************************************************************************************************
* returns:
* 0x00: no error
* 0x01:unused
* 0x02:script pointer invalid (either 0 or >  MAX_NUMBER_SCRIPTS)
* 0x03:no message address, downlaod must be at leat 2 bytes
* 0x04:could not reset script control word
* 0x05:script size too big
* 0x06:packet out of sequence
* 0x07:bad script ID read from Flash
* 0x08:could not set script pointer
* 0x09:could not load global variables
* 0x80 + err: WriteSectorNvDataMultiple
*/
#define  DATA_MESSAGE_SIZE   32

UNS8 LoadScriptToFlash(UNS8 scriptPointer, UNS8 * data, UNS16 dataLen, UNS8 counter)
{ 
  CPU_INT08U packet[DATA_MESSAGE_SIZE];
  CPU_INT16U messageAddress = 0;
  CPU_INT08U messageSize = 0;  
  static CPU_INT32U scriptBaseAddress = 0;   //absolute address of scrip in flash
  static CPU_INT32U scriptAddress = 0; //address of script within sector
  static CPU_INT16U scriptSize = 0;
  static CPU_INT32U indexPacket = 0;
  static CPU_INT08U counterPrevious = 0;
  
  CPU_BOOLEAN lastPacket = FALSE;
  CPU_BOOLEAN firstPacket = FALSE;
  static CPU_INT08U sector = 0;

  CPU_INT08U status = 0; // 0 = ok.
  
  CPU_INT32U pControlWord = 0;
  CPU_INT32U varsize = 4;
  
  OS_ERR err;
 
  ScriptDebug_statusByte = 0; //reset status byte
  // stop execution of scripts
  Scripts_Disabled();
  
  // check for legal script pointer
  if (scriptPointer > MAX_NUMBER_SCRIPTS || scriptPointer == 0)
    return 2; //error:script pointer invalid
  if (dataLen < 2)
    return 3; //error:no MessageAddress
    
    
    memset(packet, 0xFF, sizeof(packet)); //initialize packet
    messageAddress = (CPU_INT16U)(data[0] + 256 * data[1]);
    messageSize = dataLen - 2;
    
    if (messageAddress == 0) //first packet of script
    {
      //set the script control word to 0
      UNS32 pZeroWord = 0; 
      if(writeLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, &pZeroWord, &varsize, 0))
      {
        scriptSize = 0;
        return 4; //error:could not reset script control word
      }
      
      if(messageSize > 0)
      {
        scriptSize = data[2] + data[3] * 256; // first two bytes of download file  
        if((scriptSize > SCRIPT_SIZE) && scriptPointer < 25 || scriptSize > LONG_SCRIPT_SIZE )
        {
          scriptSize = 0;
          return 5; //error:script too big
        }
      }
      else
        scriptSize = 0;
      
      //initialize static variables
      // calculate sector and address based on pointer
      scriptBaseAddress = FindScriptAddress( scriptPointer );
      sector = FindScriptSector ( scriptBaseAddress );
      scriptAddress = scriptBaseAddress - sectorAddresses[sector - SECTOR_MIN];

      indexPacket = scriptAddress; //initialize indexPacket
      
      firstPacket = TRUE;
    }
    else if (counter != counterPrevious - 1) 
    {
      scriptSize = 0;
      return 6; //error:packet out of sequence
    }
    
    if(messageSize > 0)
    {
      memcpy(packet, &data[2], messageSize); //first two bytes of data are messageAddress, don't include in packet
    }
   
    //Write the packet to NV memory process.  If the last message has been sent, keep writing packets 
    //until script space is full.
    do
    {
        if(scriptPointer == 25)
        {
          if(indexPacket - scriptAddress + DATA_MESSAGE_SIZE == LONG_SCRIPT_SIZE) 
              lastPacket = TRUE;
        }
        else if(indexPacket - scriptAddress + DATA_MESSAGE_SIZE == SCRIPT_SIZE) 
              lastPacket = TRUE;
        
        status = WriteSectorNvDataMultiple( sector, indexPacket, packet, DATA_MESSAGE_SIZE, lastPacket, firstPacket);
        if (status)
        { 
          scriptSize = 0;
          return status + 0x80; //return error
        }
        
        firstPacket = FALSE;
        indexPacket += DATA_MESSAGE_SIZE;
        
        //still more messages to be received, quit here
        if (counter > 0) 
        {
          counterPrevious = counter;
          return status; //should be 0.  
          //This is not an error response, just a normal early exit if we're not on the last packet
          //scriptSize is maintained for the next call of this function
        }
        //otherwise fill an empty packet
        memset(packet, 0xFF, sizeof(packet)); //initialize packet
    }
    while(!lastPacket);
         
    OSTimeDlyHMSM(0, 0, 0, 50, OS_OPT_TIME_HMSM_STRICT,  &err); // wait until flash is burned
   
    
    if (status == 0) 
    {
      // write the script pointer with the ScriptID
      CPU_INT08U byteScriptID = 0;
      ReadLocalFlashData( scriptBaseAddress + scriptSize - 1, &byteScriptID, 1 ); //ScriptID is last byte of script
      if (byteScriptID == 0xFF)
      {
        scriptSize = 0;
        return 7; //error: bad script ID or script did not download correctly
      }
      else
      {
        pControlWord = byteScriptID << 8;
        if(writeLocalDict( &ObjDict_Data, 0x1F51, scriptPointer, &pControlWord, &varsize, 0))
        {
          scriptSize = 0;
          return 8; //error: could not set script pointer
        }
        
        // need to write script ID before loading var table, otherwise it bypasses it.
        status = LoadGlobalVarTable( 0 );
        if(status)
        {
          scriptSize = 0;
          EraseScriptSegment(scriptPointer);
          ScriptDebug_statusByte = SCRIPT_ERR_RESET_GLOBALS;
          return 9; //error: could not load global variables
        }
      }
    }
        
    scriptSize = 0;
    return status; //should be 0
} 


/**
@brief Reads 32 bytes from local flash memory through OD interface.  
@param none.  Address of read determined by global OD values
@return status (0=success, 1=read not triggered and address has not changed since last read, 2=out of range address)
*/
CPU_INT08U ReadLocalFlashMemory(void)
{
  CPU_INT08U RMDataSize = sizeof(ReadMemoryData);
  CPU_INT08U recordSize = RMDataSize - 4;
  
  if (AddressRequest + recordSize > 0x03FFFF)
    return 2;
  
  if (lastRequestedAddress != AddressRequest || (triggerReadMemory == 1))
  { 
    // set pattern to clear if read
    memset( ReadMemoryData, 0xA5, sizeof(ReadMemoryData));   
    ReadLocalFlashData( AddressRequest, ReadMemoryData, recordSize );
    // now copy requested address into the top of the array
    ReadMemoryData[RMDataSize - 4] = (CPU_INT08U)AddressRequest;
    ReadMemoryData[RMDataSize - 3] = (CPU_INT08U)(AddressRequest >> 8);
    ReadMemoryData[RMDataSize - 2] = (CPU_INT08U)(AddressRequest >> 16);
    ReadMemoryData[RMDataSize - 1] = (CPU_INT08U)(AddressRequest >> 24);
    
    lastRequestedAddress = AddressRequest;
    triggerReadMemory = 0;
    return 0;
  }   
  return 1;
}


/**
@brief Reads 32 bytes from remote flash memory through OD interface.  
@param none.  Address of read determined by global OD values
@return status (0=success, 1=read not triggered and address has not changed since last read, 2=out of range address)
*/
CPU_INT08U ReadRemoteFlashMemory(void)
{
  CPU_INT08U status = 1;
  CPU_INT08U RMDataSize = sizeof(ReadMemoryData);
  CPU_INT08U recordSize = RMDataSize - 4;
  
  if (lastRequestedAddress != AddressRequest || (triggerReadMemory == 1))
  {
     // set pattern to clear if read
    memset( ReadMemoryData, 0xA5, sizeof(ReadMemoryData));   
    
    status = ReadRemoteFlash( AddressRequest, ReadMemoryData, recordSize );
    // now copy requested address into the top of the array
    ReadMemoryData[RMDataSize - 4] = (CPU_INT08U)AddressRequest;
    ReadMemoryData[RMDataSize - 3] = (CPU_INT08U)(AddressRequest >> 8);
    ReadMemoryData[RMDataSize - 2] = (CPU_INT08U)(AddressRequest >> 16);
    ReadMemoryData[RMDataSize - 1] = (CPU_INT08U)(AddressRequest >> 24);
    
    lastRequestedAddress = AddressRequest;
    triggerReadMemory = 0;
    return status;
  }   
  return 1;
}


/**
@brief Reads 32 bytes from remote RAM through OD interface.  
@param none.  Address of read determined by global OD values
@return status (0=success, 1=read not triggered and address has not changed since last read, 2=out of range address)
*/
CPU_INT08U ReadRemoteRAMMemory(void)
{
  CPU_INT08U status = 1;
  CPU_INT08U RMDataSize = sizeof(ReadMemoryData);
  CPU_INT08U recordSize = RMDataSize - 4;
  
  if (AddressRequest > (0x7FFF - recordSize))
      return 2;
      
  if (lastRequestedAddress != AddressRequest || (triggerReadMemory == 1))
  {
     // set pattern to clear if read
    memset( ReadMemoryData, 0xA5, sizeof(ReadMemoryData));   
    status = ReadRemoteRAM( AddressRequest, ReadMemoryData, recordSize );
    // now copy requested address into the top of the array
    ReadMemoryData[RMDataSize - 4] = (CPU_INT08U)AddressRequest;
    ReadMemoryData[RMDataSize - 3] = (CPU_INT08U)(AddressRequest >> 8);
    ReadMemoryData[RMDataSize - 2] = (CPU_INT08U)(AddressRequest >> 16);
    ReadMemoryData[RMDataSize - 1] = (CPU_INT08U)(AddressRequest >> 24);
    
    lastRequestedAddress = AddressRequest;
    triggerReadMemory = 0;
    return status;
  }  
  return 1;
}

/** 
@brief Function called by NMT to force a memory read so don't have to wait for runcanserver loop
  Increments address to read automatically
@param memSelect (1=localFlash, 2=RemoteFlash, 3=RemoteRAM, 9=LocalRAM
@return none
*/
void ReadMemoryWithIncrement(UNS8 memSelect)
{
    // clear status
    statusByteMemory = 0;
    triggerReadMemory = 1;
    
    // run memory reads (eeprom = 4, 8) - not implemented on PM
    if (memSelect == 1)
      statusByteMemory = ReadLocalFlashMemory();
    else if (memSelect == 2)
      statusByteMemory = ReadRemoteFlashMemory();
    else if (memSelect == 3)
      statusByteMemory = ReadRemoteRAMMemory();
    else if (memSelect == 9)
      statusByteMemory = ReadLocalRAMMemory();
    else if (memSelect != 0)
    {
      statusByteMemory = 4; // no memory to process
      return;
    }
    else
    {
      return;
    }
    
    //leave memorySelect Active
    //increment address by 'record size'
    AddressRequest += 32;
    
}

/*
*********************************************************************************************************
*                                             ReadLocalRAMMemory()
*
* Description : the script interpreter
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U ReadLocalRAMMemory(void)
{
  int recordSize = 32;
  int RMDataSize = sizeof(ReadMemoryData);
  
  
  if (AddressRequest > (0x3FFF - recordSize)) //16KB on-chip RAM
      return 2;
      
  if (lastRequestedAddress != AddressRequest || (triggerReadMemory == 1))
  {
    // Read RAM: starts at 0x40000000
    memcpy( ReadMemoryData, (CPU_INT08U*)(0x40000000+AddressRequest), sizeof(ReadMemoryData));   

    // now copy requested address into the top of the array
    ReadMemoryData[RMDataSize - 4] = (CPU_INT08U)AddressRequest;
    ReadMemoryData[RMDataSize - 3] = (CPU_INT08U)(AddressRequest >> 8);
    ReadMemoryData[RMDataSize - 2] = (CPU_INT08U)(AddressRequest >> 16);
    ReadMemoryData[RMDataSize - 1] = (CPU_INT08U)(AddressRequest >> 24);
    
    lastRequestedAddress = AddressRequest;
    triggerReadMemory = 0;
    return 0;
  }  
  return 1;
}

/*
*********************************************************************************************************
*                                             WriteLocalFlashMemory()
*
* Description : the script interpreter
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U WriteLocalFlashMemory(void)
{
  CPU_INT08U sector = 0;
  CPU_INT16U index = 0;
  CPU_INT08U status = 0;
  
  sector = FindScriptSector ( AddressRequest );
  
  if (sector == 0)
  {
    status = 2; // didn't find a sector => illegal address.  (Not within script sectors 10-16)
  }
  else
  {
    index = AddressRequest - sectorAddresses[sector-SECTOR_MIN];
    status = wrCpuNvData( sector, index, &writeByteMemory, 1);
  }
  
  return status;
  
} 


/**
@brief Writes one byte to remote flash via OD interface.  Does not erase before write.  
JML: This should not be used because the remote Flash is used for logging and wirting a non-FF byte in flash could 
mess up the logging cursor position 
@param none
@return status from WriteRemoteFlash
*/
CPU_INT08U WriteRemoteFlashMemory(void)
{  
  return WriteRemoteFlash( AddressRequest, &writeByteMemory, 1 );
}     

/**
@brief Writes one byte to Remote RAM via OD interface
@param none
@return status (0=success, 2=out of range or status from WriteRemoteRAM) 
*/
CPU_INT08U WriteRemoteRAMMemory(void)
{
 CPU_INT08U status = 0;
  
  if (AddressRequest > 0x7FFF) // nominal address space
    status = 2;
  else
    status = WriteRemoteRAM( AddressRequest, &writeByteMemory, 1 );
  return status;
}

/*
*********************************************************************************************************
*                                             RunHighSpeedCollection()
*
* Description : 
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/

void RunHighSpeedCollection(CPU_INT08U skipTime)
{
  
  //wrNvData( 0x00, dataScript, sizeof(blockCounter) ) ; 
}
/*
*********************************************************************************************************
*                                             EraseScriptSegment()
*
* Description : invoked by nmt_master -- writes FF's to erase designated script.
* 
* Argument(s) : scriptNumber
*
* Return(s)   : none.
*
*             : 
*********************************************************************************************************
*/
void EraseScriptSegment(CPU_INT08U scriptNumber)
{ 
 
  UNS8 data[2] = {0, 0};
  
  LoadScriptToFlash(scriptNumber, data, 2, 0); //load an empty script
  
}

/*
*********************************************************************************************************
*                                             RunStartupScript()
*
* Description : 
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/

void RunStartupScript( void )
{
  //CPU_INT08U Script0Data[256];
  
  
  
  /* Note the sequence -- InitalizeScriptMemory copies the OD defaults to serial Eprom
   * then RunStartupScript copies them back. if this is done out of order, it will fail!*/
  
  
}


/*
*********************************************************************************************************
*                                             WriteRadioConf()
*
* Description : Writes the radio configuration to serial eeprom (part of the script(0) data. this is called
* from the NMT command NMT_Radio_Address.
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
void WriteRadioConfig( void )
{
  
  CPU_INT08U data[4];
  
  data[0] = RADIO_LocalAddress;
  data[1] = RADIO_RemoteAddress;
  data[2] = RADIO_ChannelNumber;
  data[3] = RADIO_TXPower;

  wrCpuNvData( APP_SETTINGS_SECTOR , APP_SETTINGS_SECTOR_ADDRESS, data, sizeof(data) ); // last 512 segment in addressable memory + 2
}

/*
*********************************************************************************************************
*                                             SaveValues()
*
* Description : Called when system senses a power down condition; saves RTC values; also saves the values of
* (primarily) PDO configuration data found in the list OD 0x2900.0. Called from NMT_Do_Save_Cmd.
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U SaveValues( void )
{
  CPU_INT08U pData[SCRIPT_MAX_STRING];
  CPU_INT32U abortCode;
  CPU_INT16U counter = 3;
  CPU_INT08U nIndices = 0, nSubIndices = 0;     //support for up to 255 Inidices in RestoreList
  CPU_INT08U i, subIndex;                       //support for up to 255 Inidices in RestoreList
  CPU_INT32U size;
  
  CPU_INT08U type;

  OS_ERR err;
  
  nIndices = sizeof(RestoreList) / 2;
  
  //We will copy the OD Restore Indices into the two binary buffers on external flash.  
  //After both buffers are filled, we push them into flash.  
  //Initialize the binary Buffers.  
  ClearBinaryBuffers();
    
  for (i = 0; i < nIndices; i++)
  {
    memset(pData, 0, sizeof(pData));
    size = 0; // need to initialize before SDO reads
    type = 0;
    abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], 1, pData, &size, &type, 0); // check to see if a subindex exists
      
    size = 0;
    type = 0;
    if (abortCode == OD_NO_SUCH_SUBINDEX)     // No subindices.           
    { 
      //Get the data
      abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], 0, pData, &size, &type, 0);  
      if ( abortCode != OD_SUCCESSFUL )
            return 2;
      nSubIndices = 0;
      subIndex = 0;
    }                                                                                                                               
    else  if(abortCode == OD_SUCCESSFUL) //at least one subindex
    {
      // Get number of subindices
      abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], 0, pData, &size, &type, 0);
      if ( abortCode != OD_SUCCESSFUL )
            return 3;
      nSubIndices = pData[0];
      subIndex = 1;
    }
    else
    {
      asm("nop");
      return 4;
    }
    
 
    
    do{
      //Note: if nSubIndices == 0, pData currently points to the data
        if (nSubIndices > 0)
        {
          size = 0;
          type = 0;
          //Get the data for the subindex
          abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], subIndex, pData, &size, &type, 0);
          if ( abortCode != OD_SUCCESSFUL )
            return 5;  
        }
        
        if (size > sizeof(pData))
           return 24;
        
        if (counter + size > SAVE_RESTORE_BUFF_SIZE)
        {
          Status_modeSelect |= (1 << 6);
          Status_modeSelect |= counter << 16;
          return 25;
        }
        
        FillBinaryBuffers(counter, pData, (CPU_INT16U)size);
        counter += size;       
        subIndex++;
      
    } while(subIndex <= nSubIndices);
    
  }

  //Write the header to the Restore Data
  pData[0] = (CPU_INT08U)counter;
  pData[1] = (CPU_INT08U)(counter >> 8);
  pData[2] = nIndices;
  FillBinaryBuffers(0x00, pData, 3);
  
  //Finally write the binary buffers to flash
  WriteBinaryBuffersToRemoteFlash(SAVE_RESTORE_ADDRESS);
  
  OSTimeDlyHMSM(0, 0, 0, 50, OS_OPT_TIME_HMSM_STRICT, &err); 
     
  return 0;
}
/*
*********************************************************************************************************
*                                             RestoreValues()
*
* Description : Called on power up; restores the values of (primarily) PDO configuration data contained in 
* OD index 0x2900.0
* Called from NMT_Do_Restore_Cmd (only when in the Waiting Mode) and from the startup sequence.
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U RestoreValues ( void )
{
  
  CPU_INT08U pData[SCRIPT_MAX_STRING]; //maximum size of subIndex in bytes
  CPU_INT32U abortCode;
  CPU_INT16U counter;
  CPU_INT16U restoreSize = 0;
  CPU_INT08U nIndices = 0, nSubIndices = 0;     //support for up to 255 Inidices in RestoreList
  CPU_INT08U i, subIndex;                   //support for up to 255 Inidices in RestoreList
  CPU_INT08U type;
  CPU_INT32U size;        
  
  ReadRemoteFlash( SAVE_RESTORE_ADDRESS , pData, 3 ); 
  restoreSize = pData[0] + (pData[1] << 8);
  nIndices = pData[2];
  counter = 3;
  
  if ( commandByte == 0x01 || restoreSize == 0x0000 || restoreSize == 0xFFFF || 
       restoreSize > SAVE_RESTORE_BUFF_SIZE || nIndices != sizeof(RestoreList)/2) // empty or corrupt OD restore, or user triggered restore
  {
    commandByte &= ~( 1 << 0); // remove flag to reset OD Defaults
    Status_modeSelect |= (1 << 7);
    SaveValues(); //save the default values into flash
    return 1;
  }
    
  for (i = 0; i < nIndices; i++) // iterate through list
  {
    memset(pData, 0, sizeof(pData));
    size = 0; // need to initialize before SDO reads
    type = 0;
    abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], 1, pData, &size, &type, 0); // check to see if a subindex exists
    
    size = 0;
    type = 0;
    if (abortCode == OD_NO_SUCH_SUBINDEX)     // No subindices.           
    { 
      //Get the type and size of data
      abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], 0, pData, &size, &type, 0);  
      if ( abortCode != OD_SUCCESSFUL )
            return 2;
      nSubIndices = 0;
      subIndex = 0;
    }                                                                                                                               
    else  if(abortCode == OD_SUCCESSFUL) //at least one subindex
    {
      // Get number of subindices
      abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], 0, pData, &size, &type, 0);
      if ( abortCode != OD_SUCCESSFUL )
            return 3;
      nSubIndices = pData[0];
      subIndex = 1;
    }
    else
    {
      return abortCode;
    }
    
    do{
        memset(pData, 0, sizeof(pData));

        if (nSubIndices > 0)
        {
          size = 0;
          type = 0;
          //Get the type and size of data for the subindex
          abortCode = readLocalDict( &ObjDict_Data, RestoreList[i], subIndex, pData, &size, &type, 0);
          if ( abortCode != OD_SUCCESSFUL )
            return 4;  
        }

        if(size > sizeof(pData))
           return 24;
        
        if(counter + size > restoreSize)
          return 25;
        //IO0SET = BIT0;   
        ReadRemoteFlash( SAVE_RESTORE_ADDRESS + counter, pData, size); 
        //IO0CLR = BIT0;
        counter += size;
        
        abortCode = writeLocalDict( &ObjDict_Data, RestoreList[i], subIndex, pData, &size, 0);
        if ( abortCode != OD_SUCCESSFUL )
          return 5;

        subIndex++;
      
    } while(subIndex <= nSubIndices);

  } // end for loop
           
  return 0;
}

/*
*********************************************************************************************************
*                                             ResetToODDefault()
*
* Description : invoked by nmt_master -- writes 0's to size causing bypass of OD restore, then causes reset
* 
* 
*
* Argument(s) : none
*
* Return(s)   : none.
*
*********************************************************************************************************
*/

void ResetToODDefault(void)
{
  OS_ERR err;
  CPU_INT08U data[2] = {0,0};
  
  WriteRemoteFlashWithErase( SAVE_RESTORE_ADDRESS , data, 2 ); // write two bytes of 0's to cancel restore
   
   // delay to complete write
   OSTimeDlyHMSM(0, 0, 0, 200,  OS_OPT_TIME_HMSM_STRICT,  &err); 
   
   Reset_Module();
  
}


/**
* @brief Enables or Disables TPDOs.  Called upon entry into Test Y Manual mode 
* @param mode: 1=PDO sent on sync, 2=PDO sent on SYNC if data has changed, otherwise PDO is disabled 
*/
void EnableTPDOs(UNS8 mode)
{
  UNS8 data = 253;
  UNS32 size = 0;
  UNS8 maxPDO = ObjDict_Data.lastIndex->PDO_TRS - ObjDict_Data.firstIndex->PDO_TRS;
  UNS32 abortCode;
  
  if(mode == 1)  
  {
      data = 1; //PDO sent whether data changed or not
  }
  else if(mode == 2)
  {
      data = 0; //PDO only sent when data changed
  }
  
  //only change the TPDO settings if they are being used for StimY.  
  if(StatusStimY || mode ==1 || mode==2) 
  {
    for(UNS8 i=0; i <= maxPDO; i++)
    {
      abortCode = writeLocalDict( &ObjDict_Data, 0x1800+i, 2, &data, &size, 0); //write transmission type
    }
  }
  
  if (mode<=2)
    StatusStimY = mode;
  else 
    StatusStimY = 0;
} 

/*
*********************************************************************************************************
*                                             RunHighSpeedPassThru(Param1)
*
* Description : invoked by nmt_master -- writes 0's to size causing bypass of OD restore, then causes reset
* 
* 
*
* Argument(s) : Module and channel indicator (Param1)
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
void RunHighSpeedPassThru(UNS8 Param1)
{
  targetHSNode = Param1 & 0x0F;
  targetHSChannel = Param1 >> 4;  
} 
/*
*********************************************************************************************************
*                                             GetNodeTable( Table );
*
* Description : 
* Argument(s) : pointer to array for node table
*
* Return(s)   : none.
*
*********************************************************************************************************
*/
CPU_INT08U GetNodeTable(CPU_INT08U * nodeTable)
{
  CPU_INT08U status = 0;
 
  MasterRequestNodeTable(&ObjDict_Data, nodeTable); // send local table
    
  return status;
  
}



