// Doxygen
/*! 
** @defgroup radio Radio Interface
** @ingroup radiomanagement
*/
/*!
** @file   cc1101.h
** @author Hardway
** @date   12/14/2009
**
** @brief State machine for taking incoming packets from the radio and 
** either reading or writing to the local Object Dictionary or routing 
** to the CAN network. The corresponding replies are returned to the source.
**
** @ingroup radio
*/    
#ifndef _CC1101_RADIO_H
#define _CC1101_RADIO_H


// -------- DEFINITIONS ----------
#define APPCODE_ADDRESS	0x04
#define TOWER_ADDRESS	0x03

#define RADIO_TX_TIMEOUT_TICKS  50/MS_PER_TICK

// --------   DATA   ------------
extern UINT8 remoteAddress;

// -------- PROTOTYPES ----------
void initRadioConfig( void );
void sendRadioPacket( CPU_INT08U deviceID, const CPU_INT08U *data, CPU_INT08U dataLen );
CPU_INT08U getRadioPacket( CPU_INT08U *data );
void enableRadioReceiver( void );
void enableRadio_WOR();
void disableRadio_WOR();
void powerDownRadio();
CPU_BOOLEAN statusRadioWOR();
void enableRadio_ChannelLoop();
void disableRadio_ChannelLoop();
//void CheckRadioSession();
void EnableEncryption(UINT8 val);

#endif
 
