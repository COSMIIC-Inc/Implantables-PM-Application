
// Doxygen
/*!
** @defgroup radiomanagement Radio Management and Router
** @ingroup userapi
*/
/*! @defgroup radioprotocol Radio Protocol
** @ingroup radiomanagement
*/
/*!
** @file   gateway.h
** @author Hardway
** @date   12/10/2009
**
** @brief Prototype and structs for gateway.c. State machine for taking incoming packets from the radio and 
** either reading or writing to the local Object Dictionary or routing 
** to the CAN network. The corresponding replies are returned to the source. 
** Gateway implements the other half of the Port 2 connection (see NNP Gateway Specification).
**
** @ingroup radioprotocol
*/


#ifndef GATEWAY_H
#define GATEWAY_H



// -------- DEFINITIONS ----------

#define MAX_RADIO_BUFFER	64
#define MAX_DATA_BUFFER		50

#define MS_PER_TICK		1  //dependent on OS_CFG_TICK_RATE_HZ 
#define CAN_TIMEOUT_TICKS      35/MS_PER_TICK
#define MAX_CAN_TRANSFERS_APP  10 //maximum CAN response packets for a single CAN request 
                                  //should be at least greater of (Bootloader read buffer length) 256/8 = 8.
                                  //and SDO_MAX_LENGTH_TRANSFER/8
#define RADIO_POLL_INTERVAL                     5

typedef __packed struct 
{
	CPU_INT08U protoCtrl, counter, networkId, nodeId;
	CPU_INT08U lbIndex, hbIndex, subIndex, dataLen, data;
	
} PACKET_HEADER;

#define SIZE_PACKET_HEADER 8 //size does not include first data byte

# define NUMBER_LOG_FILES                       4

/* packet ProtoCtrl byte bits */
 #define PKT_PC_SDO_WRITE_BIT		        BIT7
 #define PKT_PC_MULTI_PKT			BIT0
 #define PKT_PC_SDO_COMM			0x00
 #define PKT_PC_NMT_MESG			BIT3
 #define PKT_PC_LSS_MESG			BIT4
 #define PKT_PC_FLASH_PROG			(BIT3 | BIT4)
 #define PKT_PC_OPTION_MASK			(BIT3 | BIT4)

/* packet ExceptionCode byte */
 #define PKT_EC_ILLEGAL_FUNCTION	1
 #define PKT_EC_ILLEGAL_ADDRESS		2
 #define PKT_EC_GATEWAY_FAILURE		4
 #define PKT_EC_RADIO_RX_NONE		5
 #define PKT_EC_RADIO_RX_NOISY		6
 #define PKT_EC_NO_RESPONSE	        7

#define MAX_GTWY_PKT_DATA			50

/**************Data ***********************************/

extern const CPU_INT08U CT_NODE_ADDRESS;
extern OS_SEM GatewayAccessControl;

// -------- PROTOTYPES ----------

void processCANGateway( void );
void updateCANGatewayState( void );
void initCANGateway( PACKET_HEADER *pkt, CPU_INT08U *rxBuffer );
CPU_INT08U runCANGateway(PACKET_HEADER *pkt, CPU_INT08U *rxBuffer, CPU_INT08U *rxLenPtr); 
//CPU_INT08U putCanPacket( PACKET_HEADER *pkt, CPU_INT08U *rxBuffer );
//CPU_INT08U getCanPacket( CPU_INT08U *rxLen );
void runRadioGateway( void );
//CPU_INT08U WriteRecordToFile ( PACKET_HEADER_DATA_PTR * pkt );
//CPU_INT08U GetDateTime(CLK_DATE_TIME * currentTime );
//CPU_INT08U WriteRecordToFileGW ( PACKET_HEADER * pkt );

extern OS_ERR postGatewaySem( void );
extern OS_ERR pendGatewaySem( OS_TICK timeout );

void WaitUntilCANGatewayAvailable( void );
void MakeCANGatewayAvailable( void );
CPU_INT08U GetBlockOD( PACKET_HEADER *pkt, CPU_INT08U * data, CPU_INT08U *rxLen );
CPU_INT08U SetBlockOD( PACKET_HEADER *pkt, CPU_INT08U * data, CPU_INT08U *rxLen );

#endif
 
