//    file: nvmemat45db321.c    .

/*
	This is a 4Mbyte dataFlash device.

	We are using the at45db321D part, at 512 bytes per page.  It offers 2 buffers
	to enable interlaced operation during writes (17-40ms per page).  Page write
	time with our 10MHz clk is 13ms, so we use interlacing to write the next-page
        of data while the current page is being burned to flash.  JML Note: to speed up 
        logging writes we assume that the entire logging space has been erased prior to use.
        Therefore, the write time is reduced significantly (3-6ms per page) by not requiring erase.
        For the OD restore space, we access the Binary buffers differently becuase we can wait 
        until the binary buffers are full before burning to flash.  For logging, we want each
        logging write to be burned to flash immediately to prevent data loss in the case of 
        unexpected power loss

	We break page rd/wr at Binary boundries, 512 instead of 528, to simplify streaming 
        and leave room for future misc counters, etc.  JML note: the double pointer (**data) 
        for rdMainMemory, rdBinaryBuffer, and wrDataToBinaryBuffer is critical because 
        these functions quit early if a page boundary is detected.  The calling function then
        uses the updated address from the pointer in a follow up call to those functions.	
	
        Call manageNvRefresh() periodically (ie. from a task loop) to run the 
        overseeing sector-page-rewrite state machine.  - JML Note: this is no longer necessary 
        because logging is constrained to be sequential access.  The OD Restore and directory 
        spaces will likely be erased and reloaded entirely at least every 20,000 cycles

	The nvMem is a shared resource and incorporates an internal Mutex mechanism.

	NOTE- Nothing is being done with protection.
*/

#include <stdio.h>
#include <stdlib.h>
#include <includes.h>

#include "sys.h"
#include "SPI_Memory.h"
#include "scripts.h"
#include "ObjDict.h"



// -------- DEFINITIONS ----------

/* SPI= 125KB, 8bits, master, clk0, phase0 */ /*300KHz*/ 
 // was 8 ( 8 is max value for rate; PCLK / SxSPCCR == rate)

#define ENABLE_NV_SPI()		{	S0SPCCR = 8 ;	\
					S0SPCR  = BIT5 | BIT11 ; }

#define SELECT_FLASH_MEM()		(IO1CLR = BIT25)
#define DESELECT_FLASH_MEM()		(IO1SET = BIT25)

#define SELECT_RAM_MEM()		(IO1CLR = BIT17)
#define DESELECT_RAM_MEM()		(IO1SET = BIT17)

#define  	SRAMRead   					0x03     	//Read Command for SRAM
#define  	SRAMWrite  					0x02     	//Write Command for SRAM
#define 	SRAMRDSR   					0x05     	//Read the status register
#define  	SRAMWRSR  					0x01     	//Write the status register
#define		SRAMByteMode	                                0x01
#define		SRAMPageMode				        0x81
#define		SRAMSeqMode					0x41
#define		SRAMPageSize				        32
#define		dummyByte					0xFF
// --------   DATA   ------------

#define NUM_NV_SECTORS			64
#define NUM_NV_PG_PER_SECTOR	        128
#define MAX_SECTOR_EP_COUNT		19500
#define NV_BIN_PAGE_MASK		(NV_BIN_PAGE_SIZE - 1)
#define NV_PGADDR_MASK			0x7ffe00 // 0x7ffc00

//============================
//    LOCAL FLASH CODE
//============================


#define COPY_MEM_TO_BUF1		0x53
#define COPY_MEM_TO_BUF2		0x55

#define WR_TO_BUF1			0x84
#define WR_TO_BUF2			0x87

#define PROG_BUF1_TO_MEM_WE		0x83
#define PROG_BUF2_TO_MEM_WE		0x86

#define PROG_BUF1_TO_MEM_NOERASE	0x88
#define PROG_BUF2_TO_MEM_NOERASE	0x89

#define COMP_BUF1_TO_MEM        	0x60
#define COMP_BUF2_TO_MEM        	0x61

#define PAGE_ERASE                      0x81
#define SECTOR_ERASE                    0x7C
#define BLOCK_ERASE                     0x50

#define RD_MAIN_MEMORY                  0xD2

#define RD_FROM_BUF1			0xD4
#define RD_FROM_BUF2			0xD6

#define RD_STATUS_REG			0xD7
#define SR_RDY			        BIT7
#define SR_COMP			        BIT6
#define SR_PROTECT		        BIT1
#define SR_SIZE_512		        BIT0

#define DUMBY					0xdb
#define SPI0_DONE()	(S0SPSR_bit.SPIF == 1)

struct
{
	enum { NVS_TEST, NVS_REWRITE } state;
	UINT8  index, page;
	UINT16 wrCount[ NUM_NV_SECTORS ];
	
} static sector;


OS_MUTEX nvMutex; //mutex to prevent access to remote Flash from multiple sources



// -------- PROTOTYPES ----------

static void rdBinaryBuffer( UINT32 *address, UINT8 **data, UINT16 *len, UINT8 bufNum );
static void clearBinaryBuffer(UINT8 bufNum);
static UINT8 comparePageWithBinaryBuffer (UINT32 page, UINT8 bufNum);
static void wrDataToBinaryBuffer( UINT32 *address, UINT8 **data, UINT16 *len, UINT8 bufNum );
static void copyPageToBuffer( UINT32 address, UINT8 bufNum );
static void progBufferToPageNoErase( UINT32 address, UINT8 bufNum );
static void progBufferToPageWithErase( UINT32 address, UINT8 bufNum );
static UINT8 rdChipStatusReg( void );
static UINT8 SRAMReadBytes(CPU_INT08U AddLB, CPU_INT08U AddHB, CPU_INT08U * data, CPU_INT16U len);
static UINT8 SRAMWriteByte(CPU_INT08U AddLB,CPU_INT08U AddHB,CPU_INT08U WriteData);
static UINT8 SRAMWriteBytes(CPU_INT08U AddLB,CPU_INT08U AddHB, CPU_INT08U * WriteData, CPU_INT16U len);
static void SRAMCommand(CPU_INT08U AddLB, CPU_INT08U AddHB, CPU_INT08U RWCmd);
static UINT8 SRAMReadStatusReg(void);
static UINT8 SRAMWriteStatusReg(unsigned char WriteVal);
static void rdMainMemory( UINT32 *address, UINT8 **data, UINT16 *len);

static UINT8 waitForFlashReady();
static UINT8 waitForFlashReadySectorErase( void );
static UINT8 waitForFlashReadyBlockErase( void );

static UINT8 TxRxSpi0( UINT8 d );

 
 
//============================
//    GLOBAL CODE
//============================

/**
@brief Initialize SPI communication to remote RAM and Flash
@param none
@return status
*/
CPU_INT08U InitNvMemory( void )
{
  OS_ERR	err;
  
  OSMutexCreate( &nvMutex, "NvMemory Mutex", &err );
  
  DESELECT_FLASH_MEM();
  DESELECT_RAM_MEM();
  
  IO1DIR |= BIT25; // CS flash
  IO1DIR |= BIT17; // CS ram
  
  // setting up SPI port for 4 lines
  PINSEL0 |= BIT8 | BIT10 | BIT12 | BIT14 ;
  
  CPU_INT08U status = rdChipStatusReg();
  
  if(!(status & BIT0))
  {
    //chip is not set to 512bytes.  Remote Flash operations will not work correctly so initialize, and trigger Reset
    InitFlashMemory();
    Reset_Module();  
  }
    
  return status;	
}
/*******************************************************************************************
 * This needs to be run once in order to configure the AT chip for memory page size.
 * the software has been written to support a page size of 512 bytes. This is not the default.
 * To avoid random data errors, this command sequence needs to be executed once in the life of
 * the chip. The action cannont be undone. See page 21, Section 11, in the Atmel data sheet.
 * 
 *    YOU MUST CYCLE POWER FOR THE CHANGE TO TAKE EFFECT.
 * 
 * 512-byte page size can be verified by checking bit0 in Status Register (should be 1)
********************************************************************************************/
void InitFlashMemory(void)
{
  
  if (waitForFlashReady() )
  {
  
    ENABLE_NV_SPI();
            
    SELECT_FLASH_MEM();
            
    TxRxSpi0( 0x3D );
    TxRxSpi0( 0x2A );
    TxRxSpi0( 0x80 );
    TxRxSpi0( 0xA6 );
    
    DESELECT_FLASH_MEM();
  
  }

}

/**
@brief Writes to remote (SPI) RAM from local flash
@param ram_address to write to
@param flash_address to copy from
@param length of data to copy 
@return status (0=success, 2= RAM address+len out of range)
*/
CPU_INT08U WriteRemoteRAMfromFlash( CPU_INT32U ram_address, CPU_INT32U flash_address, UINT16 len )
{  
  if ( ( ram_address + len ) > 0x7FFF)
    return 2;
  
  CPU_INT08U AddLB = (CPU_INT08U)(ram_address & 0xFF);
  CPU_INT08U AddHB = (CPU_INT08U)(ram_address >> 8);  
  CPU_INT08U byteBuffer;
  CPU_INT16U i = 0;    
  SRAMWriteStatusReg(SRAMSeqMode);
        
  ENABLE_NV_SPI();
  SELECT_RAM_MEM();
	//Send Write command to SRAM along with address
  SRAMCommand(AddLB,AddHB,SRAMWrite);
	//Send Data to be written to SRAM
  for (i = 0; i < len; i++)
  {
    byteBuffer = *( CPU_INT08U * )(flash_address + i); 
    TxRxSpi0(byteBuffer);
  }
	
  DESELECT_RAM_MEM();
	
  return 0;
  
}

/**
@brief Writes to remote (SPI) RAM
@param address to write to
@param pointer to data to copy
@param length of data to copy 
@return status (0=success, 2= RAM address+len out of range)
*/
CPU_INT08U WriteRemoteRAM( UINT32 address, UINT8 * data, UINT16 len )
{
  
  if ( ( address + len ) > 0x7FFF)
    return 2;
  
  CPU_INT08U AddLB = (CPU_INT08U)(address & 0xFF);
  CPU_INT08U AddHB = (CPU_INT08U)(address >> 8);
  
  SRAMWriteBytes(AddLB, AddHB, data, len);
  return 0;
  
}

/**
@brief Read from remote (SPI) RAM 
@param address to read from
@param pointer to data that will be read  
@param length of data to read 
@return status (0=success, 2= RAM address+len out of range)
*/
CPU_INT08U ReadRemoteRAM( UINT32 address, UINT8 * data, UINT16 len )
{
  if ( (address + len)> 0x7FFF)   // 32K of addressable memory in MC 23A256      
    return 2;
  
  CPU_INT08U AddLB = (CPU_INT08U)(address & 0xFF);
  CPU_INT08U AddHB = (CPU_INT08U)(address >> 8);
  
  SRAMReadBytes(AddLB, AddHB, data, len);
  
  return 0;
}

/**
@brief Write to remote (SPI) Flash
@param address to write to
@param pointer to data to write
@param length of data to write
@return status (0=success, 2= RAM address+len out of range)
*/
CPU_INT08U WriteRemoteFlash( UINT32 address, UINT8 *data, UINT16 len )
{
	// write len number of data to memory; burn each page as the BINARY page
	// boundry is crossed.  work in alternating buffers to save time.
	
	UINT8 bufNum = 0;
	UINT32 activePageAddr;
	OS_ERR	err;
	CPU_TS	ts;
	
	if ( (address + len) > NV_DEVICE_SIZE )   // 4M of addressable memory in AT45DB321D     
          return 2;
        
	OSMutexPend( &nvMutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err );
	while( err != OS_ERR_NONE );	//debug: hang / cause reset
	
	if( waitForFlashReady() )
        {
          // copy data to RAM buffer
          copyPageToBuffer( address, bufNum );
          
          for( ; len > 0 ; bufNum++ )
          {
                  activePageAddr = address;
                  // write to flash
                  
                  wrDataToBinaryBuffer( &address, &data, &len, bufNum );
                  
                  copyPageToBuffer( address, (bufNum + 1) );
                  
                  progBufferToPageNoErase( activePageAddr, bufNum );
                  
          }
        }
	
	OSMutexPost( &nvMutex, OS_OPT_PEND_BLOCKING, &err );
	while( err != OS_ERR_NONE );	//debug: hang / cause reset
        
        return 0;
		
}	

//JML Note: this is mostly in common with the WriteRemoteFlash function - consider combining with parameter
CPU_INT08U WriteRemoteFlashWithErase( UINT32 address, UINT8 *data, UINT16 len )
{
	// write len number of data to memory; burn each page as the BINARY page
	// boundry is crossed.  work in alternating buffers to save time.
	
	UINT8 bufNum = 0;
	UINT32 activePageAddr;
	OS_ERR	err;
	CPU_TS	ts;
	
	if ( (address + len) > NV_DEVICE_SIZE )   // 4M of addressable memory in AT45DB321D     
          return 2;
        
	OSMutexPend( &nvMutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err );
	while( err != OS_ERR_NONE );	//debug: hang / cause reset
	
	if( waitForFlashReady() )
        {
          // copy data to RAM buffer
          copyPageToBuffer( address, bufNum );
          
          for( ; len > 0 ; bufNum++ )
          {
                  activePageAddr = address;
                  // write to flash
                  
                  wrDataToBinaryBuffer( &address, &data, &len, bufNum );
                  
                  copyPageToBuffer( address, (bufNum + 1) );
                  
                  progBufferToPageWithErase( activePageAddr, bufNum );
                  
          }
        }
	
	OSMutexPost( &nvMutex, OS_OPT_PEND_BLOCKING, &err );
	while( err != OS_ERR_NONE );	//debug: hang / cause reset
        
        return 0;
		
}	



/**
@brief Clears both binary buffers 
@param none
@return none
*/
void ClearBinaryBuffers()
{
  clearBinaryBuffer(0);
  clearBinaryBuffer(1);
}
  





/**
@brief
This function allows you to write to both binary buffers of the remote flash.  Data splitting the page boundary
will get split between buffers.  Call ClearBinaryBuffers() prior to calling this function,
if you do not plan to fill the entire buffer.  This function is intended to be called repeatedly 
to fill up the Buffer.
address < NV_BIN_PAGE_SIZE goes into buffer 0
address > NV_BIN_PAGE_SIZE goes into buffer 1
Call WriteBinaryBuffersToRemoteFlash when Binary Buffers are filled
@param address (< NV_BIN_PAGE_SIZE*2)
@param data pointer to data
@param len length of data
@return none
*/
void FillBinaryBuffers(CPU_INT32U address, CPU_INT08U *data, CPU_INT16U len)
{
  CPU_INT08U bufNum;
  
  if (address + len > NV_BIN_PAGE_SIZE*2)
  {
    return; //error.  The address cannot fit within the two buffers
  }
  if(address < NV_BIN_PAGE_SIZE)
    bufNum = 0;
  else 
    bufNum = 1;
  
  for( ; len > 0 ; bufNum++ ) //if data is split accross two buffers, this loop will run twice, otherwise once
  {
        wrDataToBinaryBuffer( &address, &data, &len, bufNum );
  }

}

/**
@brief This function assumes you have filled both binary buffers with data and 
want to write them to consecutive pages in Flash.  
 See: FillBinaryBuffers
buffer0 -> address
buffer1 -> address + NV_BIN_PAGE_SIZE
@param address (in Remote Flash)
*/
void WriteBinaryBuffersToRemoteFlash(CPU_INT32U address)
{
  progBufferToPageWithErase( address, 0 );
  progBufferToPageWithErase( address + NV_BIN_PAGE_SIZE, 1 );
}

/**
@brief Blank Checks entire remote flash (starting at end) and 
@return first (highest) non empty page address
*/
CPU_INT08U blankCheckRemoteFlash(CPU_INT32U* address)
{
  UINT32 page = NV_NUM_PAGES;
  UINT8 result = 0;
    
  clearBinaryBuffer(0);
  
  while( page > 0)
  {    
    result = comparePageWithBinaryBuffer(--page, 0);
    
    if(result == 1)
    {
      *address = page << 9;
      return 1;
    }
     else if(result == 2)
     {
      *address = page << 9;
      return 2;
     }
  }
  *address = 0;
  return 0;
}

 
/**
@brief Read remote (SPI) flash
@param address to read from
@param data location to copy read data to
@param length of data to read
@return status (0=success, 2=address+len out of range)
*/
CPU_INT08U ReadRemoteFlash( UINT32 address, UINT8 *data, UINT16 len )
{
	// read len number of data from memory to address.  Load each page 
	// as the BINARY page boundry is crossed.

	OS_ERR	err;
	CPU_TS	ts;

        CPU_INT08U bufNum = 0;
	
	if ( (address + len) > NV_DEVICE_SIZE )   // 4M of addressable memory in AT45DB321D     
          return 2;
        
	OSMutexPend( &nvMutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err );
	while( err != OS_ERR_NONE );	//debug: hang / cause reset
	
	if( waitForFlashReady() )
        {
          //len should be > 0 first time through.  If address + initial len crosses a page boundary,
          //then len will be > 0, and address will be updated second time
          
          for( ; len > 0 ; bufNum++)
          {
           
            //JDC used this
            //copyPageToBuffer( address, bufNum );
            //rdBinaryBuffer( &address, &data, &len, bufNum );
            
            //JML: this should be fine and is much faster
            //JDC note:  reads directly - no buffer transfer  but it seems to corrupt memory  
            rdMainMemory( &address, &data, &len);
          }
        }
	
	OSMutexPost( &nvMutex, OS_OPT_PEND_BLOCKING, &err );
	while( err != OS_ERR_NONE );	//debug: hang / cause reset
	return 0;
}

//unused
CPU_INT08U EraseRemoteFlashPage( UINT32 page )
{       
  // command ==> 81h
  
  OS_ERR	err;
  CPU_TS	ts;
  BW32          addr;
  UINT8         cmd;
  
  OSMutexPend( &nvMutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err );
  while( err != OS_ERR_NONE );	
  
  if( waitForFlashReady() )
  {
    addr.w = page << 9;
    cmd = PAGE_ERASE ;
    
    
    ENABLE_NV_SPI();
    
    SELECT_FLASH_MEM();
    
    TxRxSpi0( cmd );
    TxRxSpi0( addr.b[2] );
    TxRxSpi0( addr.b[1] );
    TxRxSpi0( addr.b[0] );
    
    DESELECT_FLASH_MEM();
  }
  
  OSMutexPost( &nvMutex, OS_OPT_PEND_BLOCKING, &err );
  while( err != OS_ERR_NONE );	//debug: hang / cause reset
  
  return 0;
  
}	
CPU_INT08U EraseRemoteFlashBlock( CPU_INT16U block )
{     
  // 1024 blocks
  // command ==> 0x50
  BW32          addr;
  UINT8         cmd;
  
  addr.w = block << 12; 
  
  if( waitForFlashReady() )
  {
    
    cmd = BLOCK_ERASE;
    
    ENABLE_NV_SPI();
    
    SELECT_FLASH_MEM();
    
    TxRxSpi0( cmd );
    TxRxSpi0( addr.b[2] );
    TxRxSpi0( addr.b[1] );
    TxRxSpi0( addr.b[0] );
    
    DESELECT_FLASH_MEM();    
    
    //Block erase takes more time than other functions so wait until it's done
    waitForFlashReadyBlockErase(); 
  }
  else
  {
    return 1;
  }
  
  return 0;
}
// 
CPU_INT08U EraseRemoteFlashSector( CPU_INT08U sector )
{     
  // 64 sectors
  // command ==> 0x7C
  BW32          addr;
  UINT8         cmd;
  
  if (sector == 65) // 0a - sector 0, page 0
  {
    addr.w = 0;
  }
  else if (sector == 64) // 0b - sector 0, page 8
  {
    addr.w |= (1 << 12);
  }
  else  // 1 - 63
  {
    addr.w = sector << 16;
  }
  
  if( waitForFlashReady() )
  {
    
    cmd = SECTOR_ERASE;
    
    ENABLE_NV_SPI();
    
    SELECT_FLASH_MEM();
    
    TxRxSpi0( cmd );
    TxRxSpi0( addr.b[2] );
    TxRxSpi0( addr.b[1] );
    TxRxSpi0( addr.b[0] );
    
    DESELECT_FLASH_MEM();    
    
    //Sector erase takes more time than other functions so wait until it's done
    waitForFlashReadySectorErase();
  }
  else
  {
    return 1;
  }
  
  return 0;
}
  
#define REWRITE_MEM_VIA_BUF1	0x58
#define REWRITE_MEM_VIA_BUF2	0x59
//JML: logging is done sequentially in Sectors 0b-63 so sectors do not need to rerwrite flash
//Sector 0a (OD Restore and File Directory) is likely erased and rewritten before 20,000 cumulative sector cycles.
//This was run by runcanserver.c

void ManageRemoteFlashRefresh( void )
{
	// this routine refreshes all flash every 19,500 writes - backgroung
	
	BW32 addr;
	OS_ERR	err;
	CPU_TS	ts;
	
	
	switch( sector.state )
	{
		default:
		case NVS_TEST :
			
			if( sector.wrCount[ sector.index ] > MAX_SECTOR_EP_COUNT )
			{
				sector.state = NVS_REWRITE;
				sector.page = 0;
				
			}
			else if( ++sector.index >= NUM_NV_SECTORS )
				sector.index = 0;
			
			break;
		
			
		case NVS_REWRITE :
			
			if( sector.page < NUM_NV_PG_PER_SECTOR )
			{
				addr.b[2] = sector.index ;
				addr.b[1] = sector.page << 1;
				addr.b[0] = 0;
				addr.w <<= 1;
				
				OSMutexPend( &nvMutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err );
				while( err != OS_ERR_NONE );	//debug: hang / cause reset
				
				if( waitForFlashReady() )
                                {
	
                                  ENABLE_NV_SPI();
                  
                                  SELECT_FLASH_MEM();
                  
                                  TxRxSpi0( REWRITE_MEM_VIA_BUF1 );
                                  TxRxSpi0( addr.b[2] );
                                  TxRxSpi0( addr.b[1] );
                                  TxRxSpi0( addr.b[0] );
                                  
                                  DESELECT_FLASH_MEM();
                                  
                                  sector.page++;
                                }
				
				OSMutexPost( &nvMutex, OS_OPT_PEND_BLOCKING, &err );
				while( err != OS_ERR_NONE );	//debug: hang / cause reset
				
			}
			else
			{
				sector.wrCount[ sector.index ] = 0;
				sector.state = NVS_TEST;
			}
			
			break;
		
	}
	
}



// close the files by writing to NV Memory (NOT used)
//void CloseBuffer(CPU_INT32U pgAddress)
//{
//  OS_ERR	err;
//  CPU_TS	ts;
//  BW32 addr;
//  
//  addr.w = pgAddress;
//  OSMutexPend( &nvMutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err );
//  
//  if (waitForFlashReady() )
//  {   
//    ENABLE_NV_SPI();
//    
//    SELECT_FLASH_MEM();
//    
//    TxRxSpi0( PROG_BUF1_TO_MEM_WE  );
//    TxRxSpi0( addr.b[2] );
//    TxRxSpi0( addr.b[1] );
//    TxRxSpi0( addr.b[0] );
//    
//    DESELECT_FLASH_MEM();
//    
//    SELECT_FLASH_MEM();
//    
//    TxRxSpi0( PROG_BUF2_TO_MEM_WE  );
//    TxRxSpi0( addr.b[2] );
//    TxRxSpi0( addr.b[1] );
//    TxRxSpi0( addr.b[0] + 1 );
//    
//    DESELECT_FLASH_MEM();
//    
//    sector.wrCount[ addr.b[2] >> 1 ]++;    
//  }
//  OSMutexPost( &nvMutex, OS_OPT_PEND_BLOCKING, &err );
//}

//////////////////////////////////////////////////////////////////////
/////////////////// private calls/////////////////////////////////////
static UINT8 comparePageWithBinaryBuffer (UINT32 page, UINT8 bufNum)
{
    BW32 addr;
    UINT8 cmd;
    CPU_SR cpu_sr;
    UINT16 cnt;
    UINT8 status;
	
    addr.w = page << 9;
    cmd = (bufNum & 1)?  COMP_BUF2_TO_MEM : COMP_BUF1_TO_MEM ;
    
    if( waitForFlashReady() )
    {
    
      ENABLE_NV_SPI();
              
      SELECT_FLASH_MEM();
      CPU_CRITICAL_ENTER(); 
      
      TxRxSpi0( cmd );
      TxRxSpi0( addr.b[2] );
      TxRxSpi0( addr.b[1] );
      TxRxSpi0( addr.b[0] );
              
      CPU_CRITICAL_EXIT(); 
      DESELECT_FLASH_MEM();
    }
    //wait for compare to finish should only take 300us so we don't want to use
    //waitForFlashReady function)
    do {
      status = rdChipStatusReg();
      if(cnt++ >10000) return 2;
    } while (!(status & SR_RDY));
        
    //returns 1 if at least one bit is different, 0 if all the same
    if (status & SR_COMP)
      return 1;
    else
      return 0;
}


static void rdMainMemory( UINT32 *address, UINT8 **data, UINT16 *len)
{
	
	BW32 addr;
	UINT8 cmd;
        // no page mask - needs absolute address to fit 23 bit address
        // therefore shifted up to fill serial address.
        addr.w = *address;
	
	cmd = RD_MAIN_MEMORY;
	
        if( waitForFlashReady() )
        {
        
          ENABLE_NV_SPI();
                  
          SELECT_FLASH_MEM();
                  
          TxRxSpi0( cmd );
          TxRxSpi0( addr.b[2] );
          TxRxSpi0( addr.b[1] );
          TxRxSpi0( addr.b[0] );
          TxRxSpi0( DUMBY );
          TxRxSpi0( DUMBY );
          TxRxSpi0( DUMBY );
          TxRxSpi0( DUMBY );
                  
          for( ; *len > 0 ; )
          {
                 
                  *(*data)++ = TxRxSpi0( DUMBY );
                  (*len)-- ;
                  
                  // determine if crossing page boundary
                  if( (++(*address) & NV_BIN_PAGE_MASK) == 0 )
                          break;
          }
          
          DESELECT_FLASH_MEM();
      }
	
}

/*Unused in application*/
static void rdBinaryBuffer( UINT32 *address, UINT8 **data, UINT16 *len, UINT8 bufNum )
{
	// Break pageRead at Binary boundry - 512 instead of 528 - to simplify streaming 
	//	and leave room for wr counters.
	
	BW32 addr;
	UINT8 cmd;
	CPU_SR cpu_sr;
	
	addr.w = *address & NV_BIN_PAGE_MASK ;
	cmd = (bufNum & 1)?  RD_FROM_BUF2 : RD_FROM_BUF1 ;
	
        
	if( waitForFlashReady() )
        {
          CPU_CRITICAL_ENTER(); 
          ENABLE_NV_SPI();
                  
          SELECT_FLASH_MEM();
         
          TxRxSpi0( cmd );
          TxRxSpi0( addr.b[2] );
          TxRxSpi0( addr.b[1] );
          TxRxSpi0( addr.b[0] );
          TxRxSpi0( DUMBY );
                  
          for( ; *len > 0 ; )
          {
                  *(*data)++ = TxRxSpi0( DUMBY );
                  (*len)-- ;
                  
                  if( (++(*address) & NV_BIN_PAGE_MASK) == 0 )
                          break;
          }
          CPU_CRITICAL_EXIT(); 
          DESELECT_FLASH_MEM();
        }
	
}


static void clearBinaryBuffer(UINT8 bufNum)
{
        UINT16 i;
	UINT8 cmd;
	CPU_SR cpu_sr;

        cmd = (bufNum & 1)?  WR_TO_BUF2 : WR_TO_BUF1 ;
	
	if( waitForFlashReady() )
        {
	
          ENABLE_NV_SPI();
                  
          SELECT_FLASH_MEM();
          CPU_CRITICAL_ENTER(); 
          
          TxRxSpi0( cmd );
          TxRxSpi0( 0 );
          TxRxSpi0( 0 );
          TxRxSpi0( 0 );
                  
          for(i = NV_BIN_PAGE_SIZE ; i > 0 ; i--)
          {
              TxRxSpi0( 0xFF );
          }
          CPU_CRITICAL_EXIT(); 
          DESELECT_FLASH_MEM();
        }

}

static void wrDataToBinaryBuffer( UINT32 *address, UINT8 **data, UINT16 *len, UINT8 bufNum )
{
	// Break pageWrite at Binary boundry - 512 instead of 528 - to keep the streaming progression.
	
	BW32 addr;
	UINT8 cmd;
	CPU_SR cpu_sr;
	
	addr.w = *address & NV_BIN_PAGE_MASK ;
	cmd = (bufNum & 1)?  WR_TO_BUF2 : WR_TO_BUF1 ;
	
        
	if( waitForFlashReady() )
        {
	
          ENABLE_NV_SPI();
                  
          SELECT_FLASH_MEM();
          CPU_CRITICAL_ENTER(); 
          
          TxRxSpi0( cmd );
          TxRxSpi0( addr.b[2] );
          TxRxSpi0( addr.b[1] );
          TxRxSpi0( addr.b[0] );
                  
          for( ; *len > 0 ; )
          {
                  TxRxSpi0( *(*data)++ );
                  (*len)-- ;
                  
                  if( (++(*address) & NV_BIN_PAGE_MASK) == 0 )
                          break;
          }
          CPU_CRITICAL_EXIT(); 
          DESELECT_FLASH_MEM();
        }
	
}

static void copyPageToBuffer( UINT32 address, UINT8 bufNum )
{
	BW32 addr;
	UINT8 cmd;
	
	
	addr.w = (address) & NV_PGADDR_MASK ;
	cmd = (bufNum & 1)?  COPY_MEM_TO_BUF2 : COPY_MEM_TO_BUF1 ;
	
        
	if( waitForFlashReady() )
	{
          ENABLE_NV_SPI();
                  
          SELECT_FLASH_MEM();
                  
          TxRxSpi0( cmd );
          TxRxSpi0( addr.b[2] );
          TxRxSpi0( addr.b[1] );
          TxRxSpi0( addr.b[0] );
          
          DESELECT_FLASH_MEM();
        }
	
}

static void progBufferToPageNoErase( UINT32 address, UINT8 bufNum )
{
	BW32 addr;
	UINT8 cmd;	
	
        // changed to no erase
	addr.w = (address) & NV_PGADDR_MASK ;
	cmd = (bufNum & 1)?  PROG_BUF2_TO_MEM_NOERASE : PROG_BUF1_TO_MEM_NOERASE ;

	if (waitForFlashReady() )
        {
	
          ENABLE_NV_SPI();
                  
          SELECT_FLASH_MEM();
                  
          TxRxSpi0( cmd );
          TxRxSpi0( addr.b[2] );
          TxRxSpi0( addr.b[1] );
          TxRxSpi0( addr.b[0] );
          
          DESELECT_FLASH_MEM();
          
          sector.wrCount[ addr.b[2] >> 1 ]++;
        }
	
}
static void progBufferToPageWithErase( UINT32 address, UINT8 bufNum )
{
	BW32 addr;
	UINT8 cmd;	
	
	addr.w = (address) & NV_PGADDR_MASK ;
	cmd = (bufNum & 1)?  PROG_BUF2_TO_MEM_WE : PROG_BUF1_TO_MEM_WE ;

	if (waitForFlashReady() )
        {
	
          ENABLE_NV_SPI();
                  
          SELECT_FLASH_MEM();
                  
          TxRxSpi0( cmd );
          TxRxSpi0( addr.b[2] );
          TxRxSpi0( addr.b[1] );
          TxRxSpi0( addr.b[0] );
          
          DESELECT_FLASH_MEM();
          
          sector.wrCount[ addr.b[2] >> 1 ]++;
        }
	
}
static UINT8 rdChipStatusReg( void )
{
	UINT8 sr;
	
	
	ENABLE_NV_SPI();
		
	SELECT_FLASH_MEM();
	
	TxRxSpi0( RD_STATUS_REG );
	
	sr = TxRxSpi0( DUMBY );
	
	DESELECT_FLASH_MEM();
	
	
	return sr ;
		
}

//Max waiting time is 48ms.  Supports all flash functions except block and sector erase
static UINT8 waitForFlashReady( void )
{
	OS_ERR err;
        UINT8 retry = 0;
	
	while( !(rdChipStatusReg() & SR_RDY)  )
	{
		OSTimeDlyHMSM(0, 0, 0, 2, OS_OPT_TIME_HMSM_STRICT, &err);
                
                if (retry++ > 24)  
                  return 0;
	}
        return 1;
	
}

//Max waiting time is 7.5s.  Supports sector erase (up to 5 s)
static UINT8 waitForFlashReadySectorErase( void )
{
	OS_ERR err;
        UINT8 retry = 0;
	
	while( !(rdChipStatusReg() & SR_RDY)  )
	{
		OSTimeDlyHMSM(0, 0, 0, 100, OS_OPT_TIME_HMSM_STRICT, &err);
                
                if (retry++ > 75)  
                  return 0;
	}
        return 1;
}

//Max waiting time is 200s.  Supports block erase (up to 100 ms)
static UINT8 waitForFlashReadyBlockErase( void )
{
	OS_ERR err;
        UINT8 retry = 0;
	
	while( !(rdChipStatusReg() & SR_RDY)  )
	{
		OSTimeDlyHMSM(0, 0, 0, 5, OS_OPT_TIME_HMSM_STRICT, &err);
                
                if (retry++ > 40)  
                  return 0;
	}
        return 1;
}


//============================
//    SRAM SPECIFIC CODE
//============================

UINT8 SRAMWriteStatusReg(unsigned char WriteVal)
{
	ENABLE_NV_SPI();
	SELECT_RAM_MEM();
	TxRxSpi0(SRAMWRSR);
	
	TxRxSpi0(WriteVal);
	
	DESELECT_RAM_MEM();
	return(0);			
}

UINT8 SRAMReadStatusReg(void)
{
	CPU_INT08U ReadData;
        
        ENABLE_NV_SPI();
	SELECT_RAM_MEM();
	TxRxSpi0(SRAMRDSR);
	
	ReadData = TxRxSpi0(dummyByte);
	
	DESELECT_RAM_MEM();
	return ReadData;
}
void SRAMCommand(CPU_INT08U AddLB, CPU_INT08U AddHB, CPU_INT08U RWCmd)
{
	
	TxRxSpi0(RWCmd);
	//while(!SPI_Rx_Buf_Full);
	
	//Send High byte of address to SRAM
	TxRxSpi0(AddHB);
	//while(!SPI_Rx_Buf_Full);
	
	//Send Low byte of address to SRAM
	TxRxSpi0(AddLB);
	//while(!SPI_Rx_Buf_Full);
	
}

UINT8 SRAMWriteByte(CPU_INT08U AddLB,CPU_INT08U AddHB, CPU_INT08U WriteData)
{
	SRAMWriteStatusReg(SRAMByteMode);
        
        ENABLE_NV_SPI();
	SELECT_RAM_MEM();
	//Send Write command to SRAM along with address
	SRAMCommand(AddLB,AddHB,SRAMWrite);
	//Send Data to be written to SRAM
	TxRxSpi0(WriteData);
	
	DESELECT_RAM_MEM();
	return 0;	
}
UINT8 SRAMWriteBytes(CPU_INT08U AddLB,CPU_INT08U AddHB, CPU_INT08U * WriteData, CPU_INT16U len)
{
	CPU_INT08U tempData;
        CPU_INT16U i;
        CPU_SR cpu_sr;
        
        SRAMWriteStatusReg(SRAMSeqMode);
        
        ENABLE_NV_SPI();
	SELECT_RAM_MEM();
	//Send Write command to SRAM along with address
	SRAMCommand(AddLB,AddHB,SRAMWrite);
	//Send Data to be written to SRAM
        CPU_CRITICAL_ENTER();
	for (i = 0; i < len; i++)
        {
          tempData = *(WriteData++);
          TxRxSpi0(tempData);
        }
	CPU_CRITICAL_EXIT();
	DESELECT_RAM_MEM();
	return 0;	
}
UINT8 SRAMReadBytes(CPU_INT08U AddLB, CPU_INT08U AddHB, CPU_INT08U * data, CPU_INT16U len)
{
	CPU_INT16U i;
        CPU_INT08U readData;
        CPU_SR cpu_sr;
        
        // chip select within function call
	SRAMWriteStatusReg(SRAMSeqMode);
        
        ENABLE_NV_SPI();
	SELECT_RAM_MEM();
        
	//Send Read command to SRAM along with address
	SRAMCommand(AddLB,AddHB,SRAMRead);
	CPU_CRITICAL_ENTER();
	for (i = 0; i < len; i++)
        {
          readData = TxRxSpi0(dummyByte);
          *data++ = readData;
        }
        CPU_CRITICAL_EXIT();
	DESELECT_RAM_MEM();
	return 0;
}

//============================
//    SPI COMMON CODE
//============================

static UINT8 TxRxSpi0( UINT8 d )
{
	// R/W one byte via the SPI port, full duplex.  
	// Assumes SPCR & SPSR are configured.
    
	S0SPDR = d;								// put tx data
    while( ! SPI0_DONE() );					// wait for xfer done
    return( S0SPDR );						// get rx data 
}




