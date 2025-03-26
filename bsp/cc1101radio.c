// Doxygen
/*!
** @file   cc1101.c
** @author Hardway
** @date   12/14/2009
**
** @brief configuration for c1101 radio chip. 
**
** The configuration is initially determined using the Smart RF Studio from TI.
** The radio is used in Addressed & variable packet mode; 250khz; 
** Set: packets <= 64 bytes; crcOn; idle-after-tx,rx; calib before tx,rx; 
** Monitor GDO0 status on separate pin for rx or tx done status. Isr if needed. 
** Chip is ready for commands on SO low (simply read Miso pin in ioxpin reg).
** @ingroup radio
** 
*/    


/*	THEORY OF OPERATION:

	Half duplex. App tasks call init, send, enable receiver, and read
	routines.  Routines run to completion in < 10msec.  The receiver must be
	re-enabled for each packet received.

	Radio Configuration:
	Using hardware supported packet handler at 100Kbits/sec.  
	Tx Data size = 1-62 bytes (including length and address byte); variable length. 
	Receiver appends rx status to received packet.
	AutoFlush rx buffer on crc error.
	Receiver discards packets on len violation or addr mismatch.
	Offer WakeOnRadio WOR mode since we are online only a few minutes / day.

	Configuration changes 6/6/2011: 
	- Sleep for 1sec, listen for packet for 1.95msec. Using 1385uSec osc stab time. about 61uA.
		Enable RC osc.  Wor has rx timeout, nonWor receives until packet found.
	- Tower streams NoOp messages (894uSec ea) for 1.1 secs; PM discards all NoOps without responding.
	- any received packet retriggers the 5.1sec go-to-sleep timeout.
	- 4byte preamble and sync; gdo0 = 6 for rx (and tx).  Use rising edge EINT0 and poll flag for 
		sync reception(or busy tx'ing) and low level on input pin for pkt received (or tx pkt sent).
	- all packet config is done in IDLE mode.
	- autoFlush rx buffer on crc error.

	Operational changes 4/4/2012: 
	- Using radio interrupt to flag app code when packet reception has begun.
	- GD0 rising edge causes ISR on synch detection; app should call getRadioPacket() after 
		receiving the semaphore.  
	- Data len is returned by getRadioPacket(); no action is required for insufficient len, else 
		process the packet and send response.  Always enable receiver to prepare for next packet.

*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <includes.h>
#include "sys.h"
#include "cc1101radio.h"
#include "objdict.h"
//#include "edc_crc.h"
#include "gateway.h"


// -------- DEFINITIONS ----------

/* hardware specific defs */
#define SELECT_RADIO()					(IO1PIN_bit.P1_23 = 0)
#define DESELECT_RADIO()				(IO1PIN_bit.P1_23 = 1)

#define RADIO_IS_BUSY()					(IO0PIN_bit.P0_18 == 1)		//SO
#define TX_IS_BUSY()					(IO0PIN_bit.P0_16 == 1)		//P0.16 EINT0
#define RX_IS_READY()					(IO0PIN_bit.P0_16 == 1)
                                                        
//Configure SPI Master at maximum rate, depending on PLL, SCLK is 313-1875 kHz
#define CONFIG_RADIO_SPI_COMM()		                {S1SPCCR_bit.COUNTER = 8; \
						  	S1SPCR_bit.MSTR =  1;		/*master*/ }

#define SELECT_IO_PIN_AS_EINT0()		        (PINSEL1_bit.P0_16 = 1)
//#define SELECT_EINT0_PIN_AS_IO()		        (PINSEL1_bit.P0_16 = 0)
#define CLR_RADIO_IRQ_FLAG()				(EXTINT_bit.EINT0 = 1) //(EXTINT = 1)
#define IS_RADIO_IRQ()				        (EXTINT_bit.EINT0 == 1)

#define ENABLE_WAKE_ON_EINT0()				(EXTWAKE_bit.EXTWAKE0 = 1)

#define ENABLE_RADIO_IRQ()				(VICIntEnable  = (1 << VIC_EINT0))
#define DISABLE_RADIO_IRQ()			        (VICIntEnClear = (1 << VIC_EINT0))

#define MAX_SPI_TIME 10000*BSP_PLL

#define CHANLOOP_DISABLED        0
#define CHANLOOP_ENABLED         1
#define CHANLOOP_DISABLE_PENDING 2
                               
/* normal defs */
enum				// Command strobes, PATABLE and FIFO addresses
{
	SRES	= 0x30,
	SRX	= 0x34,
	STX	= 0x35,
	SIDLE	= 0x36,
	SWOR	= 0x38,
        SPWD	= 0x39,
	SFRX	= 0x3A,
	SFTX	= 0x3B,
	SNOP	= 0x3D,
	PATABLE	= 0x3E,
	SFIFO	= 0x3F
};

enum				// Status Register Addresses
{
	LQI	= 0x33,
	RSSI	= 0x34,
	TXBYTES	= 0x3A,
	RXBYTES	= 0x3B,
        PKTSTATUS = 0x38
};

enum CcRegister
{
    FSCTRL1  = 0x0B,  // frequency synthesizer control.
    FSCTRL0  = 0x0C,  // frequency synthesizer control.
    FREQ2    = 0x0D,  // frequency control word, high byte.
    FREQ1    = 0x0E,  // frequency control word, middle byte.
    FREQ0    = 0x0F,  // frequency control word, low byte.
    MDMCFG4  = 0x10,  // modem configuration.
    MDMCFG3  = 0x11,  // modem configuration.
    MDMCFG2  = 0x12,  // modem configuration.
    MDMCFG1  = 0x13,  // modem configuration.
    MDMCFG0  = 0x14,  // modem configuration.
    CHANNR   = 0x0A,  // channel number.
    DEVIATN  = 0x15,  // modem deviation setting (when fsk modulation is enabled).
    FREND1   = 0x21,  // front end rx configuration.
    FREND0   = 0x22,  // front end tx configuration.
    MCSM2    = 0x16,  // main radio control state machine configuration.
    MCSM1    = 0x17,  // main radio control state machine configuration.
    MCSM0    = 0x18,  // main radio control state machine configuration.
    FOCCFG   = 0x19,  // frequency offset compensation configuration.
    BSCFG    = 0x1A,  // bit synchronization configuration.
    AGCCTRL2 = 0x1B,  // agc control.
    AGCCTRL1 = 0x1C,  // agc control.
    AGCCTRL0 = 0x1D,  // agc control.
    FSCAL3   = 0x23,  // frequency synthesizer calibration.
    FSCAL2   = 0x24,  // frequency synthesizer calibration.
    FSCAL1   = 0x25,  // frequency synthesizer calibration.
    FSCAL0   = 0x26,  // frequency synthesizer calibration.
    FSTEST   = 0x29,  // frequency synthesizer calibration.
    TEST2    = 0x2C,  // various test settings.
    TEST1    = 0x2D,  // various test settings.
    TEST0    = 0x2E,  // various test settings.
    FIFOTHR  = 0x03,  // rxfifo and txfifo thresholds.
    IOCFG2   = 0x00,  // gdo2 output pin configuration.
    IOCFG1   = 0x01,  // gdo1 output pin configuration.
    IOCFG0   = 0x02,  // gdo0 output pin configuration. refer to smartrf® studio user manual for detailed pseudo register explanation.
    SYNC1    = 0x04,  // MSB.
    SYNC0    = 0x05,  // LSB.
    PKTCTRL1 = 0x07,  // packet automation control.
    PKTCTRL0 = 0x08,  // packet automation control.
    MYADDR   = 0x09,  // device address.
    WOREVT1  = 0x1E,  // WOR event0 timeout HighByte.
    WOREVT0  = 0x1f,  // WOR event0 timeout LowByte.
    WORCTRL  = 0x20,  // WOR control.
    PKTLEN   = 0x06   // packet length.
};


struct CcInit
{
    enum CcRegister regNum;
    UINT8 value;
};



// --------   DATA   ------------

static struct
{
	enum {RS_IDLE, RS_RECEIVER, RS_TRANSMITTER} state;
	UINT8 worIsEnabled ;
        UINT8 inSession;
	UINT8 isrCnt;
	UINT8 errorCnt;
	
	OS_TICK tRef;
        UINT8 encryption;

} radio;



/*************
	Chipcon

	Product = CC1101
	Chip version = A   (VERSION = 0x04)
	Crystal accuracy = 10 ppm
	X-tal frequency = 26 MHz
	RF output power = -15 dBm

	RX filterbandwidth = 325K 
	Deviation = 47.6K	
	Datarate = 100k		
	Modulation = (1) GFSK
	Manchester enable = (0) Manchester disabled
	RF Frequency = 402.15M		
	Channel spacing = 299.9		
	IF Freq = 203.1K	

	Channel number = 0
	Optimization = Sensitivity
	Sync mode = (3) 30/32 sync word bits detected
	Format of RX/TX data = (0) Normal mode, use FIFOs for RX and TX
	CRC operation = (1) CRC calculation in TX and CRC check in RX enabled
	Forward Error Correction = (0) FEC disabled
	Length configuration = (1) Variable length packets, packet length configured by the first received byte after sync word.
	Packetlength = 61
	Preamble count = (2)  4 bytes
	Append status = 1
	Address check = (1) Address check, no broadcast
	FIFO autoflush = 1
	Device address = 190 (0xBe)
	GDO0 signal selection = ( 6) Asserts when sync word has been sent / received, and de-asserts at the end of the packet
	GDO2 signal selection = (41) CHIP_RDY
	GDO1 signal selection = (41) CHIP_RDY
	Wor on at 1sec x 1.9msec and 2.4 stabilize
*************/

struct CcInit ccInitTable[] =
{
	IOCFG2,   0x08,  //asserts when PQT reached //0x29,  //gdo2 output pin configuration; ChipRdy.
	IOCFG0,   0x06,  //gdo0 output pin configuration. high on startof_tx/rx; low on end-of-packet.
	FIFOTHR,  0x07,  //rxfifo and txfifo thresholds.
	PKTLEN,   0x3F,	 //max packet length byte value 
	PKTCTRL1, 0x0D,  //packet automation control. -- enable autoflush_CRC; append status; 
	PKTCTRL0, 0x05,  //packet automation control. -- CRC enabled (bit 2);
	MYADDR,   APPCODE_ADDRESS,  //device address.
	
	CHANNR,   0x05,  //channel number.
	FSCTRL1,  0x08,  //frequency synthesizer control.
	FSCTRL0,  0x00,  //frequency synthesizer control.
	FREQ2,    0x0f,  //frequency control word, high byte.
	FREQ1,    0x77,  //frequency control word, middle byte.
	FREQ0,    0xa2,  //frequency control word, low byte.
	MDMCFG4,  0x5b,  //modem configuration.
	MDMCFG3,  0xf8,  //modem configuration.
	MDMCFG2,  0x13,  //modem configuration.
	MDMCFG1,  0x23,  //modem configuration.
	MDMCFG0,  0x7a,  //modem configuration.
	DEVIATN,  0x47,  //modem deviation setting (when fsk modulation is enabled).
	MCSM2,    0x07,  //main radio control state machine configuration. (Don't time out until end of packet)
        MCSM1,    0x30,  //main radio control state machine configuration. (RX_OFF_MODE = IDLE)
        MCSM0,    0x18,  //main radio control state machine configuration.
	FOCCFG,   0x1D,  //frequency offset compensation configuration.
	BSCFG,    0x1C,  //bit synchronization configuration.
	AGCCTRL2, 0xC7,  //agc control.
	AGCCTRL1, 0x00,  //agc control.
	AGCCTRL0, 0xB2,  //agc control.
	WORCTRL,  0x78,	 //enable RC osc for WOR
	FREND1,   0xB6,  //front end rx configuration.
	FREND0,   0x10,  //front end tx configuration.
	FSCAL3,   0xEA,  //frequency synthesizer calibration.
	FSCAL2,   0x2A,  //frequency synthesizer calibration.
	FSCAL1,   0x00,  //frequency synthesizer calibration.
	FSCAL0,   0x1F,  //frequency synthesizer calibration.
	FSTEST,   0x59,  //frequency synthesizer calibration.
	TEST2,    0x88,  //various test settings.
	TEST1,    0x31,  //various test settings.
	TEST0,    0x0b   //various test settings.
		
};


#define NUM_CCINIT_REGS		(sizeof( ccInitTable ) / sizeof( struct CcInit ))

UINT32 comm_tRef;
UINT32 receiveTickCounter = 0;
UINT32 lastReceiveTick;
UINT8 txRxDone;
UINT8 remoteAddress;

                                 //reg      #  dBm     mA
const UINT8 ccValueLookUp[47] = { 0x6F,  // 0 -15.50  12.4
                                  0x6E,  // 1  -8.90  12.8
                                  0x6D,  // 2  -7.70  13
                                  0x6C,  // 3  -7.10  13.2
                                  0x6B,  // 4  -6.50  13.3
                                  0x6A,  // 5  -5.90  13.5
                                  0x69,  // 6  -5.30  13.6
                                  0x68,  // 7  -4.70  13.8
                                  0x67,  // 8  -4.10  14
                                  0x57,  // 9  -4.00  14.1
                                  0x66,  //10  -3.50  14.2
                                  0x56,  //11  -3.30  14.3
                                  0x55,  //12  -2.80  14.5
                                  0x64,  //13  -2.30  14.7
                                  0x54,  //14  -2.20  14.8
                                  0x53,  //15  -1.50  15
                                  0x52,  //16  -0.90  15.3
                                  0x40,  //17  -0.80  15.4
                                  0x61,  //18  -0.50  15.6
                                  0x51,  //19  -0.30  15.7
                                  0x60,  //20   0.10  15.9 *Bootloader/Default Setting
                                  0x50,  //21   0.40  16
                                  0x8D,  //22   1.40  16.8
                                  0x8C,  //23   1.90  17.1
                                  0x8B,  //24   2.30  17.3
                                  0x8A,  //25   2.80  17.6
                                  0x89,  //26   3.20  17.9
                                  0x88,  //27   3.60  18.2
                                  0x87,  //28   4.00  18.5
                                  0x86,  //29   4.40  18.8
                                  0x85,  //30   4.80  19.1
                                  0x84,  //31   5.10  19.4
                                  0x83,  //32   5.50  19.7
                                  0x82,  //33   5.80  20
                                  0x81,  //34   6.00  20.3
                                  0x80,  //35   6.30  20.6
                                  0xCA,  //36   6.40  23.4
                                  0xC9,  //37   6.80  23.8
                                  0xC8,  //38   7.10  24.2
                                  0xC7,  //39   7.40  24.7
                                  0xC6,  //40   7.80  25.2
                                  0xC5,  //41   8.10  25.7
                                  0xC4,  //42   8.50  26.3
                                  0xC3,  //43   8.80  26.9
                                  0xC2,  //44   9.20  27.6
                                  0xC1,  //45   9.50  28.3
                                  0xC0   //46   9.90  29.1
                                  };

// -------- PROTOTYPES ----------

static void configRadioControlPins( void );
static UINT8 readStatusRegister( UINT8 regNum );
static void flushFifosSetIdle( void );

static UINT8 wrConfigurationReg( UINT8 regAddr, UINT8 value );
static UINT8 readRegisters( UINT8 regAddr, UINT8 *buf, UINT8 numRegisters );
static UINT8 writeRegisters( UINT8 regAddr, UINT8 *buf, UINT8 numRegisters );

static void startTxRx( UINT8 cmd );
//static UINT8 isTxRxDone( void );
//static void waitForTxRxDone( void );

static UINT8 sendHeader( UINT8 regAddr, UINT8 numRegisters );
static UINT8 txRx_spi1( UINT8 d );
static void delayThisTask( UINT32 td_mSec );

static void configRadioInterrupt( void );
void Radio_ISR(void);
void btea(UINT32 *v, int n, UINT32 const key[4]);

 
//============================
//    DEFINES
//============================

/* read status once and use macros to decipher it, to reduce 
	the number of cc reads since the reads affect the chip. see errata pdf */

#define SEND_COMMAND_STROBE_GET_RXFIFO(cmd)		readRegisters( (cmd), NULL, 0 )
#define SEND_COMMAND_STROBE_GET_TXFIFO(cmd)		writeRegisters( (cmd), NULL, 0 )

#define MAX_DATALEN			60		// 64 -len-addr-rssi-lqi bytes
#define TXRX_MODE_MS		        2               

#define RX_SYNC_TIMEOUT			0x07
#define WOR_SYNC_TIMEOUT		0x18  RX_TIME_RSSI, RX_TIME_QUAL//06

OS_SEM RadioISR_Sem;

    UINT32 p[15];
    const UINT32 k[4] = {0x1EA098D4, 0x8FAECA4B, 0x2BBCF0DA, 0xFA12E8E4};
/**********************************************************************************************************
*                                             initRadioConfig()
**********************************************************************************************************/
/**
* @brief  Sets up radio including interrupt
* @todo The init routine is called when changing the address/chan or power settings
*                     This isn't necessary.  The semaphore is therefore recreated as well
*
* @param none
* @return none
*
*/
void initRadioConfig( void )
{
	// force init since the magnet reset won't affect the radio
	OS_ERR err;
	CPU_INT08U   ccValue;
	struct CcInit *reg;
	
	configRadioControlPins();     
	
	/* reset radio after PON for > 1mSec */
	DESELECT_RADIO();
	delayThisTask( 10 );
	
	SEND_COMMAND_STROBE_GET_RXFIFO( SRES );
        delayThisTask( 1 );
	
	/* copy configuration registers */
	for( reg = ccInitTable ; reg < &ccInitTable[ NUM_CCINIT_REGS ] ; reg++ )
        {
          // substitute values from the OD
          if (reg->regNum == MYADDR)
              reg->value = RADIO_LocalAddress;
          if (reg->regNum == CHANNR)
              reg->value = RADIO_ChannelNumber;
	
          // now write the registers
	  wrConfigurationReg( reg->regNum, reg->value );
        }
                
	// check for range
        if(RADIO_TXPower > sizeof(ccValueLookUp))
          RADIO_TXPower = sizeof(ccValueLookUp) - 1;
        
        /* set output Power level -- OD value is the index into the TXPowertable */
	ccValue = ccValueLookUp[RADIO_TXPower];					
	writeRegisters( PATABLE, &ccValue, 1 );
 
	
	/* verify configuration registers */
	
	for( reg = ccInitTable ; reg < &ccInitTable[ NUM_CCINIT_REGS ] ; reg++ )
	{
		readRegisters( reg->regNum, &ccValue, 1 );
		if( ccValue != reg->value )
			ccValue++;				//trap error here
	}
        
        remoteAddress = RADIO_RemoteAddress;
	
        OSSemCreate(&RadioISR_Sem, "Radio Semaphore", 0, &err); //JML: is it OK to call this when changing address/power and on startup?!
	configRadioInterrupt();
        if(BatteryControl_PowerControl & BIT6)
          enableRadio_WOR(); //Start radio in WOR mode
        else
          enableRadioReceiver();
        
        radio.inSession = FALSE; 
        radio.encryption = 0;
	
}

void EnableEncryption(UINT8 val)
{
  radio.encryption = val;
}
/**********************************************************************************************************
*                                             Radio_ISR()
**********************************************************************************************************/
/**
* @brief Called on completed reception or transmission of message
*       Specifically, the ISR is called on a falling edge of GDO0
*
* @param none
* @return none
*/
void Radio_ISR(void)
{
    OS_ERR err;
 
    DISABLE_RADIO_IRQ();
    

   OSSemPost(&RadioISR_Sem, OS_OPT_POST_1, &err); 
    
    VICVectAddr = 0x0; // Acknowledge that ISR has finished execution
}

/**********************************************************************************************************
*                                             enableRadioReceiver()
**********************************************************************************************************/
/**
* @brief reenables Recieve mode or WOR mode.  This function should be called after any
* packet is received or sent as the radio returns to idle mode by default.
* @param none
* @return none
*/
void enableRadioReceiver( void )
{
  CPU_SR cpu_sr;
  
  CPU_CRITICAL_ENTER();
  flushFifosSetIdle();
  
  if( radio.worIsEnabled)
  {
    startTxRx( SWOR );
  }
  else
  {
    startTxRx( SRX );
  }
  CPU_CRITICAL_EXIT();
	
}

/**********************************************************************************************************
*                                             sendRadioPacket()
**********************************************************************************************************/
/**
 * @brief using variable data len (0-60), addressed messages. Limiting data to 60 
 *   so the receiving radio can tack on rssi + lqi bytes. Radio returns to Idle state, 
 *   Receive mode must be reenabled by the calling function
 * @param deviceID
 * @param *data
 * @param dataLen: packet struct: Pktlen +addr +data[ dataLen = 0-60 bytes ].   PktLen 
 *                includes the address byte, so it is dataLen + 1.
 */ 
void sendRadioPacket( UINT8 deviceID, const UINT8 *data, UINT8 dataLen )
{
  CPU_INT08U	msg[MAX_DATALEN+2] = {0}; //need up to 62 bytes 
	OS_ERR err;
        CPU_TS ts;
   
        //encryption	               
        //UINT32 p[15];
        //const UINT32 k[4] = {0x1EA098D4, 0x8FAECA4B, 0x2BBCF0DA, 0xFA12E8E4};
        UINT8 pad=0; //number of dummy bytes added 
        static UINT16 cnt; //counter so that each outgoing message changes.  
        
	
	if( radio.state != RS_IDLE )
		radio.errorCnt++;
	
	
	if( dataLen > MAX_DATALEN )
		return;
	
	//ELSE..
	
        
        memcpy( &msg[2], data, dataLen ); //copy from pointer into msg
        
        if(radio.encryption)
        {
            dataLen+=2;  //increase dataLen to account for counter
            pad = (4 - dataLen%4)&0x03;
            dataLen += pad;
            if( dataLen > MAX_DATALEN || dataLen%4 != 0 ) //dataLen must be a multiple of 4 now
		return;
              

          //IO0SET = BIT0;  //JML DEBUG - See IOInit in app.c for debug usage
          //n = dataLen/4; //for encrypted packets, always UINT32 (4 byte chunks)      
          
          cnt++; //increment counter (we need a byte that changes every packet so packet won't be encrypted same way twice)
          if(cnt>0x3FFF) {cnt = 0;}
          
          //add counter and padding indicator as last 2 bytes
          msg[dataLen]=cnt & 0xFF;  
          msg[dataLen+1]=(pad<<6) | (cnt>>8);  //last byte

          memcpy(p,&msg[2], dataLen); //copy to p
          btea(p,dataLen/4,k);        //encrypt
          memcpy( &msg[2], p, dataLen ); //copy back to msg
                
        }
                
        /* build header */
	msg[0] = dataLen + sizeof( deviceID ) ;
	msg[1] = deviceID ; //address
        
	/* load Tx buffer */
	writeRegisters( SFIFO, msg, dataLen + 2 );
	
	startTxRx( STX );	
	
        OSSemPend(&RadioISR_Sem, RADIO_TX_TIMEOUT_TICKS, OS_OPT_PEND_BLOCKING, &ts, &err); 

        if(err == OS_ERR_NONE)
          RADIO_TX_Sent++;  // increment counter
        else
          radio.errorCnt++;
        
	
}


#define MIN_PKT_LEN		4					// len, addr, data, rssi, lqi


/**********************************************************************************************************
*                                             getRadioPacket()
**********************************************************************************************************/
/**
* @brief When this function is called, it will block the calling task until a radio packet is received    
*       Receiving a packet returns the radio to Idle.  enableRadioReceiver must be called to go back to RX or WOR mode
*	
*	We are using variable len, addressed messages.
*	packet struct: Pktlen +addr +data(1-58bytes) +rssi +lqi.
*	PktLen count includes the address+rssi+lqi, so it is data count + 5.
*	Rssi and Lqi are appended by receiver chip.
*	
*	In WakeOnRadio (WOR) mode the receiver is in RX mode for a short duty cycle. The device sending the packet must send a preamble 
*      that is sufficiently long to insure that the receiving radio wakes up, and stays in RX mode to hear the sent message
*      Once WOR is enabled, the radio will go back to WOR mode after sending a response.  WOR mode
*      is on by default if BIT6 of BatteryControl_PowerControl is set
*
* @param *data pointer to location to stored packet data.  Since this function loads the radio FIFO into data, data must allow 64 bytes 
* @return number of bytes received(including status), else 0 if none or w/errors
*/
UINT8 getRadioPacket( UINT8 *data )
{
	
    UINT8 len = 0 ;
    OS_ERR err;
    CPU_TS ts;
    static OS_TICK sessionStartTick;
    OS_TICK sessionTicks, delayTicks;
    OS_TICK sessionMaxTicks = (RADIO_SessionLength * 1000)/MS_PER_TICK;
    OS_TICK preambleMaxTicks = ((RADIO_WakeInterval + 4)*10 + 5)/MS_PER_TICK;
    
    while(RADIO_SessionLength > 0)
    { 
      if(!radio.inSession)
      {
        enableRadio_ChannelLoop(); 
        sessionStartTick = OSTimeGet(&err); 
      }
    
      //calculate time since session started 
      sessionTicks = OSTimeGet(&err) - sessionStartTick; //rollover case is still valid
      
      if(sessionTicks > sessionMaxTicks)
      {
        radio.inSession = FALSE;
      }
      else
      {
        //normally delay however much time is remaining in the session
        delayTicks = sessionMaxTicks - sessionTicks;
        
        //if not yet in Session, delay the samller of maximum preamble time and remaining session time
        if(!radio.inSession && preambleMaxTicks < delayTicks)
        {
          delayTicks = preambleMaxTicks;
        }
        BatteryControl_LowPowerStatus |= BIT4; //radio is pending
        OSSemPend(&RadioISR_Sem, delayTicks, OS_OPT_PEND_BLOCKING, &ts, &err);
        BatteryControl_LowPowerStatus &=~ BIT4; //radio is not pending
        if (err == OS_ERR_NONE) 
        {
          //got packet, but radio doesn't count as in session until packet is confirmed
          break; 
        }
        else //timed out (either done with session or wasn't in session yet and didn't get packet after finding preamble), go back to channel loop
        {
          radio.inSession = FALSE;
        }
      }     
    }
    
    if(RADIO_SessionLength == 0)
    {
      BatteryControl_LowPowerStatus |= BIT4; //radio is pending
      OSSemPend(&RadioISR_Sem, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
      BatteryControl_LowPowerStatus &=~ BIT4; //radio is not pending
    }
    

    //We can get here for 3 reasons:
    //1. normal receive (indicated by radio interupt line (falling edge at end of packet)
    //2*. preamble detection while in channel loop (rising edge when preamble quality reached, shouldn't happen )
    //3*. transmit complete ( this shouldn't happen because we shouldn't call getRadioPacket until transmit is complete)

    if( radio.state != RS_RECEIVER ) 
    {
       radio.errorCnt++; //shouldn't get here
       return 0;
    }
    
    radio.state = RS_IDLE;                  //change state to IDLE
                     
    /* packet is in fifo, read all data */

    len = readStatusRegister( RXBYTES );
    if( len < MIN_PKT_LEN )
    {
        /* receiver glitch */
        //if (len > 0)
        //{
          RADIO_RX_Rec_Partial++;
        //}
         
        return 0;
    }		
    // ELSE.. good message (address and CRC correct)

    readRegisters( SFIFO, data, len );

    RADIO_RSSI = data[len - 2]; 	// last two bytes of received data
    RADIO_LQI  = data[len - 1];
    data[len - 1] = 0;              // zero out
    data[len - 2] = 0;
    
    if( !(RADIO_LQI & 0x80) ) //JML: should never happen because CRC autoflushing is enabled
    {
      RADIO_Hard_Bad_CRC++;
      return 0;                
    }
    
    RADIO_RX_Rec++;
    if(RADIO_SessionLength > 0)
    {
      sessionStartTick = OSTimeGet(&err); 
      radio.inSession = TRUE;
    }
    
    if(radio.encryption)
    {
    //decryption

      INT8 n;
      UINT8 pad;
      
      //IO0SET = BIT0;  //JML DEBUG - See IOInit in app.c for debug usage
      memcpy(p, &data[2], len-4);
      n = (len-4)/4;
      btea(p, -n, k);
      memcpy(&data[2], p, len-4);
      pad = (data[len-3])>>6;
      //make packet consistent with unencrypted packets (remove padding and counter)
      data[len-pad-4]=data[len-2];
      data[len-pad-3]=data[len-1];
      data[0]-=(pad+2);
      len-=(pad+2);
      //IO0CLR = BIT0;  //JML DEBUG - See IOInit in app.c for debug usage  
    }
    
    return len; 
    
}

////This is called by an ISR so we can't do much here
//void CheckRadioSession()
//{
//  OS_ERR err;   
//  
//  //only increment the counter if the Radio Session Length specifier is enabled and this hasn't already tried to end a session
//  if(RADIO_SessionLength>0 && radio.sessionEnded == FALSE && receiveTickCounter++ > (UINT32)RADIO_SessionLength*1000)
//  {
//    //post the semaphore usually used for radio packets received 
//      receiveTickCounter = 0;
//      radio.sessionEnded = TRUE;
//      OSSemPost(&RadioISR_Sem, OS_OPT_POST_1, &err); 
//  }
//    
//}


void enableRadio_ChannelLoop()
{
        OS_ERR err;
        UINT8 stat;
        CPU_TS ts;
          

        wrConfigurationReg( PKTCTRL1, 0xED ); //set PQT=7 (29 bits must be read to wake up)
        wrConfigurationReg( IOCFG0, 0x08  ); //EVENT0 detects preamble
        
	
        /* Config IntPin using BUG WORKAROUND SEE EP_21---.PDF ERRATA SHEET */
	UINT32 apbdiv = APBDIV;
	APBDIV = 0;				// bug fix
	EXTPOLAR_bit.EXTPOLAR0 = 1;		// rising edge 
	APBDIV = 1;				// bug fix
	APBDIV = apbdiv;		        // bug fix
        
        
        while(1)
        {
            enableRadioReceiver();
            
            //now wait to listen for a preamble 
            BatteryControl_LowPowerStatus |= BIT4; //radio is pending
            OSSemPend(&RadioISR_Sem, (RADIO_WakeInterval + 2)/MS_PER_TICK, OS_OPT_PEND_BLOCKING, &ts, &err); 
            BatteryControl_LowPowerStatus &=~ BIT4; //radio is not pending
            
            if(err == OS_ERR_NONE)
            {
              stat = readStatusRegister(PKTSTATUS); //make sure preamble quality was reached
              if (stat & BIT5)
              {
                //radio heard the preamble and should now wait for sync and packet
                wrConfigurationReg( PKTCTRL1, 0x6D ); //set PQT back to original settings from WOR
                wrConfigurationReg( IOCFG0, 0x06); //EVENT0 detects sync and end of packet
                
                
                /* Config IntPin using BUG WORKAROUND SEE EP_21---.PDF ERRATA SHEET */
                UINT32 apbdiv = APBDIV;
                APBDIV = 0;				// bug fix
                EXTPOLAR_bit.EXTPOLAR0 = 0;		// falling edge 
                APBDIV = 1;				// bug fix
                APBDIV = apbdiv;		        // bug fix
                
                startTxRx( SRX );//if a full packet is received, the chanloop will be ended, otherwise it will be reenabled
               
                return;
              }
            }
            
            //we didn't hear a preamble on that channel so go to next channel
            RADIO_ChannelNumber++;
            if(RADIO_ChannelNumber==10)
            {
              RADIO_ChannelNumber=0;
            }
            wrConfigurationReg(CHANNR, RADIO_ChannelNumber);
            
          
        }
        

}



//void disableRadio_ChannelLoop()
//{
//    //the interrupt is triggered either because preamble was detected and went into RX mode, or preamble was detected but lost
//    //in the latter case, this function does nothing, and the channel loop is still enabled
//        UINT8 stat = readStatusRegister(PKTSTATUS);
//        if (stat & BIT5)
//        {
//          //if a full packet is received, the chanloop will be disabled, otherwise it will be reenabled
//          radio.channelLoop = CHANLOOP_DISABLE_PENDING; 
//          
//          wrConfigurationReg( PKTCTRL1, 0x6D ); //set PQT back to original settings from WOR
//          wrConfigurationReg( IOCFG0, 0x06); //EVENT0 detects sync and end of packet
//          
//          
//          /* Config IntPin using BUG WORKAROUND SEE EP_21---.PDF ERRATA SHEET */
//          
//          UINT32 apbdiv = APBDIV;
//          
//          APBDIV = 0;				// bug fix
//          EXTPOLAR_bit.EXTPOLAR0 = 0;		// falling edge 
//          APBDIV = 1;				// bug fix
//          
//          APBDIV = apbdiv;		// bug fix
//          
//          enableRadioReceiver();
//        }
//}

//from: https://en.wikipedia.org/wiki/XXTEA clarified version
  #define DELTA 0x9e3779b9
  #define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))
  
  void btea(UINT32 *v, int n, UINT32 const key[4]) {
    UINT32 y, z, sum;
    UINT16 p, rounds, e;
    if (n > 1) {          /* Coding Part */
      rounds = 12 + 52/n; //replace 6 with 12?
      sum = 0;
      z = v[n-1];
      do {
        sum += DELTA;
        e = (sum >> 2) & 3;
        for (p=0; p<n-1; p++) {
          y = v[p+1]; 
          z = v[p] += MX;
        }
        y = v[0];
        z = v[n-1] += MX;
      } while (--rounds);
    } else if (n < -1) {  /* Decoding Part */
      n = -n;
      rounds = 12 + 52/n; //replace 6 with 12?
      sum = rounds*DELTA;
      y = v[0];
      do {
        e = (sum >> 2) & 3;
        for (p=n-1; p>0; p--) {
          z = v[p-1];
          y = v[p] -= MX;
        }
        z = v[n-1];
        y = v[0] -= MX;
        sum -= DELTA;
      } while (--rounds);
    }
  }

/**********************************************************************************************************
*                                             statusRadioWOR()
**********************************************************************************************************/
/**
 *@brief Check if WOR is enabled and radio ready to receive.  
 *@return true if enabled and ready
 */
CPU_BOOLEAN statusRadioWOR()
{
  return (radio.worIsEnabled == 1 && radio.state == RS_RECEIVER);
}

/**********************************************************************************************************
*                                             enableRadio_WOR()
**********************************************************************************************************/
/**
 *@brief Enable Wake-On-Radio mode using the wake interval specified by RADIO_WakeInterval
 *       adjusts the duty cycle
 *         minimum time between wakeups is 14 ms, max time is 1.89s 
 *         at lower wake intervals, we need higher duty cycles
 *         at higher wake intervals, we need longer transmitting preamble
 *         (the transmitting preamble should be a few ms longer than the wake interval)
 *@return none
 */
void enableRadio_WOR()
{
	radio.worIsEnabled = 1;
        UINT8 worDutyCycle;
        UINT16 event;

        if(RADIO_WakeInterval < 28) 
        {
          worDutyCycle = 0x02; //3.125%
          
          if(RADIO_WakeInterval < 15)  
            RADIO_WakeInterval = 14;
        }
        else if(RADIO_WakeInterval < 56)
          worDutyCycle = 0x03; //1.562%
        else if(RADIO_WakeInterval < 112)
          worDutyCycle = 0x04; //0.781%
        else if(RADIO_WakeInterval < 224)
          worDutyCycle = 0x05; //0.391%
        else
        {
          worDutyCycle = 0x06; //0.195%
          
          if(RADIO_WakeInterval > 1890)
            RADIO_WakeInterval = 1890;
        }
        event = ( ((UINT32)RADIO_WakeInterval)*2600 )/75; //maximum is 65520
                
        wrConfigurationReg( WOREVT1, (UINT8)(event >> 8  )); //EVENT0 HB
        wrConfigurationReg( WOREVT0, (UINT8)(event & 0xFF)); //EVENT0 LB
        wrConfigurationReg( MCSM2, 0x08  + worDutyCycle ); //disable RX_RSSI_QUAL, enable RX_TIME_QUAL, and set duty cycle
        wrConfigurationReg( PKTCTRL1, 0x6D ); //set PQT=3 (13 bits must be read to wake up)
	
        enableRadioReceiver();
}

/**********************************************************************************************************
*                                             disableRadio_WOR()
**********************************************************************************************************/
/**
 *@brief Disable Wake-On-Radio mode 
 *@return none
 */
void disableRadio_WOR()
{
	radio.worIsEnabled = 0;
        
        wrConfigurationReg( MCSM2, 0x07 );    //disable RX_RSSI_QUAL, RX_TIME_QUAL, and don't timeout until end of packet
        wrConfigurationReg( PKTCTRL1, 0x0D ); //set PQT=0, no minimum Preamble Quality required
	
}

/**********************************************************************************************************
*                                            powerDownRadio()
**********************************************************************************************************/
/**
 *@brief Turns off radio.
 *@return none
 */
void powerDownRadio()
{
      SEND_COMMAND_STROBE_GET_RXFIFO( SIDLE );
      SEND_COMMAND_STROBE_GET_RXFIFO( SPWD );
}



//============================
//    LOCAL CODE
//============================

static UINT8 wrConfigurationReg( UINT8 regAddr, UINT8 value )
{
	return writeRegisters( regAddr, &value, 1 );
	
}

static UINT8 readStatusRegister( UINT8 regNum )
{
	UINT8 status;
	
	readRegisters( regNum | 0xC0, &status, 1 );
	
	return status;
}


static UINT8 readRegisters( UINT8 regAddr, UINT8 *buf, UINT8 numRegisters )
{
	// read num config registers from cc1101 device starting at address
	// return chip status
	//^^ SPI1 is not a shared resource, so no blocking required
	
	UINT8 status;
	CPU_SR cpu_sr;
        
	SELECT_RADIO();
        if (numRegisters > 1)
          CPU_CRITICAL_ENTER();
	status = sendHeader( (regAddr | BIT7), numRegisters );
	
	for( ; numRegisters > 0 ; numRegisters-- )
		*buf++ = txRx_spi1( 0x41 ); //why 0x41??
        if (numRegisters > 1)
          CPU_CRITICAL_EXIT();
	DESELECT_RADIO();
	
	return status;
}


static UINT8 writeRegisters( UINT8 regAddr, UINT8 *buf, UINT8 numRegisters )
{
	// write num config registers to cc1101 device starting at address
	// return chip status
	//^^ SPI1 is not a shared resource, so no blocking required
        CPU_SR cpu_sr;
	
	UINT8 status;
	
	SELECT_RADIO();
        if (numRegisters > 1)
          CPU_CRITICAL_ENTER();
	status = sendHeader( regAddr, numRegisters );
	
	for( ; numRegisters > 0 ; numRegisters-- )
		status = txRx_spi1( *buf++ );
        if (numRegisters > 1)
          CPU_CRITICAL_EXIT();	
	DESELECT_RADIO();
	
	return status;
}


static UINT8 sendHeader( UINT8 regAddr, UINT8 numRegisters )
{
	// return chip status
	
	UINT8 header, burst;
	
	/* build header info */
	burst = (numRegisters > 1) ?  BIT6 : 0 ;
	header = regAddr | burst ;
	
	/* wait for the radio to be ready by reading SOut low after CS */
	while( RADIO_IS_BUSY() ) ;
	
	/* config spi master, send header, read data */
	//JML: this only needs done once and is moved to configRadioControlPins CONFIG_RADIO_SPI_COMM();
	
	return txRx_spi1( header );
}


static void flushFifosSetIdle( void )
{
	SEND_COMMAND_STROBE_GET_RXFIFO( SIDLE );
	SEND_COMMAND_STROBE_GET_RXFIFO( SFRX );
	SEND_COMMAND_STROBE_GET_TXFIFO( SFTX );

}

static void startTxRx( UINT8 cmd )
{
	// call with cmd == STX, SRX or SWOR.
	DISABLE_RADIO_IRQ();
	
	SEND_COMMAND_STROBE_GET_TXFIFO( cmd );
	CLR_RADIO_IRQ_FLAG();
	
	if( cmd == SWOR || cmd == SRX )
          radio.state = RS_RECEIVER;
	else
          radio.state = RS_TRANSMITTER;
        
        ENABLE_RADIO_IRQ();
		
}



static void delayThisTask( UINT32 td_mSec )
{
	OS_ERR  err;

	OSTimeDlyHMSM(0, 0, 0, td_mSec,  OS_OPT_TIME_HMSM_STRICT, &err);	
}




//============================
//    HARDWARE SPECIFIC CODE
//============================

#define SPI1_DONE()	(S1SPSR_bit.SPIF == 1)

static UINT8 txRx_spi1( UINT8 d )
{
	// R/W one byte via the SPI port, full duplex.  
	// Assumes SPCR & SPSR are configured.
    //^^ SPI1 is not a shared resource, so no blocking required
	UINT16 timeout = 0; //timeout counter
	
	S1SPDR = d;								// put tx data
        while( ! SPI1_DONE() )					// wait for xfer done
        {
          if(timeout++ > MAX_SPI_TIME)
          {
            asm("NOP");
            CONFIG_RADIO_SPI_COMM();	
            break;
          }
        }
    return( S1SPDR );						// get rx data 
	
}

static void configRadioControlPins( void )
{
	// PINSEL2 NOTE: the nCS pin P1.23is a trace pin and is selected at PON as IO 
	// because of a pullup on P1.20.
	//^^ SPI1 is not a shared resource, so no blocking required
	
	 
	/* set chip select pin */
	IO1PIN_bit.P1_23 = 1;
	IO1DIR_bit.P1_23  = 1;
	
	/* set spi1 port pins */
	PINSEL1 |= BIT3 | BIT5 | BIT7 | BIT9 ; 
        
        CONFIG_RADIO_SPI_COMM();

}

/**********************************************************************************************************
*                                             configRadioInterrupt()
**********************************************************************************************************/
/**
* @brief Configure the radio interrupt
* ISR disables itself, and is enabled when starting TX or RX/WOR
* GDO0 output configuration must be set to 6:
*       TX: Asserts when sync word has been sent and de-asserts at the end of the packet. 
*              Also de-asserts if the TX FIFO underflows.
*       RX: Asserts when the sync word has been received and de-asserts at the end of the packet 
*              Also de-asserts if the optional address check fails or the RX FIFO overflows.
* We trigge ron the falling edge of GDO0, indicating that packet sending or receiving is complete.
* GDO0 is connected to EINT0 on processor (external interrupt) allowing it to wake up the processor from powerdown
*
* @todo ISR will be triggered on wrong address and bad CRC.  Check this
* 
*/
static void configRadioInterrupt( void )
{
	 UINT32 apbdiv;
	
	 
	/* select EINT0 function for p0.16 */
	SELECT_IO_PIN_AS_EINT0(); //PINSEL1 |= BIT0;  
        ENABLE_WAKE_ON_EINT0();
	
	
	/* Config IntPin using BUG WORKAROUND SEE EP_21---.PDF ERRATA SHEET */
	
	apbdiv = APBDIV;
	
	APBDIV = 0;				// bug fix
	EXTMODE_bit.EXTMODE0 = 1;			// edge sensitivity for EINT0
	APBDIV = 1;				// bug fix
	
	APBDIV = apbdiv;		// bug fix
	
        CLR_RADIO_IRQ_FLAG();   //clear appropriate EXTINT bit after setting mode or polatrity
	
        /************ Setup ISR for receive interrupt using EINT0 on PO.16 ********************/
        /**************************************************************************************/
        
        VICIntSelect &= ~(1 << VIC_EINT0);                         /* select as not FIQ   */
        VICVectAddr14  = (CPU_INT32U)Radio_ISR;              /* Set the vector address        */
        VICVectCntl14  = 0x20 | VIC_EINT0;                          /* Enable vectored interrupts    */
       //Interrupt will be enabled separately
	
}



