//    file: .h     HEADER FILE.

#ifndef CPUFLASH_H
#define CPUFLASH_H


// -------- DEFINITIONS ----------
#define CPU_NV_BLOCK_SIZE		        512	// options are: 512/1k/4k/8k (ocumentation says 256, but it doesn't work)
#define NUMBER_AVAILABLE_SECTORS                 7    //available for scripts (10-16)    
#define SECTOR_MIN		10
#define SECTOR_MAX		16    

// --------   DATA   ------------

extern const CPU_INT32U sectorAddresses[8];
// -------- PROTOTYPES ----------

UINT8 wrCpuNvData( CPU_INT08U sector, CPU_INT16U index, CPU_INT08U *data, CPU_INT16U numData );
CPU_INT08U rdCpuNvData( CPU_INT08U sector, CPU_INT16U index, CPU_INT08U *data, CPU_INT08U numData );
CPU_INT08U ReadLocalFlashData( CPU_INT32U nvAddress, CPU_INT08U *data, CPU_INT08U numData );
CPU_INT08U WriteSectorNvDataMultiple( CPU_INT08U sector, CPU_INT16U index, CPU_INT08U *data, CPU_INT16U numData, CPU_BOOLEAN lastPacket, CPU_BOOLEAN firstPacket );


#endif
 
