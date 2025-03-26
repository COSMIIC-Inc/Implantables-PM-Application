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
#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "gateway.h"
#include "SPI_Memory.h"

/******************************************************************************************************
*                                         Defines
*******************************************************************************************************/

#define FS_CFG_MAX_FILE_NAME_LEN                           4
#define  CLK_STR_DIG_YR_LEN                                4u   /*     Str len of       yr dig.                         */
#define  CLK_STR_DIG_YR_TRUNC_LEN                          2u   /*     Str len of trunc yr dig.                         */
#define  CLK_STR_DIG_MONTH_LEN                             2u   /*     Str len of mon      dig.                         */
#define  CLK_STR_DIG_DAY_LEN                               2u   /*     Str len of day      dig.                         */
#define  CLK_STR_DIG_HR_LEN                                2u   /*     Str len of hr       dig.                         */
#define  CLK_STR_DIG_MIN_LEN                               2u   /*     Str len of min      dig.                         */
#define  CLK_STR_DIG_SEC_LEN                               2u   /*     Str len of sec      dig.                         */
#define  CLK_STR_DAY_OF_WK_MAX_LEN                         9u   /* Max str len of day of wk       str (e.g. Wednesday). */
#define  CLK_STR_DAY_OF_WK_TRUNC_LEN                       3u   /*     Str len of day of wk trunc str.                  */
#define  CLK_STR_MONTH_MAX_LEN                             9u   /* Max str len of month           str (e.g. September). */
#define  CLK_STR_MONTH_TRUNC_LEN                           3u   /*     Str len of month     trunc str.                  */
#define  CLK_STR_AM_PM_LEN                                 2u   /*     Str len of am-pm           str.                  */
#define  CLK_STR_DIG_TZ_HR_LEN                             2u   /*     Str len of tz hr    dig.                         */
#define  CLK_STR_DIG_TZ_MIN_LEN                            2u   /*     Str len of tz min   dig.                         */
#define  CLK_STR_DIG_TZ_MAX_LEN                            2u   /* Max str len of tz       digs.       */
#define  CLK_STR_FMT_YYYY_MM_DD_HH_MM_SS                   2u   /* Fmt date/time as "YYYY-MM-DD HH:MM:SS".              */
#define  CLK_STR_FMT_MM_DD_YY_HH_MM_SS                     3u   /* Fmt date/time as "MM-DD-YY HH:MM:SS".                */

#define  CLK_STR_FMT_YYYY_MM_DD_HH_MM_SS_LEN              20u   /*     Str len of fmt "YYYY-MM-DD HH:MM:SS".            */
#define  CLK_STR_FMT_MM_DD_YY_HH_MM_SS_LEN                18u   /*     Str len of fmt "MM-DD-YY HH:MM:SS".              */

                                                                /* See Note #1.                                         */
#define  CLK_UNIX_EPOCH_YR_START                        1970u   /* Unix epoch starts = 1970-01-01 00:00:00 UTC.         */
#define  CLK_UNIX_EPOCH_YR_END                          2106u   /*            ends   = 2105-12-31 23:59:59 UTC.         */
#define  CLK_UNIX_EPOCH_DAY_OF_WK                          5u   /*                     1970-01-01 is Thu.               */
#define  CLK_UNIX_EPOCH_OFFSET_YR_CNT          (CLK_EPOCH_YR_START - CLK_UNIX_EPOCH_YR_START)
#define  CLK_UNIX_EPOCH_OFFSET_LEAP_DAY_CNT    (CLK_UNIX_EPOCH_OFFSET_YR_CNT / 4u)

                                                                /*     30 yrs * 365 * 24 * 60 * 60 = 946080000          */
                                                                /*   +  7 leap days * 24 * 60 * 60 =    604800          */
                                                                /* CLK_UNIX_OFFSET_SEC             = 946684800          */
#define  CLK_UNIX_EPOCH_OFFSET_SEC            ((CLK_UNIX_EPOCH_OFFSET_YR_CNT       * DEF_TIME_NBR_SEC_PER_YR ) +  \
                                               (CLK_UNIX_EPOCH_OFFSET_LEAP_DAY_CNT * DEF_TIME_NBR_SEC_PER_DAY))

#define  CLK_FIRST_MONTH_OF_YR                             1u   /* First month of a yr    [1 to  12].                   */
#define  CLK_FIRST_DAY_OF_MONTH                            1u   /* First day   of a month [1 to  31].                   */
#define  CLK_FIRST_DAY_OF_YR                               1u   /* First day   of a yr    [1 to 366].                   */
#define  CLK_FIRST_DAY_OF_WK                               1u   /* First day   of a wk    [1 to   7].                   */


#define  CLK_MONTH_PER_YR                                 12u
#define  CLK_HR_PER_HALF_DAY                              12uL

#define  CLK_DAY_OF_WK_NONE                                0u

/*******************************************************************FILE REFERENCES*************************************************/ 
#define MAX_NUM_FILES                    2     // Log Files + Param File should match number defined in ObjDict_highestSubIndex_objA200-1 
#define DIRECTORY_LOC                   1024   // directory base address plus size does not cross page boundary  (must be >= SAVE_RESTORE_BUFF_SIZE)

#define FS_BUFFER_SIZE                  512
#define LOGFILE_1_BASE                  4096   // beginning of sector 0b                                              
#define PAGE_SIZE                       512
#define MAX_RECORD_LENGTH               80


#define NUM_LOG_EMPTY                   SCRIPT_MAX_STRING
#define BASE_LOG_ADDRESS                LOGFILE_1_BASE
#define END_LOG_ADDRESS                 NV_DEVICE_SIZE - NUM_LOG_EMPTY
/* ----------------- APPLICATION GLOBALS ------------------ */
extern int DirEntry;
extern int  *   pDir;

/*********************************************** Data *******************************************************************/
typedef  CPU_INT16U  CLK_YR;
typedef  CPU_INT08U  CLK_MONTH;
typedef  CPU_INT16U  CLK_DAY;
typedef  CPU_INT32U  CLK_NBR_DAYS;
typedef  CPU_INT08U  CLK_HR;
typedef  CPU_INT08U  CLK_MIN;
typedef  CPU_INT08U  CLK_SEC;
typedef  CPU_INT32S  CLK_TZ_SEC;
typedef  CPU_INT08U  CLK_STR_FMT;
typedef  CPU_INT32U  CLK_TS_SEC;

typedef  struct  clk_date_time 
{
    CLK_YR      Yr;                                             /* Yr since epoch start yr      (see Note #1).          */
    CLK_MONTH   Month;                                          /* Month     [     1 to    12], (Jan to Dec).           */
    CLK_DAY     Day;                                            /* Day       [     1 to    31].                         */
    CLK_DAY     DayOfWk;                                        /* Day of wk [     1 to     7], (Sun to Sat).           */
    CLK_DAY     DayOfYr;                                        /* Day of yr [     1 to   366].                         */
    CLK_HR      Hr;                                             /* Hr        [     0 to    23].                         */
    CLK_MIN     Min;                                            /* Min       [     0 to    59].                         */
    CLK_SEC     Sec;                                            /* Sec       [     0 to    60], (see Note #2).          */
    CLK_TZ_SEC  TZ_sec;                                         /* TZ        [-43200 to 43200], (see Note #3).          */
} CLK_DATE_TIME;




/*-------- PROTOTYPES ---------- */
// called by scripts
CPU_INT08U FileWrite ( int FileID, int index, int numberToWrite, CPU_INT32U * data);
CPU_INT08U FileRead ( int FileID, int index, int numberToRead, CPU_INT08U * data);
CPU_INT08U WriteRecordToFile ( PACKET_HEADER * pkt );
// called by CE

CPU_INT08U FlushFile( CPU_INT08U fileID);
CPU_INT08U ReadFileExternal( PACKET_HEADER * txPkt, CPU_INT08U * rxBuffer, CPU_INT08U * len );
CPU_INT08U WriteFileExternal( PACKET_HEADER * txPkt, CPU_INT08U * rxBuffer, CPU_INT08U * rxLen );
CPU_INT08U InitFiles(CPU_INT08U reset);
void BlankCheckRemoteFlash();

#endif