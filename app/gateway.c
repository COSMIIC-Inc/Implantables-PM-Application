// Doxygen
/*!
** @file   gateway.c
** @author Hardway
** @date   12/14/2009
**
** @brief State machine for taking incoming packets from the radio and 
** either reading or writing to the local Object Dictionary or routing 
** to the CAN network. The corresponding replies are returned to the source.
** @radioprotocol
**
*/   

#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "sys.h"
#include "cc1101radio.h"
#include "canfestival.h"
#include "can_cpu.h"
#include "objdict.h"
#include "gateway.h"
#include "objacces.h"
#include "sysdep.h"
#include "RMBootloader.h"
#include "scripts.h"
#include "File_Operations.h"
#include "runcanserver.h"

#pragma data_alignment = 4	
// --------   DATA   ------------

static CPU_INT08U radioBuffer[ MAX_RADIO_BUFFER ];
static CPU_INT08U rxBuffer[ MAX_DATA_BUFFER + 2 ]; //need 2 additional bytes for encryption (assumes MAX_DATA_BUFFER+2 is a multiple of 4)

CPU_BOOLEAN  WaitingForCAN = FALSE;
CPU_BOOLEAN  WaitingForCANTimeout = FALSE;

const CPU_INT08U CT_NODE_ADDRESS = 8;
CO_Data * d  =  &ObjDict_Data;

/* ----------------- APPLICATION GLOBALS ------------------ */
OS_SEM GatewayAccessControl;


static struct
{
	enum 
	{	
		GW_DONE=0,
		GW_SDO_UPLOAD,
		GW_SDO_DOWNLOAD,
                GW_SDO_BLOCK_UPLOAD,
		GW_EXPECTING_SDO_UPLOAD_REPLY, 
		GW_EXPECTING_SDO_DOWNLOAD_REPLY, 
		GW_RECEIVED_REPLY
	
	} state;
	CPU_INT16U index;
	CPU_INT08U  subIndex, channel, nodeId;
	CPU_INT08U *txData, txDataLen;
	CPU_INT08U *rxData, rxDataLen;
	CPU_INT32U abortCode;
	
} gateway;


// -------- LOCAL PROTOTYPES ----------

void sendRadioResponse( PACKET_HEADER *txPkt, CPU_INT08U *data, CPU_INT08U dataLen );

CPU_INT08U GetLocalOD( PACKET_HEADER *pkt, CPU_INT08U *rxBuffer, CPU_INT08U *rxLen );
CPU_INT08U SetLocalOD( PACKET_HEADER *pkt, CPU_INT08U *rxBuffer, CPU_INT08U *rxLen );
CPU_INT08U GetBlockOD( PACKET_HEADER *pkt, CPU_INT08U *data, CPU_INT08U *dataLen );
void processNetworkID0( PACKET_HEADER *pkt, CPU_INT08U *rxBuffer, CPU_INT08U *rxLen );

 
//============================
//    GLOBAL CODE
//============================

//called by runcanservertask
//Posts that a CAN SDO packet is available, allowing the gateway task to continue
void processCANGateway( void )
{ 
  postGatewaySem();
}

void WaitUntilCANGatewayAvailable( void )
{
  OS_ERR err;
  CPU_TS ts;
  
  OSSemPend(&GatewayAccessControl, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
}

void MakeCANGatewayAvailable( void )
{
  OS_ERR err;
  
  OSSemPost(&GatewayAccessControl, OS_OPT_POST_1, &err);
}

/* updateCANGatewayState is called by runCANGateway */
void updateCANGatewayState( void )
{
	UNS32 len, abortCode;
	UNS8 result;
        static CPU_INT08U tempBuffer[ MAX_DATA_BUFFER ];
        CPU_INT08U i;
        
	switch( gateway.state )
	{
		case GW_SDO_UPLOAD :
		
			result = readNetworkDict( &ObjDict_Data, gateway.nodeId, gateway.index, gateway.subIndex, 0 );
			
                        if (result == 0) 
                        {
                          gateway.state = GW_EXPECTING_SDO_UPLOAD_REPLY;
                        }
			else if( result == 0xFF || result == 0xFE ) 
			{
                          gateway.abortCode = OD_NOT_MAPPABLE; // result returned 0xFF, check for OD entry
                          gateway.state = GW_DONE;
			}
                        else 
                        {
                          gateway.abortCode = 0xFFFFFFFF;
                          gateway.state = GW_DONE; //should not get here
                        }
			
			break;
                        
                 case GW_SDO_BLOCK_UPLOAD :
		
			result = readBlockNetworkDict( &ObjDict_Data, gateway.nodeId, gateway.index, \
                                                        gateway.subIndex, gateway.txDataLen, 0 );
			if (result ==0)
                        {
                          gateway.state = GW_EXPECTING_SDO_UPLOAD_REPLY ;
                        }
			else if( result == 0xFF || result == 0xFE ) 
			{
                          gateway.abortCode = OD_NOT_MAPPABLE; // result returned 0xFF, check for OD entry
                          gateway.state = GW_DONE;
			}
			else 
                        {
                          gateway.abortCode = 0xFFFFFFFF;
                          gateway.state = GW_DONE; //should not get here
                        }
			break;
			
			
		case GW_EXPECTING_SDO_UPLOAD_REPLY :
			
			len = SDO_MAX_LENGTH_TRANSFER;
			
			result = getReadResultNetworkDict( &ObjDict_Data, gateway.nodeId, tempBuffer, &len, &abortCode );
                       
			if( result == SDO_FINISHED )
			{
                          
                          //Copy data to tempBuffer
                          for (i = 0; i < len; i++)
                            gateway.rxData[i] = tempBuffer[i];
                          
                          gateway.rxDataLen = len;	
                          gateway.abortCode = 0;
                          gateway.state = GW_DONE;                        	
			}                      
                        else if (result == SDO_ABORTED_RCV || result == SDO_ABORTED_INTERNAL || result == 0xFF || result == 0xFE)      
                        {
                          gateway.rxDataLen = 0x04; 
                          
                          if (abortCode == 0)
                          {
                            gateway.abortCode = 0x06040048;    
                          } 
                          else
                          {
                            gateway.abortCode = abortCode;
                          }
                          gateway.state = GW_DONE;  

                        }
                        else if ( result == SDO_UPLOAD_IN_PROGRESS || result == SDO_BLOCK_UPLOAD_IN_PROGRESS)
                        {
                          //gateway state unchanged
                        }
                        else
                        {
                          gateway.abortCode = 0xFFFFFFFF; //shouldn't get here
                          gateway.state = GW_DONE; 
                        }
                        break;

		case GW_SDO_DOWNLOAD :
			
			result = writeNetworkDict( &ObjDict_Data, gateway.nodeId, gateway.index, \
								gateway.subIndex, gateway.txDataLen, 0, gateway.txData );		
			
                        if (result == 0)
                        {
                          gateway.state = GW_EXPECTING_SDO_DOWNLOAD_REPLY ;
                        }
                        else if( result == SDO_ABORTED_RCV || result == SDO_ABORTED_INTERNAL || result == 0xFF || result == 0xFE) 
			{
                          gateway.abortCode = SDOABT_GENERAL_ERROR; // result returned 0xFF, check for OD entry
                          gateway.state = GW_DONE;
			}
                        else
                        {
                          gateway.abortCode = 0xFFFFFFFF; //shouldn't get here
                          gateway.state = GW_DONE; 
                        }
			
			break;
		
		case GW_EXPECTING_SDO_DOWNLOAD_REPLY :
			
			result = getWriteResultNetworkDict( &ObjDict_Data, gateway.nodeId, &abortCode );

                        
                        if (result == SDO_FINISHED)
                        {
                          memset(gateway.rxData, 0, gateway.rxDataLen);
                          gateway.rxDataLen = 1;
                          gateway.abortCode = 0;
                          gateway.state = GW_DONE;
                        }
			else if (result == SDO_ABORTED_RCV || result == SDO_ABORTED_INTERNAL || result == 0xFF || result == 0xFE)
                        {
                          gateway.rxDataLen = 0x04;  
                          if (abortCode == 0)
                          {
                            gateway.abortCode = 0x06040048;    
                          }
                          else
                          {
                            gateway.abortCode = abortCode;
                          }
			  gateway.state = GW_DONE;
                       }
                       else if (result == SDO_DOWNLOAD_IN_PROGRESS || result == SDO_BLOCK_DOWNLOAD_IN_PROGRESS)
                       {
                         //gateway state unchanged
                       }
                       else
                      {
                        gateway.abortCode = 0xFFFFFFFF; //shouldn't get here
                        gateway.state = GW_DONE; 
                      }         
                      break;
                        	
		default :
		case GW_DONE :
			
			break;
	}
        
        if( gateway.state == GW_DONE )
          closeSDOtransfer (&ObjDict_Data, gateway.nodeId, SDO_CLIENT ); 
}



void initCANGateway( PACKET_HEADER *pkt, CPU_INT08U *rxBuffer)
{
       
	gateway.channel  	= pkt->networkId;		
	gateway.nodeId   	= pkt->nodeId;
	gateway.index    	= (pkt->hbIndex << 8) + pkt->lbIndex;
	gateway.subIndex 	= pkt->subIndex;
	gateway.rxData   	= rxBuffer;
	gateway.txData   	= &pkt->data;
	gateway.txDataLen       = pkt->dataLen;
        gateway.state           = GW_DONE;
	
	if( pkt->protoCtrl == 0xA4 || pkt->protoCtrl == 0xE4 )
	{
		gateway.state = GW_SDO_DOWNLOAD; //write
	}
	else if ( pkt->protoCtrl == 0x24 || pkt->protoCtrl == 0x64)
	{
		gateway.state = GW_SDO_UPLOAD; //read
	}
	else if ( pkt->protoCtrl == 0x30 )
        {
                gateway.state = GW_SDO_BLOCK_UPLOAD; 
        }
	
        
}

//CPU_INT08U getCanPacket( CPU_INT08U *rxLen )
//{       
//        if( gateway.state != GW_DONE )
//	{
//          return 0;
//        }
//        
//        if( gateway.abortCode == OD_SUCCESSFUL )
//        {
//                *rxLen = gateway.rxDataLen;
//                return 1;
//                
//        }
//        else
//        {		
//                *(CPU_INT32U *)gateway.rxData = gateway.abortCode;
//                *rxLen = 4 ;		
//                return 2;
//        }
//}

//must setup gateway structure before running
//Inputs:
//pkt: pointer to the PACKET_HEADER struct that defines the node,index,subindex, and what SDO mechanism to use
// If an SDO write (download), pkt->data and pkt->dataLen define the data to write
//rxBuffer is used to output the SDO response, rxLenPtr is used to output the SDO response length (in bytes)
//Returns:
//0: success
//1: TX error
//2: RX error
//3: Gateway Pend error
//4: Too many packets
//

CPU_INT08U runCANGateway(PACKET_HEADER *pkt, CPU_INT08U *rxBuffer, CPU_INT08U* rxLenPtr) 
{
  CPU_INT08U nPackets = MAX_CAN_TRANSFERS_APP;
  
  initCANGateway(pkt, rxBuffer);
  
  updateCANGatewayState();
  if( gateway.state == GW_DONE )
  {
    *rxLenPtr = gateway.rxDataLen; 
    return 1;
  }

  
  while ( nPackets-- ) 
  {
    OS_ERR err = pendGatewaySem(CAN_TIMEOUT_TICKS);  //               
    if( err == OS_ERR_NONE)  
    { 
      updateCANGatewayState();
      if(gateway.state == GW_DONE)
      {
        if( gateway.abortCode == OD_SUCCESSFUL )
        {
          *rxLenPtr = gateway.rxDataLen; 
          return 0;    
        }
        else
        {
          *rxLenPtr = gateway.rxDataLen;  
          return 2;
        }
      }
      //else continues at beginning of while: more CAN packets required
      //until gateway.state reaches GW_DONE.  
    }
    else //timeout
    {
      //Must close out and set abort code, because CAN gateway did not complete
      closeSDOtransfer (&ObjDict_Data, gateway.nodeId, SDO_CLIENT ); 
      gateway.state = GW_DONE;
      gateway.abortCode = SDOABT_APP_TIMEOUT;
      *rxLenPtr = gateway.rxDataLen; 
      return 3;
    }
  }//end of while
  
  //too many packets...
  //Must close out and set abort code, because CAN gateway did not complete
  closeSDOtransfer (&ObjDict_Data, gateway.nodeId, SDO_CLIENT ); 
  gateway.state = GW_DONE;
  gateway.abortCode = SDOABT_OUT_OF_MEMORY;
  *rxLenPtr = gateway.rxDataLen; 
  return 4;
  
}


//============================
//    LOCAL CODE
//============================
#define MIN_PACKET_SIZE			10  //shortest packet used including rssi/lqi bytes
#define MAX_RETRIES			150


void runRadioGateway( void )
{
	
	CPU_INT08U numBytesRead, rxLen;
	static PACKET_HEADER *pkt;
	CPU_INT32U abortCode;
       
        CPU_BOOLEAN sendResponse = TRUE;
        OS_ERR err;
       
	               
	while(DEF_TRUE)  //infinite loop
        {
         
          Status_TestValue8++;
          /* clear buffer first, then read MEI packet from radio port */
          memset( radioBuffer, '\0', sizeof(radioBuffer) );
          memset( rxBuffer, '\0', sizeof(rxBuffer) ); //JML added
          
          numBytesRead = getRadioPacket( radioBuffer ); //getRadioPacket blocks further execution until message is received
                       
          //ResetWatchDogTimer(); // reset watchdog
                                      
          if( numBytesRead >= MIN_PACKET_SIZE )  
          {
                  /* We have a valid packet and now process */
                  
                  // start with third byte
                  pkt = (PACKET_HEADER *)&radioBuffer[2] ;  
                  
                  //JML TODO: should we check pkt->networkID?
                  
                  if( (pkt->nodeId == getNodeId(&ObjDict_Data) || pkt->protoCtrl == 0x34 ) ) // local OD processing or NMT
                  {
                          /* check protocol byte first -- look for NMT message */
                    switch (pkt->protoCtrl)
                    {
                      case 0x34:  /////////////// NMT COMMANDS ////////////////////////////////////////
                          {
                              UNS8 temp[ACTIVE_NODE_COUNT + 9]; // tack on additional status info
                              memset(rxBuffer, '\0', sizeof(rxBuffer));
                              if (pkt->data == NMT_Return_Node_Table)
                              {
                                d->NMTable[getNodeId(&ObjDict_Data)] = getState(&ObjDict_Data);
                                MasterRequestNodeTable(&ObjDict_Data, temp);
                                temp[ACTIVE_NODE_COUNT] = RADIO_RSSI;
                                temp[ACTIVE_NODE_COUNT + 1] = RADIO_LQI;
                                temp[ACTIVE_NODE_COUNT + 2] = NetworkPowerControl;
                                temp[ACTIVE_NODE_COUNT + 3] = (UNS8)BatteryControl_VSYS;        //low byte
                                temp[ACTIVE_NODE_COUNT + 4] = (UNS8)(BatteryControl_VSYS >> 8); //high byte
                                Control_CurrentGroup |= duplicateNode << 3; 
                                temp[ACTIVE_NODE_COUNT + 5] = Control_CurrentGroup;
                                temp[ACTIVE_NODE_COUNT + 6] = (UNS8)Temperature;        //low byte
                                temp[ACTIVE_NODE_COUNT + 7] = (UNS8)(Temperature >> 8); //high byte
                                temp[ACTIVE_NODE_COUNT + 8] = BatteryControl_LowPowerStatus;
                                memcpy(rxBuffer, temp, ACTIVE_NODE_COUNT + 9);                                    
                                rxLen = ACTIVE_NODE_COUNT + 9;
                              }
                              else if ( pkt->data == NMT_Return_Single_Node )
                              {
                                masterRequestNodeState( &ObjDict_Data, (UNS8)pkt->nodeId );
                                MasterRequestNodeTable( &ObjDict_Data, temp );
                                rxBuffer[0] = temp[pkt->nodeId];
                                rxLen = 1;
                              }
                              else //NMT with standard response
                              {
                                rxBuffer[0] = pkt->data;
                                rxLen = 1;
                                
                                //send confirmation message before applying NMT change
                                sendRadioResponse( pkt, rxBuffer, rxLen ); 
                                                                                               
                                if ( pkt->nodeId != getNodeId(&ObjDict_Data) )  
                                {
                                  //send the NMT to the targeted module (or all modules) via CAN (must check if network is active)
                                  masterSendNMTstateChange(&ObjDict_Data, pkt->nodeId, &pkt->data);  
                                }
                                // pass through for any address value of Produce_X_Manual
                                if ( pkt->nodeId == 0x00 || pkt->nodeId == getNodeId(&ObjDict_Data) || ( pkt->data == NMT_Enter_Produce_X_Manual))
                                {
                                  ProcessNMTLocalStateChange(&ObjDict_Data, &pkt->data);    
                                }                                  
                                
                                sendResponse = FALSE; //already sent response
                              }
                            break;
                          } // end case: protoCrl == 0x34                        
                    
                      case 0x24:   /////////////// PM OD READ //////////////////////////////////////////////////
                          {   
                           GetLocalOD( pkt, rxBuffer, &rxLen ); //JML TODO: handle error response
                           break;
                          }
                       case 0x64: // SDO read - sourced from embedded
                          {   
                           GetLocalOD( pkt, rxBuffer, &rxLen );  //JML TODO: handle error response
                           break;
                          }        
                      case 0xA4:  ///////////////// PM OD WRITE /////////////////////////////////////////////////
                          {
                            SetLocalOD( pkt, rxBuffer, &rxLen ); //JML TODO: handle error response
                           break;                                 
                          }
                      case 0xE4:  //////////////// PM OD WRITE ///////////////////////////////////////////////////
                          {
                           SetLocalOD( pkt, rxBuffer, &rxLen ); //JML TODO: handle error response
                           break;                                 
                          }
                      case 0x30:  //////////////// BLOCK READ LOCAL //////////////////////////////////////////////
                          {
                            
                            GetBlockOD (pkt, rxBuffer, &rxLen ); //JML TODO: handle error response
                            break;
                          }
                      case 0xB0:  //////////////// BLOCK WRITE LOCAL //////////////////////////////////////////////
                          {
                            
                            SetBlockOD (pkt, rxBuffer, &rxLen ); //JML TODO: handle error response
                            break;
                          }
                          
                      case 0x3C:  //////////////// REMOTE MODULE DOWNLOAD /////////////////////////////////////////
                          { 
                            
                            WaitUntilCANGatewayAvailable();
                            abortCode = RMBootDownload (pkt, rxBuffer, &rxLen );
                            MakeCANGatewayAvailable();
                            
                            if (abortCode) 
                            {
                              rxBuffer[0] = 0; //pkt->subIndex;
                              rxBuffer[1] = (CPU_INT08U)abortCode;
                              rxLen = 2;
                              pkt->nodeId |= 0x80;
                              
                            }
                            break;
                          }
                    case 0x26: /////////// FILE OPERATIONS ////////////////////////////////////////////////////////////
                      {
                        

                        if ((pkt->counter >> 4) == 0xE && FlushFile(pkt->counter & 0xF) == 0) //the same functionality is achieved with NMT_Flush
                        {
                          /* Normal packet response  */
                          sendResponse = TRUE;                       
                          
                        }
                        else if ((pkt->counter >> 4) == 0xD && ReadFileExternal( pkt, rxBuffer, &rxLen) == 0)
                        {
                          /* Normal packet response   */
                          sendResponse = TRUE; 
                        }
                        else // error 
                        {                        
                          pkt->nodeId |= 0x80;
                          rxLen = 1;
                          sendResponse = TRUE;		          
                        }
                        break;
                      }
          
                      default:
                        {
                          rxBuffer[0] =   pkt->data;
                          pkt->nodeId |= 0x80;
                          rxLen = 1;                          
                        }
                                                           
                    }// end of protocol switch
                    
                    if(sendResponse)
                      sendRadioResponse( pkt, rxBuffer, rxLen ); 
                    else
                      sendResponse = TRUE;

                  } 
                  else if (pkt->networkId == 1 && pkt->nodeId != getNodeId(&ObjDict_Data)) // remote OD processing
                  {                             
                      WaitUntilCANGatewayAvailable(); 
                      
                      if(runCANGateway(pkt, rxBuffer, &rxLen))
                      { 
                        //error: copy abortcode into rxBuffer
                        rxBuffer[0] =  (CPU_INT08U)gateway.abortCode;       //LSB
                        rxBuffer[1] =  (CPU_INT08U)(gateway.abortCode >>  8);
                        rxBuffer[2] =  (CPU_INT08U)(gateway.abortCode >> 16);
                        rxBuffer[3] =  (CPU_INT08U)(gateway.abortCode >> 24); //MSB
                        rxLen = 4;	
                        pkt->nodeId |= 0x80; //specify error response
                      }
                      
                      MakeCANGatewayAvailable();
                                                                
                      sendRadioResponse( pkt, rxBuffer, rxLen );
                                                             
                  }
                                                  
                  else //JML NOTE: logically can't get here based.  Previous elseif conditions account for all possible cases
                  {
                          /* illegal networkId -- returns error and no data*/
                    pkt->protoCtrl = 0xFF;
                    pkt->nodeId |= 0x80;
                    rxLen = 0;
                    sendRadioResponse( pkt, rxBuffer, rxLen ); 		                    
                                
                  }  
          } 
          else		
          {
            // insufficient packet len - turn back on receiver
            enableRadioReceiver();    
            
          }
          
          //put PM back to sleep
          if(BatteryControl_LowPowerStatus & BIT7)
          {
            if(BatteryControl_LowPowerStatus & BIT3) //sleep task is pending
            {
              BatteryControl_LowPowerStatus|=BIT2; //set source as radio
              OSSemPost(&SleepSem, OS_OPT_POST_1, &err);
            }
          }
              
     }

}
	

//----------------------------------------
void sendRadioResponse( PACKET_HEADER *txPkt, CPU_INT08U *data, CPU_INT08U dataLen )
{
	//assume we are returning the original input buffer and header 
	
	txPkt->dataLen	= dataLen;
	memcpy( &txPkt->data, data, dataLen );
	
	sendRadioPacket( remoteAddress, (CPU_INT08U *)txPkt, dataLen + 8 );
        enableRadioReceiver(); 
}

CPU_INT08U GetLocalOD( PACKET_HEADER * pkt, CPU_INT08U * varData, CPU_INT08U *dataLen )
{
	// read local OD data to *data; send data len.
	// return 1=done normal, 2=data is abortcode.

  CPU_INT16U Index = 0;
  UNS8 subIndex;
  UNS32 abortCode;
  UNS8 type;
  CPU_INT08U status; 
  UNS32 size = 0;
  Index = (CPU_INT16U)(pkt->hbIndex) * 256 + (CPU_INT16U)(pkt->lbIndex);
  subIndex = pkt->subIndex;
  
  if (Index == 0x2053 && subIndex == 3 && TransferBuffer_Flag == 1)
  {
    for (int i = 0; i < sizeof(TransferBuffer_Copy); i++)
       varData[i] = TransferBuffer_Copy[i];
    
    varData[ sizeof(TransferBuffer_Copy)] = TransferBuffer_RadioCounter; // append packet counter to data
    
    *dataLen = sizeof(TransferBuffer_Copy) + 1;
   
    TransferBuffer_Flag = 0; // clear the read flag
   
    IO1CLR = 0x01000000;     //bit 24 Heartbeat - HBT_LED             
    
  }
  else if ( Index != 0x2053 || subIndex != 3)
  {
      abortCode = readLocalDict( &ObjDict_Data, Index, subIndex, varData, \
                         &size, &type, 0 );
  
    if( abortCode != OD_SUCCESSFUL )
    {
      *(CPU_INT32U *)varData = abortCode ;
          pkt->nodeId |= 0x80;
          status = 2;
    }
    else
    {
       /*indicate successful op */
      *dataLen = size;
      status = 1;
    }
  }
  else
  {
    varData[0] = 0xFF;
    *dataLen = 1;
    
  }
     
  return status;
}
 

CPU_INT08U SetLocalOD( PACKET_HEADER *pkt, CPU_INT08U *data, CPU_INT08U *dataLen )
{
	// write local OD data; send data len.
	// return 1=done normal, 2=data is abortcode.

  UNS16 Index;
  UNS8 Subindex;
  UNS32 abortCode;
  UNS8 checkAccess = 1;
  CPU_INT08U *pbData;
  UNS32 varsize = pkt->dataLen;
  CPU_INT08U status;
  
  
  Index= pkt->lbIndex + (256 * pkt->hbIndex);
  Subindex = pkt->subIndex;
  pbData = &pkt->data;
  CPU_INT16U size = (CPU_INT16U)pkt->dataLen;
  memset(data, 0, 4);
  
  if ( pkt->nodeId == getNodeId(&ObjDict_Data) && Index == 0x1F50)
  {
    abortCode = LoadScriptToFlash(Subindex, pbData, size, pkt->counter);
  }
  else
  {
    abortCode = writeLocalDict( &ObjDict_Data, Index, Subindex, pbData, \
                         &varsize, checkAccess ); 
  }
  
  if (abortCode)
  {
    *(CPU_INT32U *)data = abortCode ;
	varsize = 4;
  	pkt->nodeId |= 0x80;
	status = 2;
  }
  else
  {
	  /*indicate successful op */
	  varsize = 1;
	 status = 1;
  }
  
 	*dataLen = varsize;
	return status;
}

CPU_INT08U GetBlockOD( PACKET_HEADER *pkt, CPU_INT08U * data, CPU_INT08U *rxLen )
{
	// read local OD data to *data; send data len.
	// return 1=done normal, 2=data is abortcode.

  UNS16 Index;
  UNS8 Subindex;
  UNS32 abortCode;
  UNS8 checkAccess = 0;
  UNS8 type;
  UNS32 varsize = 0;
  CPU_INT08U status = 0;
  CPU_INT08U tempData[4];
  CPU_INT08U * tData = 0;
  
  Index= pkt->lbIndex + (256 * pkt->hbIndex);
  Subindex = pkt->subIndex;
  
  // get the address of data to start
  tData = data;
  
  memset(data, 0, MAX_GTWY_PKT_DATA);  // clear data
  
  for (int i = 0; i < (pkt->dataLen); i++)
  {
    varsize = 0;
    type = 0;
    
    abortCode = readLocalDict( &ObjDict_Data, Index, Subindex + i, tempData, \
                         &varsize, &type, checkAccess );
    
     if( abortCode != OD_SUCCESSFUL) // check for overrun
      {
        *(CPU_INT32U *)data = abortCode ;
	varsize = 4;
  	pkt->nodeId |= 0x80;
	status = 2;
        break;
      }
     switch (type)
           {
            case 1: // boolean
            case 2: // int8
            case 5: // uns8
              *data = tempData[0];  
              data++;
              break; 
                           
            case 3: // int16
            case 6: // uns16
              memcpy(data, tempData, 2);
              data+=2;        
              break;
              
            case 4: // int32
            case 7: // uns32
              memcpy(data, tempData, 4);
              *data =     tempData[0];
              data+=4;
              break;
           default:
             {
               break; // no string support
             }
           
           }
         
     memset(tempData, 0, sizeof(tempData));
     status = 1;
  }
 if ((data - tData) <= MAX_DATA_BUFFER)
  {
    *rxLen = data - tData; 
  }
  else 
  {
    *rxLen = 1;
    status = 3;
  }
  return status;
}

CPU_INT08U SetBlockOD( PACKET_HEADER *pkt, CPU_INT08U * data, CPU_INT08U *rxLen )
{
	// set local OD data to *data; 
	// return 1=done normal, 2=data is abortcode.

  UNS16 Index;
  UNS8 Subindex;
  UNS32 abortCode;
  UNS8 checkAccess = 1;
  UNS32 varsize = 0;
  CPU_INT08U status = 0;
  CPU_INT08U tempData[4];
  
  Index= pkt->lbIndex + (256 * pkt->hbIndex);
  Subindex = pkt->subIndex;
  
  UNS8 nSubindices = pkt->dataLen & 0x3F;
  varsize = 1 << ((pkt->dataLen & 0xC0) >> 6 );  //nbytes specified in top two bits: 0=1, 1=2, 2=4, 3=8(unused)
  
  if (varsize == 8)
    return 2;
    
  // get the address of data to start
  CPU_INT08U * pData = &pkt->data;
  
  //initialize non-error response
  *rxLen = 1;
  *data = 0;
  
  for (int i = 0; i < nSubindices; i++)
  {
   
      memcpy(tempData, pData, varsize);
      pData+=varsize;

    
      abortCode = writeLocalDict( &ObjDict_Data, Index, Subindex + i, tempData, &varsize, checkAccess ); 
     if( abortCode != OD_SUCCESSFUL ) // check for overrun
     {
	*(CPU_INT32U *)data = abortCode ;
	*rxLen = 4;
  	pkt->nodeId |= 0x80;
        status = 2;
        break;
     }

     memset(tempData, 0, sizeof(tempData)); //reset tempData
     status = 1;
  }
   
  return status;
}

void processNetworkID0( PACKET_HEADER *pkt, CPU_INT08U *rxBuffer, CPU_INT08U *rxLen )
{
  // tells the query the CAN node ID.
  
  //memset(rxBuffer, 0, MAX_DATA_BUFFER); //JML moved
  
  *rxBuffer = getNodeId(&ObjDict_Data);
  *rxLen = 1;
}


