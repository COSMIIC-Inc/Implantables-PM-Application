
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





CPU_INT32U fileSize[MAX_NUM_FILES];

//UNS16 pageCount[] = { 4096, 2048, 1024, 1014};
//UNS16 pageAddrByFile[] = { 4, 4100, 6148, 7172, 8186};

// index 0 (File ID = 1) is Log File 1 
// index 1 (File ID = 2) is Params File 
UNS32 baseAddress[MAX_NUM_FILES] = {LOGFILE_1_BASE, SAVE_RESTORE_ADDRESS}; 
                                   


UNS32 pageAddress = 0;

/*******************************************************************************************************
*                                         Structs
********************************************************************************************************/

OS_MUTEX FileOpInterlock;


// local definitions
CPU_INT08U GetDateTime(  CLK_DATE_TIME * currentTime );
CPU_BOOLEAN  Clk_DateTimeToStr (CLK_DATE_TIME  *p_date_time, CLK_STR_FMT fmt, CPU_CHAR *p_str, CPU_SIZE_T str_len);
static  CPU_BOOLEAN  Clk_IsLeapYr (CLK_YR  yr);
CPU_INT32U FindFilePointer(void);


/*
*********************************************************************************************************
*                                                InitFiles()
*

********************************************************************************************************/
CPU_INT08U InitFiles(CPU_INT08U reset)
{
  CPU_INT08U status = 0;
  
  OS_ERR err;
  CPU_TS ts;
  CPU_INT16U paramFileSize = 0;
  CPU_INT08U dataFS[2];
  
 
  // power up
  if (reset == 1) // reboot
    OSMutexCreate(&FileOpInterlock, "File Op Write Interlock", &err); //JML todo: move this to where other mutexes get created
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);

  // find the size of the param file
  status = ReadRemoteFlash( SAVE_RESTORE_ADDRESS , dataFS, 0x02 );
  paramFileSize = dataFS[0] + (dataFS[1]<<8);
 
  fileSize[0] =  WriteFileSize_1;  fileSize[1] = paramFileSize;

  FilePointer = FindFilePointer(); //JML note: if there has been a rollover, this will incorrectly set the FilePointer.
                                                     //Rollovers are not currently fully supported!
     
  
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
  
  CPU_INT08U  status = 0;
  CPU_INT08U  saveODsize[2];
  OS_ERR err;
  CPU_TS ts;
  
  if (fileID > MAX_NUM_FILES || fileID == 0 )
    return 22;
  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err); 
  
  if (fileID == MAX_NUM_FILES) // param file
  {
    saveODsize[0] = 0;
    saveODsize[1] = 0;
    WriteRemoteFlashWithErase( SAVE_RESTORE_ADDRESS, saveODsize, sizeof(saveODsize));    
    OSTimeDlyHMSM(0, 0, 0, 45,  OS_OPT_TIME_HMSM_STRICT,  &err); 
  }
  else //log file(s)
  {
    FilePointer = 0;

     // erase flash memory except for block 0
     if (Status_RemoteFlashBlock == 0)
        Status_RemoteFlashBlock = 0xFFFE; // initialize memory 
    
  }
  //note the erase won't complete until runcanserver completes, but dont delay here because then radio task can't monitor completion
   
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
* NOTE: FILE ID IS 1 FOR LOG FILES AND 2 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
*********************************************************************************************************
*/  
CPU_INT08U ReadFileExternal( PACKET_HEADER * txPkt, CPU_INT08U * rxBuffer, CPU_INT08U * len )
{
  /*  command == 0xD  */
  /* fileID is 1 to 5 */
  
  CPU_INT08U *data = &(txPkt->data); //pointer to packet data
  CPU_INT08U status = 0;
  CPU_INT08U fileID = (txPkt->counter) & 0x0F;
  CPU_INT32U readFileAddress = data[0] + (data[1]<<8) + (data[2]<<16) + (data[3]<<24); 
  CPU_INT08U readRequestedSize = 0;
 
  OS_ERR err;
  CPU_TS ts;
  

  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
  
  if (fileID > 0  && fileID <= MAX_NUM_FILES) 
  {    
    readRequestedSize = data[4];
    
  
    if (readRequestedSize > MAX_DATA_BUFFER-2)  //leave room for size
    {
      rxBuffer[0] = 5;
      *len = 1;
      status = 5;
    }
    else
    {
    
      status = ReadRemoteFlash( baseAddress[fileID - 1] + readFileAddress, &rxBuffer[2], readRequestedSize);   

      
      *len = readRequestedSize + 2;
        
      rxBuffer[0] = 0;
      rxBuffer[1] = 0;
    }
  }
  else
  {
    rxBuffer[0] = 1;
    *len = 1;
    status = 1;
  }
  OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
  return status;
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
* NOTE: FILE ID IS 1 FOR LOG FILES AND 2 FOR PARAM FILE. SOME OPERATIONS USE ZERO BASED INDEXES WHERE
* THE REFERENCE (TYPICALLY FOR AN ARRAY) IS THEN FILE ID - 1.
********************************************************************************************************/

CPU_INT08U WriteRecordToFile ( PACKET_HEADER * pkt )
{ 
  // offset is 33 - read size is 8, write size is 12
  CPU_INT08U  status = 0;
  CPU_INT08U copyBuffer[SCRIPT_MAX_STRING+20];

  UNS8 stringLen = pkt->dataLen;
  UNS8 withClock = pkt->networkId; 
  
  CLK_DATE_TIME currentTime;
  OS_ERR err;
  CPU_TS ts;
  

  
  OSMutexPend(&FileOpInterlock, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
 
  memset(copyBuffer, 0x0, sizeof(copyBuffer));  // set as nulls.
         
  if ((stringLen + 20) > sizeof(copyBuffer)) 
  {
    OSMutexPost(&FileOpInterlock, OS_OPT_POST_1, &err);
     return 8;
  }

  if (withClock)
  {
    GetDateTime( &currentTime );
    Clk_DateTimeToStr(&currentTime, CLK_STR_FMT_YYYY_MM_DD_HH_MM_SS, (CPU_CHAR*) copyBuffer, sizeof(copyBuffer));
    copyBuffer[19]=0x7C; // "|"
    memcpy (&copyBuffer[20], &pkt->data, stringLen );
    stringLen += 20;
  }
  else
  {  
    memcpy(copyBuffer, &pkt->data, stringLen ); 
  }

  if ((FilePointer + stringLen) > WriteFileSize_1) // can't fit -> stop logging
  {
    status =  36;
  }
  else
  {
    status = WriteRemoteFlash(LOGFILE_1_BASE + FilePointer, copyBuffer, stringLen); 
  }
  
  if (status == 0) // 
  {
    FilePointer += stringLen;
  }
 
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
