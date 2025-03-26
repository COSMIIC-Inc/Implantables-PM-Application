
// Doxygen
/*!
** @file   File_Operations.c
** @author Coburn
** @date   10/24/2015
** @note   This file contains the API for log file read and write operations. Since the underlying storage mechanisms are different in
** the Control Tower from the Power Module, these files are not interchangeable.
**
**
*/


#include "includes.h"
#include "sys.h"
#include "scripts.h"
#include "ObjDict.h"
#include "sysdep.h"
#include "data.h"
#include "cc1101radio.h"
#include "gateway.h"
#include "cpuFlash.h"
#include "ScriptInterpreter.h"
#include "File_Operations.h"
#include "ObjDict.h"
#include "lib_str.h"
#include "lib_def.h"

/*******************************************************************************************************
*                                         Globals
********************************************************************************************************/
//Status_NV_Flash_Status: 16 bit status register for NV_flash
//15:9 - currently unused
//   8 - erase all in progress
// 7:0 - AT45DB321D status register 
//     7: chip ready
//     6: Compare Result
//     5:2: unused
//     1: Protect bit
//     0 Page size

// File Attributes (Flags)
// 31:16 - rollover count (NOTE: rollovers are not fully supported- FilePointer will be incorrect resulting in data corruption)
// 15:9  - unused
//    8  - use rollover if bit set.  (Note: don't use rollover)
// 7:2   - unused
// 1:0   - r/w priveledges - unused by this application

//JML Notes for future implementation:
// multiple log files are only partially supported.  If multiple log files are required, FindFilePointer will 
// need to be implemented for each Log File.  Also, the Flush File functionality will have to limit block erases to only within
// that files address range.

static  const  CLK_DAY  Clk_DaysInYr[2u] = 
{
    DEF_TIME_NBR_DAY_PER_YR, DEF_TIME_NBR_DAY_PER_YR_LEAP
};

static  const  CLK_DAY  Clk_DaysInMonth[2u][CLK_MONTH_PER_YR] = {
  /* Jan  Feb  Mar  Apr  May  Jun  Jul  Aug  Sep  Oct  Nov  Dec */
   { 31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u },
   { 31u, 29u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u }
};


//JML Note: This fileBuffer is wasteful of RAM.  There is little value in buffering the data, when it needs 
//to be sent out in shorter (50byte) packets over radio anyway.  The Read_File_External and Read_File_Directory 
//functions should be simplified to avoid use of the filebuffer.  The Read from RAM is fast anyways. 
//If speed is essential, particularly if write file capability is added, the 512 buffer in AT45DB321 could be 
//used instead of this buffer in RAM
#if MAX_NUM_FILES > 4
    CPU_INT08U fileBuffer[MAX_NUM_FILES * SIZE_NNP_DIR + 20]; // 
#else
    CPU_INT08U fileBuffer[270]; 
#endif

CPU_INT32U fileSize[MAX_NUM_FILES];

//UNS16 pageCount[] = { 4096, 2048, 1024, 1014};
//UNS16 pageAddrByFile[] = { 4, 4100, 6148, 7172, 8186};

// index 0 (File ID = 1) is Log File 1 
// index 1 (File ID = 2) is Params File 
UNS32 baseAddress[MAX_NUM_FILES] = {LOGFILE_1_BASE, SAVE_RESTORE_ADDRESS}; 
                                   

CPU_BOOLEAN writeFileInterlock = FALSE; // if true, the file buffer has data to write.
CPU_INT08U writeFileID = 0;

UNS32 pageAddress = 0;

/*******************************************************************************************************
*                                         Structs
********************************************************************************************************/

FS_DIR_ENTRY NNP_DirEntry1;
FS_DIR_ENTRY NNP_DirEntryParam;
OS_MUTEX FileOpInterlock;
CPU_INT32U oldPointer;

// local definitions
CPU_INT08U GetDateTime(  CLK_DATE_TIME * currentTime );
CPU_BOOLEAN  Clk_DateTimeToStr (CLK_DATE_TIME  *p_date_time, CLK_STR_FMT fmt, CPU_CHAR *p_str, CPU_SIZE_T str_len);
static  CPU_BOOLEAN  Clk_IsLeapYr (CLK_YR  yr);
CPU_INT32U FindFilePointer(void);


/*
*********************************************************************************************************
*                                                InitFiles()
*
* Description : check to make sure external Flash is set up as log files; only reset if either reset is 
*               true or no value is found matching in directory.
*               
*
* Argument(s) : none
* Return(s)   : status
*
* Caller(s)   : Init Scripts
* Note        : fileSize[0-3] are the log files; fileSize[4] is the param file
* 
********************************************************************************************************/
CPU_INT08U InitFiles(CPU_INT08U reset)
{
  CPU_INT08U fileID;
  CPU_INT08U status = 0;
  
  char fileDir[FS_CFG_MAX_FILE_NAME_LEN];
  unsigned long varsize = 0;
  CPU_INT08U type = 0;
  OS_ERR err;
  CPU_TS ts;
  CPU_INT32U epochTime;
  CPU_INT16U paramFileSize = 0;
  CPU_INT08U dataFS[2];
  
  writeFileInterlock = FALSE;
  writeFileID = 0;
  
  // power up
  if (reset == 1) // reboot
      OSMutexCreate(&FileOpInterlock, "File Op Write Interlock", &err); 
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
  // clear buffer
  memset(fileBuffer, 0, sizeof(fileBuffer));

  // find the size of the param file
  ReadRemoteFlash( SAVE_RESTORE_ADDRESS , dataFS, 0x02 );
  paramFileSize = dataFS[0] + dataFS[1] * 256;
 
  fileSize[0] =  WriteFileSize_1;  fileSize[1] = paramFileSize;
             
  GetEpochTime( &epochTime);
  
  //Read Param File Data from directory
  status = ReadRemoteFlash( DIRECTORY_LOC + sizeof(NNP_DirEntry1), (CPU_INT08U *)(&NNP_DirEntryParam) , sizeof(NNP_DirEntryParam) );   
  
   // rebuild the directory and all files
  if ( reset == 2 )
  {   
    for ( fileID = 1; fileID <= MAX_NUM_FILES; fileID++)
    {
      memset(fileDir, 0, sizeof(fileDir));
       
      varsize = 0;
      type = 0;
      
      if (fileID == MAX_NUM_FILES ) // Param file
      {
        strcpy( (char *)(&NNP_DirEntryParam.Name), "NNP_PM_Params.bin");
        NNP_DirEntryParam.Info.MaxFileSize =  fileSize[fileID-1];
        NNP_DirEntryParam.Info.FileBaseAddress = baseAddress[fileID-1];
        NNP_DirEntryParam.Info.Pointer   = 0; 
        NNP_DirEntryParam.Info.Attrib    = ATTRIB_DEFAULT; //no rollover; read only.  JML:was 1 
        NNP_DirEntryParam.Info.DateAccess = epochTime;   
        NNP_DirEntryParam.Info.DateTimeLastSave = epochTime;     
        NNP_DirEntryParam.Info.DateTimeWr = epochTime;  
        // now rewrite directory
        status = WriteRemoteFlashWithErase( DIRECTORY_LOC +  (fileID-1)*sizeof(NNP_DirEntry1), (CPU_INT08U *)(&NNP_DirEntryParam) , sizeof(NNP_DirEntryParam) );
      }
      else //Log file(s)
      {
        readLocalDict( &ObjDict_Data, 0xA200, fileID, fileDir, &varsize, &type, 0);     
        memcpy(&NNP_DirEntry1.Name, &fileDir, sizeof(fileDir));
        NNP_DirEntry1.Info.MaxFileSize = fileSize[fileID-1];
        NNP_DirEntry1.Info.FileBaseAddress = baseAddress[fileID-1];
        NNP_DirEntry1.Info.Pointer   = 0;
        NNP_DirEntry1.Info.Attrib = ATTRIB_DEFAULT; // read write - default to overwrite 0x0103
        NNP_DirEntry1.Info.DateAccess = epochTime;   
        NNP_DirEntry1.Info.DateTimeLastSave = epochTime;     
        NNP_DirEntry1.Info.DateTimeWr = epochTime;  
        // now rewrite directory
         status = WriteRemoteFlashWithErase( DIRECTORY_LOC + (fileID-1)*sizeof(NNP_DirEntry1), (CPU_INT08U *)(&NNP_DirEntry1) , sizeof(NNP_DirEntry1) );
      }
           
    }  
    // erase flash memory except for block 0
     if (Status_RemoteFlashBlock == 0)
            Status_RemoteFlashBlock = 0xFFFE; // initialize memory 
    
    //this won't complete erase until runcanserver gets through all blocks, but don't delay here otherwise radio task is held up

  }
  else // just init on power up.
  {
     for ( fileID = 1; fileID <= MAX_NUM_FILES; fileID++)
     {
        OSTimeDlyHMSM(0, 0, 0, 45,  OS_OPT_TIME_HMSM_STRICT,  &err); 
        if (fileID == MAX_NUM_FILES)
            status = ReadRemoteFlash( DIRECTORY_LOC + (fileID - 1)*sizeof(NNP_DirEntry1), (CPU_INT08U  *)(&NNP_DirEntryParam), sizeof(NNP_DirEntryParam) ); // read dir for param 
        else            
            status = ReadRemoteFlash( DIRECTORY_LOC + (fileID - 1)*sizeof(NNP_DirEntry1), (CPU_INT08U  *)(&NNP_DirEntry1), sizeof(NNP_DirEntry1) ); 
        }
     
     NNP_DirEntry1.Info.Pointer = FindFilePointer(); //JML note: if there has been a rollover, this will incorrectly set the FilePointer.
                                                     //Rollovers are not currently fully supported!
     
  }
  FilePointer =  NNP_DirEntry1.Info.Pointer; //JML added 
  OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
  
  return status;

}

/*
*********************************************************************************************************
*                                             ReadFileDirectory( file number, return buffer )
*
* Description : read directory structure - populated manually from Flash
*
* Argument(s) : File ID, return buffer, len
*
* Return(s)   : status
*********************************************************************************************************
* NOTE: FILE ID IS 1-4 FOR LOG FILES AND 5 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
*********************************************************************************************************
*/  
CPU_INT08U ReadFileDirectory(CPU_INT08U fileID, CPU_INT08U * rxBuffer, CPU_INT08U * len)
{
 
  CPU_INT08U  index, k;
  CPU_INT08U  status = 0;
  static CPU_INT16U remainingSize = 0;
  static CPU_INT08U counter = 0;
  OS_ERR err;
  CPU_TS ts;
  
  if (fileID < 1)
    return 2;
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
  
  if (fileID == 0xF) // init command - read directory and store results in fileBuffer - 60 bytes = dir size
  {    
    // initialize
    remainingSize = 0;
    counter = 0;
    
    memset(fileBuffer, 0, sizeof(fileBuffer));
    
    // copy directories into file buffer
    memcpy(fileBuffer, &NNP_DirEntry1, sizeof(NNP_DirEntry1));
    memcpy(&fileBuffer[sizeof(NNP_DirEntry1)], &NNP_DirEntryParam, sizeof(NNP_DirEntryParam));
                       
    // ok - now start sending the directory info back.
    memcpy(&rxBuffer[2], fileBuffer, MAX_DATA_BUFFER - 2);
    remainingSize = sizeof (NNP_DirEntry1) * MAX_NUM_FILES; // nothing sent
    rxBuffer[0] = (CPU_INT08U)(remainingSize & 0xFF);
    rxBuffer[1] = (CPU_INT08U)(remainingSize >> 8);
    
    if (remainingSize <= MAX_DATA_BUFFER - 2)
    {
      *len = remainingSize + 2;
    }
    else
    {
      remainingSize -= MAX_DATA_BUFFER - 2;
      *len = MAX_DATA_BUFFER;
    }  
  }
  else if (fileID == 0xE) // follow on command after 0x0F
  {    
    counter++;
    
    if (remainingSize <= (MAX_DATA_BUFFER - 2)) // last packet
    {
      memcpy(&rxBuffer[2], &fileBuffer[(MAX_DATA_BUFFER	 - 2) * counter], remainingSize);
      rxBuffer[0] = (CPU_INT08U)(remainingSize & 0xFF);
      rxBuffer[1] = (CPU_INT08U)(remainingSize >> 8);
      *len = remainingSize + 2;
    }
    else
    {
      memcpy(&rxBuffer[2], &fileBuffer[(MAX_DATA_BUFFER	 - 2) * counter], MAX_DATA_BUFFER - 2);
      rxBuffer[0] = (CPU_INT08U)(remainingSize & 0xFF);
      rxBuffer[1] = (CPU_INT08U)(remainingSize >> 8);
      *len = MAX_DATA_BUFFER;
      remainingSize -= MAX_DATA_BUFFER - 2;
    }
  }
  else
  {
    status = 1;
  }
  OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
  return status;

}

/*
*********************************************************************************************************
*                                             FlushFile( file number )
*
* Description : Flushfile truncates the file to zero length. if file ID = 0 all files are reset.
*
* Argument(s) : File ID
*
* Return(s)   : status = 0 for success.
*
**********************************************************************************************************
* NOTE: FILE ID IS 1 FOR LOG FILES AND 2 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
*********************************************************************************************************
*/  
CPU_INT08U FlushFile( CPU_INT08U fileID)
{
  /* fileID is 1 (Log) or 2 (Param)*/
  
  CPU_INT32U epochTime = 0;
  CPU_INT08U  status = 0;
  CPU_INT08U  saveODsize[2];
  OS_ERR err;
  CPU_TS ts;
  
  if (fileID > MAX_NUM_FILES || fileID == 0 )
    return 22;
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err); 
  
  GetEpochTime(&epochTime);

  status = ReadRemoteFlash( DIRECTORY_LOC + (fileID - 1) * sizeof(NNP_DirEntry1), (CPU_INT08U *)(&NNP_DirEntry1) , sizeof(NNP_DirEntry1) );
  if (status > 0)
  {
    OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
    return status;
  }
  if (fileID == MAX_NUM_FILES) // param file
  {
    saveODsize[0] = 0;
    saveODsize[1] = 0;
    NNP_DirEntryParam.Info.Pointer = 0;
    NNP_DirEntryParam.Info.DateTimeWr = epochTime;
    WriteRemoteFlashWithErase( SAVE_RESTORE_ADDRESS, saveODsize, sizeof(saveODsize));    
    OSTimeDlyHMSM(0, 0, 0, 45,  OS_OPT_TIME_HMSM_STRICT,  &err); 
    status = WriteRemoteFlashWithErase( DIRECTORY_LOC + (fileID - 1) * sizeof(NNP_DirEntry1), (CPU_INT08U *)(&NNP_DirEntry1) , sizeof(NNP_DirEntry1) );
  }
  else //log file(s)
  {
    NNP_DirEntry1.Info.Pointer = 0;
    NNP_DirEntry1.Info.DateAccess = epochTime;
    NNP_DirEntry1.Info.DateTimeWr = epochTime;
    NNP_DirEntry1.Info.Attrib = ATTRIB_DEFAULT; // clear roll counter

    OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
 
     // erase flash memory except for block 0
     if (Status_RemoteFlashBlock == 0)
        Status_RemoteFlashBlock = 0xFFFE; // initialize memory 
    
  }
  //note the erase won't complete until runcanserver completes, but dont delay here because then radio task can't monitor completion
   
  FilePointer =  NNP_DirEntry1.Info.Pointer; //JML added 
  return status;
}
/*
*********************************************************************************************************
*                                             SetFileAttributes(pkt, rxbuffer, rxLen)
*
* Description : sets the file parameters in the Dir. if file ID = 0 all files are reset.
*
* Argument(s) : File ID, Attribute value
*
* Return(s)   : status = 0 for success.
*
**********************************************************************************************************
* NOTE: FILE ID IS 1-4 FOR LOG FILES AND 5 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
*********************************************************************************************************
*/  
CPU_INT08U SetFileAttributes( PACKET_HEADER * pkt, CPU_INT08U * rxBuffer, CPU_INT08U * len)
{
  
  CPU_INT08U status = 0;
  CPU_INT08U fileID = pkt->counter & 0x0F;
  OS_ERR err;
  CPU_TS ts;
  
  if (fileID > MAX_NUM_FILES || fileID == 0 )
    return 22;
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);  
  //OSTimeDlyHMSM(0, 0, 0, 45,  OS_OPT_TIME_HMSM_STRICT,  &err); 
  status = ReadRemoteFlash( DIRECTORY_LOC + (fileID - 1) * sizeof(NNP_DirEntry1), (CPU_INT08U *)(&NNP_DirEntry1) , sizeof(NNP_DirEntry1) );
  
  NNP_DirEntry1.Info.Attrib              = pkt->data + *(&pkt->data + 1) * 256 + *(&pkt->data + 2) * 65536 + *(&pkt->data + 3) * 0x01FFFFFF;
  NNP_DirEntry1.Info.Pointer                = *(&pkt->data + 4) + *(&pkt->data + 5) * 256 + *(&pkt->data + 6) * 65536 + *(&pkt->data + 7) * 0x01FFFFFF;
  NNP_DirEntry1.Info.DateTimeLastSave      = *(&pkt->data + 8) + *(&pkt->data + 9) * 256 + *(&pkt->data + 10) * 65536 + *(&pkt->data + 11) * 0x01FFFFFF;
  NNP_DirEntry1.Info.DateAccess          = *(&pkt->data + 12) + *(&pkt->data + 13) * 256 + *(&pkt->data + 14) * 65536 + *(&pkt->data + 15) * 0x01FFFFFF;
  NNP_DirEntry1.Info.DateTimeWr          = *(&pkt->data + 16) + *(&pkt->data + 17) * 256 + *(&pkt->data + 18) * 65536 + *(&pkt->data + 19) * 0x01FFFFFF;
  
  status = WriteRemoteFlashWithErase( DIRECTORY_LOC + (fileID - 1) * sizeof(NNP_DirEntry1), (CPU_INT08U *)(&NNP_DirEntry1) , sizeof(NNP_DirEntry1) );
  
  if (status > 0)
  {
    OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
    return status;
  }
  
  *len = 1; // successful write
  rxBuffer[0] = 0;
  OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
  return status;
}
/*
*********************************************************************************************************
*                                             ReadFileExternal( header packet, rxBuffer, rxLen )
*
* Description : uploads file contents
*
* Argument(s) : File ID
*
* Return(s)   : status = 0 for success.
*
**********************************************************************************************************
* NOTE: FILE ID IS 1-4 FOR LOG FILES AND 5 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
*********************************************************************************************************
*/  
CPU_INT08U ReadFileExternal( PACKET_HEADER * txPkt, CPU_INT08U * rxBuffer, CPU_INT08U * len )
{
  /*  command == 0xD  */
  /* fileID is 1 to 5 */
  
  CPU_INT08U status = 0;
  CPU_INT08U fileID = txPkt->counter & 0x0F;
  CPU_INT32U readFileAddress = txPkt->data + *(&txPkt->data + 1) * 256 + *(&txPkt->data + 2) * 65536 + *(&txPkt->data + 3) * 0x01FFFFFF; 
  CPU_INT16U readRequestedSize = 0;
  static CPU_INT08U counter;
  static CPU_INT16U remainingSize;
  OS_ERR err;
  CPU_TS ts;
  
  CPU_INT32U epochTime = 0;
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
  
  if (fileID < 0xE) // send first packet of data back with the original command
                    // where command = 0xD | fileID = directory file number
  {    
    // initialize 
    memset(fileBuffer, 0, sizeof(fileBuffer));
    counter = 0;
    remainingSize = 0;
    readRequestedSize = *(&txPkt->data + 4) + *(&txPkt->data + 5) * 256;   
    
    fileID = fileID & 0x07;
    if (fileID <= (MAX_NUM_FILES - 1))  // log file
    {
        GetEpochTime(&epochTime);
        NNP_DirEntry1.Info.DateAccess = epochTime;       
    }
    
    if (readRequestedSize > sizeof(fileBuffer))
    {
      OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
      return 5;
    }
    else if (fileID <= (MAX_NUM_FILES - 1)) // not the param file ( ie 1 - 4 log files)
    {
      status = ReadRemoteFlash( baseAddress[fileID - 1] + readFileAddress, fileBuffer, readRequestedSize);   
    }
    else if (fileID == MAX_NUM_FILES) // param file
    {
      status = ReadRemoteFlash( SAVE_RESTORE_ADDRESS + readFileAddress, fileBuffer, readRequestedSize); // start 00
    }
    
    remainingSize = readRequestedSize;
    
    // ok - now start sending the file info back.
    memcpy(&rxBuffer[2], fileBuffer, MAX_DATA_BUFFER - 2);
    // remaining size are the bytes left after the packet is sent.
    
    if (remainingSize <= (MAX_DATA_BUFFER - 2))
    {
      *len = remainingSize + 2; // everything in the first packet
    }
    else
    {
      remainingSize -= (MAX_DATA_BUFFER - 2);
      *len = MAX_DATA_BUFFER;
    }
    
    rxBuffer[0] = (CPU_INT08U)(remainingSize & 0xFF);
    rxBuffer[1] = (CPU_INT08U)(remainingSize >> 8);
    
    // end of first packet. if remaining size is bigger than one packet, process below
  }
  else if (fileID == 0xE) // all remaining packets have a fileID of E
                          // command = 0xD | fileID = 0xE
  {
    counter++;
    if (remainingSize <= (MAX_DATA_BUFFER - 2)) // last packet
    {
      memcpy(&rxBuffer[2], &fileBuffer[(MAX_DATA_BUFFER - 2) * counter], remainingSize);
      *len = remainingSize + 2;
    }
    else
    {
      memcpy(&rxBuffer[2], &fileBuffer[(MAX_DATA_BUFFER - 2) * counter], MAX_DATA_BUFFER - 2);
      *len = MAX_DATA_BUFFER	;
      remainingSize -= MAX_DATA_BUFFER - 2;
    }
    rxBuffer[0] = (CPU_INT08U)(remainingSize & 0xFF);
    rxBuffer[1] = (CPU_INT08U)(remainingSize >> 8);
  }
  else
  {
    status = 1;
  }
  OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
  return status;
}
/*
*********************************************************************************************************
*                                             WriteFileExternal( header packet )
*
* Description : downloads file contents
*
* Argument(s) : txpacket
*
* Return(s)   : status = 0 for success.
* Note - File ID (as opposed to the PM), is used as the position in the directory structure.
*
**********************************************************************************************************
* NOTE: FILE ID IS THE POSITION IN THE DIRECTORY STRUCTURE. THE FILE ID IS THE INDEX INTO THE FILE NAME
* ARRAY - NOT THE INDEX INTO THE OD ADDRESS.
*********************************************************************************************************
*/  
CPU_INT08U WriteFileExternal( PACKET_HEADER * txPkt, CPU_INT08U * rxBuffer, CPU_INT08U * rxLen )
{
    FS_DIR_ENTRY NNP_DirEntry;
    CPU_INT08U status = 0;
    CPU_INT08U fileID = txPkt->counter & 0x0F;
    
    if (fileID > MAX_NUM_FILES || fileID == 0 )
    return 22;
  
    //JML note: This function doesn't do anything yet!

  return 0;
}


/*
*********************************************************************************************************
*                                                WriteRecordToFile ()
*
* Description : writes serial data to file for logging using script header.
*               
*
* Argument(s) : PACKET_HEADER * pkt
*
* Return(s)   : status
*
* Caller(s)   : SDO Write Data from scripts ONLY - the param file is managed from scripts.c
*
**********************************************************************************************************
* NOTE: FILE ID IS 1-4 FOR LOG FILES AND 5 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
********************************************************************************************************/

CPU_INT08U WriteRecordToFile ( PACKET_HEADER * pkt )
{ 
  // offset is 33 - read size is 8, write size is 12
  CPU_INT08U  transferBuffer[12];
  CPU_INT08U  status = 0;
  CPU_INT08U copyBuffer[SCRIPT_MAX_STRING+20];
  UNS8 fileID    = pkt->subIndex;  
  UNS8 stringLen = pkt->dataLen;
  UNS8 stringLenPartial;
  UNS8 withClock = pkt->networkId; 
  
  UNS16 flags = NNP_DirEntry1.Info.Attrib & 0xFFFF; 
  UNS16 rollCounter = NNP_DirEntry1.Info.Attrib >> 16;
  CPU_INT32U epochTime = 0;
  CLK_DATE_TIME currentTime;
  OS_ERR err;
  CPU_TS ts;
  
  if (fileID < 1 || fileID > (MAX_NUM_FILES - 1)) // don't count Params
    return 4;
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
 
  memset(copyBuffer, 0x0, sizeof(copyBuffer));  // set as nulls.
  memset(transferBuffer, 0x0, sizeof(transferBuffer));
         
  if ((stringLen + 20) > sizeof(copyBuffer)) 
  {
    OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
     return 8;
  }

  if (withClock)
  {
    GetDateTime( &currentTime );
    Clk_DateTimeToStr(&currentTime, CLK_STR_FMT_YYYY_MM_DD_HH_MM_SS, copyBuffer, sizeof(copyBuffer));
    copyBuffer[19]=0x7C; // "|"
    memcpy (&copyBuffer[20], &pkt->data, stringLen );
    stringLen += 20;
  }
  else
  {  
    memcpy(copyBuffer, &pkt->data, stringLen ); 
  }
  
  // Check that log entry will fit 
  // - if rollover flag set, then write the part of the file log that fits, then reset the pointer (roll over), 
  // and write the remaing log entry.  After rollover, writeRemoteFlashWithErase is required.
  // - if rollover flag not set, skip the entire log entry
  if ((flags & 0x100) && (NNP_DirEntry1.Info.Pointer + stringLen) > fileSize[ fileID - 1]) // can't fit -> rollover
  {
    stringLenPartial = fileSize[ fileID - 1] - NNP_DirEntry1.Info.Pointer  ;
    
    status = WriteRemoteFlash( NNP_DirEntry1.Info.Pointer + baseAddress[fileID - 1], copyBuffer, stringLenPartial);
    NNP_DirEntry1.Info.Pointer = 0;
    rollCounter += 1;
    status = WriteRemoteFlashWithErase( NNP_DirEntry1.Info.Pointer + baseAddress[fileID - 1], &copyBuffer[stringLenPartial], stringLen - stringLenPartial);
  }
  else if ((NNP_DirEntry1.Info.Pointer + stringLen) > fileSize[ fileID - 1]) // can't fit -> stop logging
  {
    rollCounter = 1;
    OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
    return 36;
  }
  else if ((flags & 0x100) && rollCounter > 0) //has rolled over
  {
    status = WriteRemoteFlashWithErase( NNP_DirEntry1.Info.Pointer + baseAddress[fileID - 1], copyBuffer, stringLen); 
  }
  else
  {
    status = WriteRemoteFlash( NNP_DirEntry1.Info.Pointer + baseAddress[fileID - 1], copyBuffer, stringLen); 
  }
  
  if (status == 0) // update directory -- but not saved to NV memory
  {
    NNP_DirEntry1.Info.Pointer += stringLen;
    GetEpochTime(&epochTime);
    NNP_DirEntry1.Info.Attrib = flags + (rollCounter << 16);
    NNP_DirEntry1.Info.DateTimeWr = epochTime;
    // write data
  }
 
  FilePointer = NNP_DirEntry1.Info.Pointer; //JML added 
  OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
  return status;

}
/*
*********************************************************************************************************
*                                          UpdateFileDirectory(CPU_INT08U fileID)
*
* Description : closes the file operation of one or more writes to the chip RAM buffers. RAM buffers are
*               are copied to flash.
*
* Argument(s) : void. global parameters include -- Local_NNP_DirEntry, currentPointer, flags and rollCounter
*
* Return(s)   : status = 0 for success.
*
**********************************************************************************************************
* NOTE: FILE ID IS 1-4 FOR LOG FILES AND 5 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
*********************************************************************************************************
*/  
CPU_INT08U UpdateFileDirectory(CPU_INT08U fileID) //Not Used
{
  
  CPU_INT32U epochTime = 0;
  CPU_INT08U status = 0;
  CPU_INT16U flags = 0;
  CPU_INT32U rollCounter;
  CPU_TS ts;
  CPU_ERR err;
  
  if (oldPointer == NNP_DirEntry1.Info.Pointer)
    return 0;
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
  OSTimeDlyHMSM(0, 0, 0, 45,  OS_OPT_TIME_HMSM_STRICT,  &err);
  GetEpochTime(&epochTime);
  NNP_DirEntry1.Info.DateTimeLastSave = epochTime;
// write new pointer values - update directory
  status = WriteRemoteFlash( DIRECTORY_LOC + ((fileID - 1) * sizeof(NNP_DirEntry1)), (CPU_INT08U *)(&NNP_DirEntry1) , sizeof(NNP_DirEntry1) );
  oldPointer = NNP_DirEntry1.Info.Pointer;
  OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
  return status;
}

/*
*********************************************************************************************************
*                                          CPU_INT032U FindFilePointer(void)
*
* Description : 
*
* Argument(s) : void
*
* Return(s)   : file pointer
*
**********************************************************************************************************
* NOTE: This function finds the FilePointer for the single LogFile
*********************************************************************************************************
*/  
CPU_INT32U FindFilePointer(void)
{
  CPU_INT32U address = BASE_LOG_ADDRESS + ( (END_LOG_ADDRESS - BASE_LOG_ADDRESS) >> 1);  
  CPU_INT32U min_address = BASE_LOG_ADDRESS-1;
  CPU_INT32U max_address = END_LOG_ADDRESS;
  CPU_INT08U data[NUM_LOG_EMPTY];
  CPU_INT08U i;
  CPU_BOOLEAN empty;

  while(max_address - min_address > 1) 
  {
    ReadRemoteFlash( address, data, NUM_LOG_EMPTY );

    empty = TRUE;
    for (i = 0; i < NUM_LOG_EMPTY; i++)
    {
      if(data[i] != 0xFF) 
      {
        empty = FALSE;
        break; //break from for loop
      }
    }
    if( empty )
    {
      //search down
      max_address = address; //set new upper limit
      address -= (address - min_address)>>1; 
    }
    else
    {
      //search up
      min_address = address; //set new lower limit
      address += (max_address - address)>>1; 
    }  
  } //end while
  
  if( !empty )
    address++; //correct the address if the last search was up
  
  return(address-BASE_LOG_ADDRESS);
  
  
  return 0;
}

void BlankCheckRemoteFlash()
{
  UINT8 result = blankCheckRemoteFlash(&Status_RemoteFlashBlankCheck);
  if (result == 1) 
    Status_RemoteFlashBlankCheck |= BIT31;
  if (result == 2) 
    Status_RemoteFlashBlankCheck |= BIT30;
}

/*
*********************************************************************************************************
*                                                GetDateTime ()
*
* Description : Get date time from RTC but use uC to format clock string
*               
*
* Argument(s) :
* Return(s)   : status
*
* Caller(s)   : SDO Write File Data from scripts
*
* 
********************************************************************************************************/
CPU_INT08U GetDateTime(  CLK_DATE_TIME * currentTime )
{
  currentTime->Sec = (CLK_SEC)SEC;
  currentTime->Min = (CLK_MIN)MIN;
  currentTime->Hr = HOUR;
  currentTime->Day = (CLK_DAY)DOM;
  currentTime->DayOfWk = (CLK_DAY)DOW;
  currentTime->DayOfYr = (CLK_DAY)DOY;
  currentTime->Month = (CLK_MONTH)MONTH;
  currentTime->Yr = (CLK_YR)YEAR;
  currentTime->TZ_sec = (CLK_TZ_SEC)0;
  
  return 0;
}

/*
*********************************************************************************************************
*                                                GetEpochTime ()
*
* Description : Get epoch time from RTC 
*               
*
* Argument(s) : none
* Return(s)   : epoch time (32 bit)
*
* Caller(s)   : 
*
* 
********************************************************************************************************/
CPU_INT08U GetEpochTime( CPU_INT32U * epochTime )
{
  CLK_DATE_TIME  currentTime;
  CLK_DATE_TIME * pcurrentTime;
  
  currentTime.Sec = (CLK_SEC)SEC;
  currentTime.Min = (CLK_MIN)MIN;
  currentTime.Hr = HOUR;
  currentTime.Day = (CLK_DAY)DOM;
  currentTime.DayOfWk = (CLK_DAY)DOW;
  currentTime.DayOfYr = (CLK_DAY)DOY;
  currentTime.Month = (CLK_MONTH)MONTH;
  currentTime.Yr = (CLK_YR)YEAR;
  currentTime.TZ_sec = (CLK_TZ_SEC)0;
  
  pcurrentTime = &currentTime;
  
  CPU_BOOLEAN   leap_yr;
  CPU_INT08U    leap_yr_ix;
  CLK_YR        yr_ix;
  CLK_MONTH     month_ix;
  CLK_NBR_DAYS  nbr_days;
  CLK_TS_SEC    ts_sec;
  CLK_TS_SEC    tz_sec_abs; 
  CLK_YR        yr_start = CLK_UNIX_EPOCH_YR_START;
  
  /* ------------- CONV DATE/TIME TO CLK TS ------------- */
  nbr_days   =  pcurrentTime->Day - CLK_FIRST_DAY_OF_MONTH;
  leap_yr    =  Clk_IsLeapYr(pcurrentTime->Yr);
  leap_yr_ix = (leap_yr == DEF_YES) ? 1u : 0u;
  for (month_ix = CLK_FIRST_MONTH_OF_YR; month_ix < pcurrentTime->Month; month_ix++) 
  {
      nbr_days += Clk_DaysInMonth[leap_yr_ix][month_ix - CLK_FIRST_MONTH_OF_YR];
  }
  
  for (yr_ix = yr_start; yr_ix < pcurrentTime->Yr; yr_ix++) 
  {
      leap_yr     =  Clk_IsLeapYr(yr_ix);
      leap_yr_ix  = (leap_yr == DEF_YES) ? 1u : 0u;
      nbr_days   +=  Clk_DaysInYr[leap_yr_ix];
  }

  ts_sec  = nbr_days         * DEF_TIME_NBR_SEC_PER_DAY;
  ts_sec += pcurrentTime->Hr  * DEF_TIME_NBR_SEC_PER_HR;
  ts_sec += pcurrentTime->Min * DEF_TIME_NBR_SEC_PER_MIN;
  ts_sec += pcurrentTime->Sec;

                                                              /* ------------ ADJ FINAL TS FOR TZ OFFSET ------------ */
  tz_sec_abs = DEF_ABS(pcurrentTime->TZ_sec);
  if (pcurrentTime->TZ_sec < 0) 
  {
      ts_sec += tz_sec_abs;                                   /* See Note #1c1.                                       */
      if (ts_sec < tz_sec_abs) 
      {                              /* Chk for ovf when tz is neg.                          */
          return (DEF_FAIL);
      }
  } 
  else 
  {
      if (ts_sec < tz_sec_abs) 
      {                              /* Chk for ovf when tz is pos.                          */
          return (DEF_FAIL);
      }
      ts_sec -= tz_sec_abs;                                   /* See Note #1c2.                                       */
  }

  *epochTime = ts_sec; // passes epoch time
  
  return (DEF_OK);
  
}


CPU_BOOLEAN  Clk_DateTimeToStr (CLK_DATE_TIME  *p_date_time,
                                CLK_STR_FMT     fmt,
                                CPU_CHAR       *p_str,
                                CPU_SIZE_T      str_len)
{
    CPU_CHAR     yr   [CLK_STR_DIG_YR_LEN     + 1u];
    CPU_CHAR     month[CLK_STR_DIG_MONTH_LEN  + 1u];
    CPU_CHAR     day  [CLK_STR_DIG_DAY_LEN    + 1u];
    CPU_CHAR     hr   [CLK_STR_DIG_HR_LEN     + 1u];
    CPU_CHAR     min  [CLK_STR_DIG_MIN_LEN    + 1u];
    CPU_CHAR     sec  [CLK_STR_DIG_SEC_LEN    + 1u];
    CPU_CHAR     tz   [CLK_STR_DIG_TZ_MAX_LEN + 1u];
    CPU_CHAR     am_pm[CLK_STR_AM_PM_LEN      + 1u];
    CPU_BOOLEAN  valid;
    CLK_HR       half_day_hr;
    CLK_TS_SEC   tz_hr_abs;                                     /* See Note #4.                                         */
    CLK_TS_SEC   tz_min_abs;                                    /* See Note #4.                                         */
    CLK_TS_SEC   tz_sec_rem_abs;                                /* See Note #4.                                         */

   *p_str = '\0';                                               /* Init to NULL str for err (see Note #3).              */

                                                            
/* ----------------- VALIDATE STR LEN ----------------- */
   
    switch (fmt) 
    {
        
        case CLK_STR_FMT_YYYY_MM_DD_HH_MM_SS:
             if (str_len < CLK_STR_FMT_YYYY_MM_DD_HH_MM_SS_LEN) 
             {
                 return (DEF_FAIL);
             }
             break;

        case CLK_STR_FMT_MM_DD_YY_HH_MM_SS:
             if (str_len < CLK_STR_FMT_MM_DD_YY_HH_MM_SS_LEN) 
             {
                 return (DEF_FAIL);
             }
             break;
    }
    
   /* -------------- CREATE DATE/TIME STRS --------------- */
   (void)Str_FmtNbr_Int32U((CPU_INT32U )p_date_time->Yr,        /* Create yr str.                                       */
                           (CPU_INT08U )CLK_STR_DIG_YR_LEN,
                           (CPU_INT08U )DEF_NBR_BASE_DEC,
                           (CPU_CHAR   )'\0',
                           (CPU_BOOLEAN)DEF_NO,
                           (CPU_BOOLEAN)DEF_YES,
                           (CPU_CHAR  *)yr);

   (void)Str_FmtNbr_Int32U((CPU_INT32U )p_date_time->Month,     /* Create month (dig) str.                              */
                           (CPU_INT08U )CLK_STR_DIG_MONTH_LEN,
                           (CPU_INT08U )DEF_NBR_BASE_DEC,
                           (CPU_CHAR   )'0',
                           (CPU_BOOLEAN)DEF_NO,
                           (CPU_BOOLEAN)DEF_YES,
                           (CPU_CHAR  *)month);

   (void)Str_FmtNbr_Int32U((CPU_INT32U )p_date_time->Day,       /* Create day str.                                      */
                           (CPU_INT08U )CLK_STR_DIG_DAY_LEN,
                           (CPU_INT08U )DEF_NBR_BASE_DEC,
                           (CPU_CHAR   )'0',
                           (CPU_BOOLEAN)DEF_NO,
                           (CPU_BOOLEAN)DEF_YES,
                           (CPU_CHAR  *)day);

   (void)Str_FmtNbr_Int32U((CPU_INT32U )p_date_time->Hr,        /* Create hr str.                                       */
                           (CPU_INT08U )CLK_STR_DIG_HR_LEN,
                           (CPU_INT08U )DEF_NBR_BASE_DEC,
                           (CPU_CHAR   )'0',
                           (CPU_BOOLEAN)DEF_NO,
                           (CPU_BOOLEAN)DEF_YES,
                           (CPU_CHAR  *)hr);

   (void)Str_FmtNbr_Int32U((CPU_INT32U )p_date_time->Min,       /* Create min str.                                      */
                           (CPU_INT08U )CLK_STR_DIG_MIN_LEN,
                           (CPU_INT08U )DEF_NBR_BASE_DEC,
                           (CPU_CHAR   )'0',
                           (CPU_BOOLEAN)DEF_NO,
                           (CPU_BOOLEAN)DEF_YES,
                           (CPU_CHAR  *)min);

   (void)Str_FmtNbr_Int32U((CPU_INT32U )p_date_time->Sec,       /* Create sec str.                                      */
                           (CPU_INT08U )CLK_STR_DIG_SEC_LEN,
                           (CPU_INT08U )DEF_NBR_BASE_DEC,
                           (CPU_CHAR   )'0',
                           (CPU_BOOLEAN)DEF_NO,
                           (CPU_BOOLEAN)DEF_YES,
                           (CPU_CHAR  *)sec);
    
    switch (fmt) 
    {                                              /* ---------------- FMT DATE/TIME STR ----------------- */
       
        case CLK_STR_FMT_YYYY_MM_DD_HH_MM_SS:                   /* --------- BUILD STR "YYYY-MM-DD HH:MM:SS" ---------- */
             
            (void)Str_Copy_N(p_str, yr,    CLK_STR_DIG_YR_LEN + 1u);
            (void)Str_Cat_N (p_str, "-",   1u);

            (void)Str_Cat_N (p_str, month, CLK_STR_DIG_MONTH_LEN);
            (void)Str_Cat_N (p_str, "-",   1u);

            (void)Str_Cat_N (p_str, day,   CLK_STR_DIG_DAY_LEN);
            (void)Str_Cat_N (p_str, " ",   1u);

            (void)Str_Cat_N (p_str, hr,    CLK_STR_DIG_HR_LEN);
            (void)Str_Cat_N (p_str, ":",   1u);

            (void)Str_Cat_N (p_str, min,   CLK_STR_DIG_MIN_LEN);
            (void)Str_Cat_N (p_str, ":",   1u);

            (void)Str_Cat_N (p_str, sec,   CLK_STR_DIG_SEC_LEN);
             break;

        case CLK_STR_FMT_MM_DD_YY_HH_MM_SS:                     /* ---------- BUILD STR "MM-DD-YY HH:MM:SS" ----------- */
            

            (void)Str_FmtNbr_Int32U((CPU_INT32U )p_date_time->Yr,
                                    (CPU_INT08U )2u,
                                    (CPU_INT08U )DEF_NBR_BASE_DEC,
                                    (CPU_CHAR   )' ',
                                    (CPU_BOOLEAN)DEF_NO,
                                    (CPU_BOOLEAN)DEF_YES,
                                    (CPU_CHAR  *)yr);

            (void)Str_Copy_N(p_str, month, CLK_STR_DIG_MONTH_LEN + 1u);
            (void)Str_Cat_N (p_str, "-",   1u);

            (void)Str_Cat_N (p_str, day,   CLK_STR_DIG_DAY_LEN);
            (void)Str_Cat_N (p_str, "-",   1u);

            (void)Str_Cat_N (p_str, yr,    CLK_STR_DIG_YR_TRUNC_LEN);
            (void)Str_Cat_N (p_str, " ",   1u);

            (void)Str_Cat_N (p_str, hr,    CLK_STR_DIG_HR_LEN);
            (void)Str_Cat_N (p_str, ":",   1u);

            (void)Str_Cat_N (p_str, min,   CLK_STR_DIG_MIN_LEN);
            (void)Str_Cat_N (p_str, ":",   1u);

            (void)Str_Cat_N (p_str, sec,   CLK_STR_DIG_SEC_LEN);
             break;
    }

    return (DEF_OK);
}

/*
*********************************************************************************************************
*                                           Clk_IsLeapYr()
*
* Description : Determines if year is a leap year.
*
* Argument(s) : yr          Year value [1900 to  2135].
*
* Return(s)   : DEF_YES, if 'yr' is     a leap year.
*
*               DEF_NO,  if 'yr' is NOT a leap year.
*
* Caller(s)   : Clk_IsDateValid(),
*               Clk_IsDayOfYrValid(),
*               Clk_GetDayOfYrHandler(),
*               Clk_GetDayOfWkHandler(),
*               Clk_TS_ToDateTimeHandler(),
*               Clk_DateTimeToTS_Handler().
*
* Note(s)     : (1) (a) Most years that are evenly divisible by 4 are leap years; ...
*                   (b) (1) except years that are evenly divisible by 100,        ...
*                       (2) unless they  are also evenly divisible by 400.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  Clk_IsLeapYr (CLK_YR  yr)
{
    CPU_BOOLEAN  leap_yr;


    leap_yr = ( ((yr %   4u) == 0u) &&                          /* Chk for leap yr (see Note #1).                       */
               (((yr % 100u) != 0u) || ((yr % 400u) == 0u))) ? DEF_YES : DEF_NO;

    return (leap_yr);
}
