
// Doxygen
/*!
** @file   RMBootloader.c
** @author Coburn
** @date   5/1/2013
**
** @brief this file should be identical for PM and CT
** @ingroup iotasks
**
*/

#include "RMBootloader.h"

/******************************************************************************************************
*                                         Defines
*******************************************************************************************************/

#define MAX_TRANSFERS_BOOTLOADER                15
#define RM_ESTABLISH_COMM                       0
#define RM_DOWNLOAD                             2
#define RM_ERASE_MEM                            3
#define RM_RESTART_TARGET                       4
#define RM_READ_MEMORY                          5
#define RM_BLANK_CHECK                          6
#define RM_CHANGE_NODE_ADDR                     7
#define RM_CALC_CHECKSUM                        8
#define RM_SET_CHECKSUM                         9
#define RM_SET_OD_DEFAULTS                      10 
#define RM_RESTART_ALL                          15

//TimeOuts (minimum requirements as measured):
// VERYLONG
//  Erase EEPROM: ~36s 
// LONG
//  Erase Flash: ~2.1s
//  Blank Check Flash: ~2.7s
// MEDIUM
//  Write Flash: 105 ms (up to62 bytes)
//  Write EEPROM: ~505ms (up to 62 bytes)
// SHORT
//  All other commands: ~3ms
//
//CAN BL SCAN is expected to fail on many nodes, so keep the timeout short

#define CAN_BL_ULTRALONGTIMEOUT_TICKS            39900/MS_PER_TICK 
#define CAN_BL_LONGTIMEOUT_TICKS                  4000/MS_PER_TICK 
#define CAN_BL_MEDIUMTIMEOUT_TICKS                 600/MS_PER_TICK 
#define CAN_BL_SHORTTIMEOUT_TICKS                   40/MS_PER_TICK 



/*******************************************************************************************************
*                                         Static Globals40
********************************************************************************************************/
CPU_INT08U newReceivedReplyFlag = 0;
CPU_INT08U transferBuffer[40];
CPU_INT16U transferLength = 0;
CPU_INT08U flashReadCounter = 0;
CPU_INT08U flashReadBuffer[256];
CPU_INT08U flashReadFlag = 0; // write == 0, read == 1
CPU_INT08U copySize = 0;
CPU_INT08U post1MessageReplyFlag = 0;



//**************************************   RMBootDownload   *********************************
                                             
/**
 * @brief Manages CAN packets during the RM Bootload process.  This function is called
 *   within the RunGatewayTask by runRadioGateway.  
 * @details After every CAN transmit, the function stops and waits for a response using the 
 *   "WaitingForCAN" Semaphore (posted by ProcessRMBOOT).  
 * 
 * @param *pkt  the packet header
 * @param *data the data to be written or read
 * @param *dataLen data length
 */
CPU_INT08U RMBootDownload( PACKET_HEADER *pkt, CPU_INT08U * data, CPU_INT08U * dataLen )
{
   
  // return 1=done normal, 2=data is abortcode.
  // setting flashReadFlag (read == 1) implies reply messages may span multiple CAN packets.
  Message m;
  UNS8 targetNode = 0;
  UNS8 command = 0;
 // CPU_INT16U bootRetries = MAX_RETRIES_BOOTLOADER;
  CPU_INT08U status = 0;
  CPU_INT16U index = 0;
  CPU_INT08U copyIndex = 0;
  CPU_INT08U extraResponseByte = 0;
  CPU_INT16U msgCounter;
  CPU_INT08U * tempPktData;
  
  m.cob_id = 0x14 << 4;
  m.rtr = NOT_A_REQUEST;
  
  
  // init buffers and states
  
  transferLength = 0;  //usually gets set by ProcessRMBoot, indicates how many bytes have been received over CAN.  If flashReadFlag=1
  copySize = 0;        //
  flashReadCounter = 0;
  flashReadFlag = 0;
  memset(data, 0, MAX_DATA_BUFFER);
  memset(transferBuffer, 0, sizeof(transferBuffer));
  memset(flashReadBuffer, 0, sizeof(flashReadBuffer));
  post1MessageReplyFlag = 0;
  newReceivedReplyFlag = 0;
  
  //for the RM Bootloader the target node is specified in what is normally used as the OD subIndex
  targetNode = pkt->subIndex;
  tempPktData = &pkt->data;
  command = pkt->data;
  
  
 
  if ( !IS_PWR_NETWORK_ENABLED() )  
  {
    //If network is off, don't send any CAN requests, just return
    status = 3;
    return status;
  }
  switch (command)
  {
  case RM_ESTABLISH_COMM:
    {
      // get target node
      m.cob_id  = 0x140;
      m.data[0] = targetNode;
      m.data[1] = pkt->hbIndex; //SN_high
      m.data[2] = pkt->lbIndex; //SN_low
      //m.data[7] = targetNode;
      m.len = 3;
           
      canSend(0 ,&m);

      post1MessageReplyFlag = 1;
      
      msgCounter = CAN_Receive_Messages; 
      //There may be multiple responses from modules with the same node number.
      //Keep handling new messages until an error (timeout) is reported.  Only one 
      //module to respond will be reported back to the CT.  Note that multiple messages 
      //may occur within the same gateway pend/post process, so we use the RX interrupt
      //counter (CAN_Receive_Messages) to see how many total responses were received.
      while( pendGatewaySem(CAN_TIMEOUT_TICKS) == OS_ERR_NONE );
      
      if( CAN_Receive_Messages > msgCounter ) 
        extraResponseByte = CAN_Receive_Messages - msgCounter;
      else //assume rollover
        extraResponseByte = (0xFFFF - msgCounter) + CAN_Receive_Messages + 1;
      
      
      break;
    }
    case RM_DOWNLOAD:
    {
      CPU_INT08U nPackets; 
      CPU_INT08U extraBytes;
      // set memory write - download program to flash
      
      m.cob_id  = 0x146; 

      // write to lower page (or other pages if pkt->hbIndex > 0)
      if (pkt->subIndex == 0) 
      {
        m.data[0] = 0x03; //set memspace & page
        m.data[1] = 0x00; //memspace = Flash
        m.data[2] = pkt->hbIndex; //page (sepecified by byte 0 or 1 for older bootloaders)
        
      }
      // write to upper page memory (no longer neeeded...can use above)
      else if (pkt->subIndex == 2)
      {
        m.data[0] = 0x03; //set memspace & page
        m.data[1] = 0x00; //memspace = Flash
        m.data[2] = 0x01; //page = upper (is this used?)
        
      }
      // write to protected upper page memory
      else if (pkt->subIndex == 4)
      {
        m.data[0] = 0x03; //set memspace & page
        m.data[1] = 0x04; //memspace = Protected Flash
        m.data[2] = 0x01; //page = upper (is this used?)
        
      }
      // write to eeprom  
      else if (pkt->subIndex == 1)
      {
        m.data[0] = 0x03; //set memspace & page
        m.data[1] = 0x01; //memspace = EEPROM 
        m.data[2] = 0x00; //page = 0 (not used)
      }    
      else
      {
        break;
      }
      m.len = 3;
      canSend(0 ,&m);
      
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      
      // reset message receive buffers  
      
      
      transferLength = 0;
      flashReadFlag = 0; // write == 0, read == 1
      
      m.cob_id  = 0x141;
      m.data[0] = 0x00;
      m.data[1] = ((UNS8 *)tempPktData)[1];
      m.data[2] = ((UNS8 *)tempPktData)[2];
      m.data[3] = ((UNS8 *)tempPktData)[3];
      m.data[4] = ((UNS8 *)tempPktData)[4];
      copySize  = pkt->dataLen - 4; // number of bytes to write   
      m.len = 5;
      canSend(0 ,&m);

      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS); 
      
      extraBytes = copySize % 8;
      nPackets = copySize / 8 + (extraBytes > 0); 
      
      m.cob_id  = 0x142;
      for (index = 0; index < nPackets; index++)
      {
        m.data[0] = ((UNS8 *)tempPktData)[ 8 * index + 5];
        m.data[1] = ((UNS8 *)tempPktData)[ 8 * index + 6];
        m.data[2] = ((UNS8 *)tempPktData)[ 8 * index + 7];
        m.data[3] = ((UNS8 *)tempPktData)[ 8 * index + 8];
        m.data[4] = ((UNS8 *)tempPktData)[ 8 * index + 9];
        m.data[5] = ((UNS8 *)tempPktData)[ 8 * index + 10];
        m.data[6] = ((UNS8 *)tempPktData)[ 8 * index + 11];
        m.data[7] = ((UNS8 *)tempPktData)[ 8 * index + 12];
        
        if (extraBytes && index == nPackets -1)
          m.len = extraBytes;
        else
          m.len = 8;
        
        // set flag to wait on reply
        
        canSend(0 ,&m);
        post1MessageReplyFlag = 1;  //JML note: all replies except the last (when index=copySize/8) will be overwritten
        
        //last packet takes longer than the rest
        if (index == nPackets -1)
          pendGatewaySem(CAN_BL_MEDIUMTIMEOUT_TICKS);
        else
          pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      }

      break;
    }
   case RM_ERASE_MEM:
    {
     // bulk erase selected memory location   
      copySize = 1;
      // erase either flash(0), eeprom(1), or protected flash(4)
      // Erases all pages in memory space, so no page select necessary
      m.cob_id  = 0x146; 
      m.data[0] = 0x01;  //set  memspace only
      m.data[1] = pkt->subIndex; //memspace
      m.data[2] = 0x00;
      m.len = 3;
      canSend(0 ,&m);
      
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      
      m.cob_id  = 0x141;
      m.data[0] = 0x80;
      m.data[1] = 0xFF;
      m.data[2] = 0xFF;
      m.len = 3;
      
      canSend(0 ,&m);
      post1MessageReplyFlag = 1;
          
      if ( pkt->subIndex == 1 ) //EEPROM
        pendGatewaySem(CAN_BL_ULTRALONGTIMEOUT_TICKS); //JML: erase EEPROM takes a very long time
      else
        pendGatewaySem(CAN_BL_LONGTIMEOUT_TICKS); //JML: erase takes a long time
      
      break;
    }
  case RM_RESTART_TARGET:
    {
      // restart app for node that is ON
      copySize = 1;
      m.cob_id  = 0x144;
      m.data[0] = 0x03;
      m.data[1] = 0x80;
      m.data[2] = 0x00;
      m.data[3] = 0x00;
      m.len = 4;
      canSend(0 ,&m);
            
      // no reply expected from target so generate app reply using flag, and don't pend on semaphore
      newReceivedReplyFlag = 1;
      transferLength = 1; // send a 0x00 back  
      
      break;
    }
  case RM_READ_MEMORY:
    {
      
      // set memory read - use ProcessRMBootloader
      // to return the requested block of memory
      
      m.cob_id  = 0x146;     
      if (pkt->subIndex == 0) // flash lower page
      {
        m.data[0] = 0x03; //set  memspace and page 
        m.data[1] = 0x00; //memspace = Flash
        m.data[2] = 0x00; //page = lower
        
      }
      else if (pkt->subIndex == 2)
      {
        m.data[0] = 0x03; //set  memspace and page 
        m.data[1] = 0x00; //memspace = Flash
        m.data[2] = 0x01; //page = upper
        
      }
      else if (pkt->subIndex == 4)
      {
        m.data[0] = 0x03; //set  memspace and page 
        m.data[1] = 0x04; //memspace = ProtectedFlash
        m.data[2] = 0x01; //page = upper
        
      }
      else
      {
        m.data[0] = 0x03; //set  memspace and page
        m.data[1] = pkt->subIndex; //memspace = EEPROM or other
        m.data[2] = 0x00; //page = 0 (not used)
        
      }    
      m.len = 3;
      canSend(0 ,&m);

      transferLength = 0;
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      if (transferLength != 1)
      {
        status = 4;
        break;
      }
      
      transferLength = 0;
      flashReadCounter = 0;
      flashReadFlag = 1;
      
      // set address for block read
      
      m.cob_id  = 0x143;
      m.data[0] = 0x00;
      m.data[1] = ((UNS8 *)tempPktData)[1];
      m.data[2] = ((UNS8 *)tempPktData)[2];
      m.data[3] = ((UNS8 *)tempPktData)[3];
      m.data[4] = ((UNS8 *)tempPktData)[4];
      copySize  = ((UNS8 *)tempPktData)[5];
      
      if (copySize > 44)
        copySize = 44;
      
      m.len = 5;
      canSend(0 ,&m);
      
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      
      break;
    }
  case RM_BLANK_CHECK:
    {
      //JML Note: Blank Check only for Flash
      // blank check the first page
      
      m.cob_id  = 0x146; 
      m.data[0] = 0x03; //set  memspace and page
      m.data[1] = pkt->subIndex; //memspace
      m.data[2] = pkt->hbIndex; //page
      m.len = 3;
      canSend(0 ,&m);
      //wait for response and confirm 
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      
      if (transferLength != 1)
      {
          status = 4;
          break;
      }
          
        
      flashReadCounter = 0;
      flashReadFlag = 1; 
      copySize = 2; //if blank check fails, 2 bytes will be first address of failure
                    //if blank check suceeds, copySize=0
      transferLength = 0; 
      
      m.cob_id  = 0x143;
      m.data[0] = 0x80;
      m.data[1] = ((UNS8 *)tempPktData)[1];
      m.data[2] = ((UNS8 *)tempPktData)[2];
      m.data[3] = ((UNS8 *)tempPktData)[3];
      m.data[4] = ((UNS8 *)tempPktData)[4];
      m.len = 5;
      canSend(0 ,&m);
      
      pendGatewaySem(CAN_BL_LONGTIMEOUT_TICKS);
      if (transferLength == 0)
      {
          newReceivedReplyFlag = 1; //success.  Length is 2 if found non-FF bytes
      }
      break;
      
    }
  case RM_CHANGE_NODE_ADDR:
    {
      copySize = 1;
      m.cob_id  = 0x145;
      m.data[0] = targetNode;
      m.len = 1;
      
      canSend(0 ,&m);
      post1MessageReplyFlag = 1;
      
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      
      break;
    }

    case RM_CALC_CHECKSUM: 
    {
      //send address range to sum over.  
      m.cob_id  = 0x148;
      m.data[0] = ((UNS8 *)tempPktData)[1]; //APP_SIZE
      m.data[1] = ((UNS8 *)tempPktData)[2]; //APP_SIZE
      m.data[2] = ((UNS8 *)tempPktData)[3]; //APP_SIZE
      m.len = 3;
      canSend(0 ,&m);
      
      post1MessageReplyFlag = 1;
      pendGatewaySem(CAN_BL_LONGTIMEOUT_TICKS);
      
      break;
    }
    case RM_SET_CHECKSUM: 
    {
     
      m.cob_id  = 0x149;
      m.data[0] = ((UNS8 *)tempPktData)[1]; //CHECKSUM
      m.data[1] = ((UNS8 *)tempPktData)[2]; //APP_SIZE
      m.data[2] = ((UNS8 *)tempPktData)[3]; //APP_SIZE
      m.data[3] = ((UNS8 *)tempPktData)[4]; //APP_SIZE
      m.data[4] = ((UNS8 *)tempPktData)[5]; //CHECKSUM_CLK
      m.len = 5;
      canSend(0 ,&m);
      
      post1MessageReplyFlag = 1;
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      
      break;
    }
    case RM_SET_OD_DEFAULTS: 
    {
     
      m.cob_id  = 0x14A;
      m.len = 0;
      canSend(0 ,&m);
      
      post1MessageReplyFlag = 1;
      pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
      
      break;
    }
     
    case RM_RESTART_ALL:
    {
      // broadcast restart
      copySize = 0;
      m.cob_id  = 0x14F;
      m.data[0] = 0x03;
      m.data[1] = 0x80;
      m.data[2] = 0x00;
      m.data[3] = 0x00;
      m.len = 4;
      canSend(0 ,&m);
      
      // no reply expected - generate app reply, don't pend on semaphore
      newReceivedReplyFlag = 1;
      transferLength = 1;
      break;
    }
    
  default:
    status = 1;
    
  }
  
  // if an error has occured, return, otherwise process the reply
  if (status > 0)
    return status;
  
    //this loop should only pass the first iteration if reading a block of data 
    //containing more than one packet
    //in this case, wait until next packet arrives.  In this case, it is not 
    //really a retry, but rather a new packet
  for ( index = 0; index < MAX_TRANSFERS_BOOTLOADER; index++ ) 
  {

    if (newReceivedReplyFlag) // set from ProcessRMBoot, except when a reply
                              // is not expected
    {
      for ( copyIndex = 0; copyIndex < transferLength; copyIndex++)
        data[copyIndex] = flashReadBuffer[copyIndex];
      
      //add the extra response byte if nonzero
      //For RM_ESTABLISH_COMM, this indictates the number of nodes that responded 
      
      if( extraResponseByte )
      {
        data[transferLength] = extraResponseByte;
        *dataLen = transferLength+1;
      }
      else
        *dataLen = transferLength;
          
      break;     
    } 
    
    if(!flashReadFlag)  
    {
      status = 2;  //this may happen if connection failed, or if node does not exist
      break;
      
    }

    pendGatewaySem(CAN_BL_SHORTTIMEOUT_TICKS);
  }
  
 
  newReceivedReplyFlag = 0;                                               
  return status;
  
}

/**
 * @brief Lets the application handle RMboot messages.  This function is called
 *   within the RunCanServer Task.  
 * @details if "post1MessageReplyFlag" is true, then the CAN response is stored
 *   in a buffer that will in turn be sent back to the host
 *   Upon completion, this function posts to the GatewayTask Semaphore, to 
 *   indicate that the RMBootDownload process can continue.
 * @param *d  Pointer to the CAN data structure
 * @param *m  message dispatched
 */
void ProcessRMBOOT(CO_Data* d, Message * m)
{
  CPU_INT08U i;
  
  if (flashReadFlag)
  // transferLength is accumlative for flashRead
    transferLength += (*m).len;
  else
    transferLength = (*m).len;
  
  // get message reply from a write sequence
  if (post1MessageReplyFlag)
  {
     if ((*m).len > 0)
     {
        for ( i = 0; i < (*m).len; i++)
        {
          flashReadBuffer[i] = (*m).data[i];
        } 
        
     }
     else // handle case with reply and no data
     {
       transferLength = 1;
       flashReadBuffer[0] = 0x00;
     }
     newReceivedReplyFlag = 1; // set new message
     post1MessageReplyFlag = 0;
  }
  
  // get series of message replies in response to a read command
  if ((transferLength <= copySize) && flashReadFlag) 
  {
    for ( i = 0; i < (*m).len; i++)
    {
      flashReadBuffer[ i + (flashReadCounter) * 8] = (*m).data[i];
    }
    flashReadCounter++;   
    
  }
  // finished reading, set flag for reply polling
  if ((transferLength >= copySize) && flashReadFlag)
    newReceivedReplyFlag = 1;
  
 
  postGatewaySem();
  
  
}


/**
 * @brief Lets scripts scan a bootloader node without using 
 *      the bootloader UI 
 * @param nodeNumber 
 */
CPU_INT08U scanBootloaderNode(CPU_INT08U nodeNumber)
{
    Message m;
    OS_ERR err;  
        
    //check if node is selected and get serial number
    m.cob_id = 0x140;
    m.len = 3;
    m.data[0] = nodeNumber;
    m.data[1] = 0;
    m.data[2] = 0;
    
    post1MessageReplyFlag = 1;
    canSend(0, &m);
    
    err = pendGatewaySem(CAN_TIMEOUT_TICKS);
    //if nodeNumber is not on the network, this will timeout
    if( err != OS_ERR_NONE)
      return 0xFF;

    if (newReceivedReplyFlag && transferLength >= 4)
    {
        return flashReadBuffer[1]; //0 if node not selected, 1 if selected, 2 if running App  
    }
    else
      return 0xFF; //no response
}
    
/**
 * @brief Lets scripts and the application select a bootloader node without using t
 *      the bootloader UI (e.g for waking only slected nodes)
 * @param nodeNumber 
 */
   
CPU_INT08U selectBootloaderNode(CPU_INT08U nodeNumber)
{
    Message m;
    OS_ERR err;  
    
    CPU_INT08U nodeSelected;
    
    //check if node is selected and get serial number
    m.cob_id = 0x140;
    m.len = 3;
    m.data[0] = nodeNumber;
    m.data[1] = 0;
    m.data[2] = 0;
    
    post1MessageReplyFlag = 1;
    canSend(0, &m);
    
    err = pendGatewaySem(CAN_TIMEOUT_TICKS);
    //if nodeNumber is not on the network, this will timeout
    if( err != OS_ERR_NONE)
      return 1;

    if (newReceivedReplyFlag && transferLength >= 4)
    {
        nodeSelected = flashReadBuffer[1]; //0 if node not selected, 1 if selected, 2 if running App  
    }
    else
      return 1; //no response

    if(nodeSelected == 0)
    {
      //Select node using nodeNumber AND serialNumber
      m.cob_id = 0x140;
      m.len = 3;
      m.data[0] = nodeNumber;
      m.data[1] = flashReadBuffer[3]; //SN LB
      m.data[2] = flashReadBuffer[2]; //SN HB
      
      post1MessageReplyFlag = 1;
      canSend(0, &m);
      

      err = pendGatewaySem(CAN_TIMEOUT_TICKS);
      if( err != OS_ERR_NONE)
        return 1;
    
      //make sure node is selected now
      if (newReceivedReplyFlag && transferLength >= 4)
      {
          nodeSelected = flashReadBuffer[1]; //0 if node not selected, 1 if selected, 2 if running App    
      }
      else
        return 1; //no response
    }
    
    if (nodeSelected == 1) //no error, node selected
      return 0;
    else if(nodeSelected == 2) //no error, but already in App
      return 2; 
    else if (nodeSelected == 0) //could not select node
      return 4;
    else //error response 
      return 3; 
    

}
    
