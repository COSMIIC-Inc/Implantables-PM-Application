//    file: .c    .

#include "sys.h"
#include "bsp.h"
#include "cpuFlash.h"
#include "SPI_Memory.h"

// -------- DEFINITIONS ----------

typedef void (*IAP)(unsigned int [],unsigned int[]);

#define IAP_LOCATION 	                0x7ffffff1

// --------   DATA   ------------

IAP runIapCommand = (IAP) IAP_LOCATION;

UINT32 iapCommand[5], iapResult[2];

const CPU_INT32U sectorAddresses[8] = {0x00030000, // 10 
                                      0x00032000, // 11
                                      0x00034000, // 12
                                      0x00036000, // 13
                                      0x00038000, // 14
                                      0x0003A000, // 15
                                      0x0003C000, // 16
                                      0x0003E000  // 17 (unused, but need starting address)
                                                }; 

// -------- PROTOTYPES ----------

static UINT8 IAP_prepareSectorsForWrite( UINT8 start, UINT8 end );
static UINT8 IAP_eraseSectors( UINT8 start, UINT8 end );
static UINT8 IAP_copyRamToFlash( UINT32 flashAddr, UINT32 ramAddr, UINT32 numBytes );
static UINT8 IAP_blankCheck( UINT8 start, UINT8 end );
static UINT8 CopySectorToRAM( UINT8 sector );


//============================
//    GLOBAL CODE
//============================

/* nonVolatile data array in local cpu flash
   NOTE: it MUST be on a 512 byte boundry
*/

                    

UINT8 wrCpuNvData( CPU_INT08U sector, CPU_INT16U index, CPU_INT08U *data, CPU_INT16U numData )
{
	
        return WriteSectorNvDataMultiple( sector, index, data, numData, TRUE, TRUE);

}

/*
*********************************************************************************************************
*                                             WriteSectorNvDataMultiple()
*
* Description : Allows one setup (erase and blank check) and multiple data packet writes to fill sector. 
*               Otherwise, since the packet and wrImage size is less than the sector size (2KB), it will erase
*               previously written sector. if only one packet, then lastPAcket and firstPacket = TRUE
*               firstPacket triggers buffer copy to Remote RAM
*               lastPacket triggers sector erase and copy from RemoteRAM to local RAM followed by local RAM to Flash (both in 512B pages)
*               All intermediate packets just write to RemoteRAM
* 
* Argument(s) : Sector, index within sector,  data to write (on fixed boundary sizes), last packet indicator
*
* Return(s)   : status.
*
*
*********************************************************************************************************
*/
static UINT8 wrImage[ CPU_NV_BLOCK_SIZE ];

UINT8 WriteSectorNvDataMultiple( CPU_INT08U sector, CPU_INT16U index, CPU_INT08U *data, CPU_INT16U numData, CPU_BOOLEAN lastPacket, CPU_BOOLEAN firstPacket )
{
	
        //#pragma data_alignment=4 
        UINT32 nvAddress = 0;	
	CPU_INT08U      status=1; // 0 = ok
	CPU_SR          cpu_sr;
        CPU_INT08U sectorIndex = sector - SECTOR_MIN;
        CPU_INT16U sectorSize = sectorAddresses[ sectorIndex + 1 ] - sectorAddresses[ sectorIndex ];
        CPU_INT16U i = 0;

        
        if (sector >= SECTOR_MIN && sector <= SECTOR_MAX)
          nvAddress = sectorAddresses[ sectorIndex ];
        else
          return 1; //error: sector out of bounds
        
        // check to make sure index and following data won't exceed sector boundary
        if ( (index + numData) > sectorSize)
          return 2; //error: data crosses sector boundary
                 
        if (firstPacket)
        {
          /* Buffer sector to to scratchpad */
          status = CopySectorToRAM( sector ); //returns 0 if successful
          if (status)  
          {
            return 3; //error: copying sector to remote RAM
          }
        }
        
        //All intermediate Packets
        /* Write Data to remote RAM */
        status = WriteRemoteRAM( (CPU_INT32U)index, data, numData );  //returns 0 if successful
        if(status)
        {
          return 5; //error: writing to remote RAM
        }

        if (lastPacket)
        {
            status = 4;
            /* now erase sector  */
            CPU_CRITICAL_ENTER();
            if(IAP_prepareSectorsForWrite( sector, sector) && 
               IAP_eraseSectors( sector, sector))
            {
              status = 0;
            }
            CPU_CRITICAL_EXIT();
         
            if(status)
              return 4; //error: erasing flash sector
             
            status = 8;
            CPU_CRITICAL_ENTER();
            if(IAP_blankCheck( sector, sector))
            {
              status = 0;
            }
            CPU_CRITICAL_EXIT();
            
            if(status)
              return 8; //error: blank check

            
          /* now copy RAM back to Flash */
          for (i = 0; i < sectorSize / CPU_NV_BLOCK_SIZE; i++)
          {
            status = ReadRemoteRAM( (CPU_INT32U )(CPU_NV_BLOCK_SIZE * i), &wrImage[0], CPU_NV_BLOCK_SIZE );
            if(status)
            {
              return 6; //error: reading remote RAM
            }
            
            status = 7;
            CPU_CRITICAL_ENTER();
            if( IAP_prepareSectorsForWrite( sector, sector )
            &&	IAP_copyRamToFlash( (CPU_INT32U )(nvAddress + i * CPU_NV_BLOCK_SIZE ), (CPU_INT32U)&wrImage[0], CPU_NV_BLOCK_SIZE  ) )
            {
                    status = 0;
            }
            CPU_CRITICAL_EXIT();
            
              
            if(status)
              return 7; //error: copying remote RAM to flash sector
            
          }
        }
               
	return status;
	
}
CPU_INT08U ReadLocalFlashData( CPU_INT32U nvAddress, CPU_INT08U *data, CPU_INT08U numData )
{
	memcpy( data, (CPU_INT08U *)( nvAddress) , numData );       
        return 0;
	
}

//============================
//    LOCAL CODE
//============================

//============================
// 	 iap commands 
//============================
#define IAP_CMD_SUCCESS		0
#define IAP_CCLK_FREQ		BSP_BOARD_CCLK_FREQ / 1000      //System Frequency (CCLK) in KHz


static UINT8 CopySectorToRAM( UINT8 sector )
{
	//return 0 =successful, else 1 or 2.  
	CPU_INT08U sectorIndex = sector - SECTOR_MIN;
        CPU_INT16U sectorSize = sectorAddresses[ sectorIndex + 1 ] - sectorAddresses[ sectorIndex ];
        
	if( sector >= SECTOR_MIN
	&&	sector <= SECTOR_MAX  )
	{
          
		return WriteRemoteRAMfromFlash( 0, sectorAddresses[ sectorIndex ], sectorSize);
	}
	
	else
		return 1;
}

static UINT8 IAP_prepareSectorsForWrite( UINT8 start, UINT8 end )
{
	//return 1 =successful, else 0.  Use to verify sectors for write.
	
	if( start >= SECTOR_MIN
	&&	start <= SECTOR_MAX
	&&	end >= start
	&&	end <= SECTOR_MAX  )
	{
		iapCommand[0] = 50;
		iapCommand[1] = start;
		iapCommand[2] = end;
		
		runIapCommand( iapCommand, iapResult );
		return (iapResult[0] == IAP_CMD_SUCCESS)?  1 : 0 ;
	}
	
	else
		return 0;
}


static UINT8 IAP_eraseSectors( UINT8 start, UINT8 end )
{
    //return 1 =successful, else 0
    
    iapCommand[0] = 52;
    iapCommand[1] = start;
    iapCommand[2] = end;
    iapCommand[3] = IAP_CCLK_FREQ;
    
    runIapCommand( iapCommand, iapResult );

    return (iapResult[0] == IAP_CMD_SUCCESS)?  1 : 0 ;
}


static UINT8 IAP_blankCheck( UINT8 start, UINT8 end )
{        
        iapCommand[0] = 53;
	iapCommand[1] = start;
	iapCommand[2] = end;
	
        runIapCommand( iapCommand, iapResult );
        
        return (iapResult[0] == IAP_CMD_SUCCESS)?  1 : 0 ;
	
}

static UINT8 IAP_copyRamToFlash( UINT32 flashAddr,UINT32 ramAddr, UINT32 numBytes )
{
	//return 1 =successful, else 0
	
	iapCommand[0] = 51;
	iapCommand[1] = flashAddr;
	iapCommand[2] = ramAddr;
	iapCommand[3] = numBytes;
	iapCommand[4] = IAP_CCLK_FREQ;
	
	runIapCommand( iapCommand, iapResult );
	return (iapResult[0] == IAP_CMD_SUCCESS)?  1 : 0 ;
}



//============================
//    INTERRUPT SERVICE ROUTINES
//============================


//============================
//    HARDWARE SPECIFIC CODE
//============================


