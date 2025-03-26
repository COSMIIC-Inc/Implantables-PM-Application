//   SPI_Memory: .h     HEADER FILE.

#ifndef SPI_MEMORY_H
#define SPI_MEMORY_H


// -------- DEFINITIONS ----------


#define NV_BIN_PAGE_SIZE		512
#define NV_NUM_PAGES                    8192
#define NV_NUM_BLOCKS                   1024
#define NV_DEVICE_SIZE                  NV_BIN_PAGE_SIZE*NV_NUM_PAGES  //8192 pages x 512 bytes/page = 4194304 (0x400000)


// --------   DATA   ------------


// -------- PROTOTYPES ----------

CPU_INT08U InitNvMemory( void );
CPU_INT08U WriteRemoteFlash( CPU_INT32U address, CPU_INT08U *data, CPU_INT16U len );
CPU_INT08U ReadRemoteFlash( CPU_INT32U address, CPU_INT08U *data, CPU_INT16U len );
CPU_INT08U WriteRemoteRAM( CPU_INT32U address, CPU_INT08U *data, CPU_INT16U len );
CPU_INT08U ReadRemoteRAM( CPU_INT32U address, CPU_INT08U *data, CPU_INT16U len );
CPU_INT08U WriteRemoteRAMfromFlash( CPU_INT32U ram_address, CPU_INT32U flash_address, CPU_INT16U len );
void ManageRemoteFlashRefresh( void );
CPU_INT08U EraseRemoteFlashPage( CPU_INT32U page );
void Setup_Dual_Buffer(CPU_INT32U address);
void CloseBuffer(CPU_INT32U pgAddress);
CPU_INT08U WriteToDualDataBuffer(CPU_INT32U address, CPU_INT08U * data, CPU_INT16U len);
void InitFlashMemory(void);
CPU_INT08U blankCheckRemoteFlash(CPU_INT32U* address);
CPU_INT08U EraseRemoteFlashSector( CPU_INT08U sector );
CPU_INT08U EraseRemoteFlashBlock( CPU_INT16U block );
CPU_INT08U WriteRemoteFlashWithErase( CPU_INT32U address, CPU_INT08U *data, CPU_INT16U len );
void FillBinaryBuffers(CPU_INT32U address, CPU_INT08U *data, CPU_INT16U len);
void ClearBinaryBuffers();
void WriteBinaryBuffersToRemoteFlash(CPU_INT32U address);



#endif
 
