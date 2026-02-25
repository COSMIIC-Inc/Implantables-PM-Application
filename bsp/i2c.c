//    file: .c    .

#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "sys.h"
#include "i2c.h"


// -------- DEFINITIONS ----------

#define BIT_AA			BIT2		//I2CONSET REG
#define BIT_SI			BIT3
#define BIT_STO			BIT4
#define BIT_STA			BIT5
#define BIT_I2EN		BIT6

#define BIT_AAC			BIT2		//I2CONCLR REG
#define BIT_SIC			BIT3
#define BIT_STAC		BIT5
#define BIT_I2ENC		BIT6

//#define BIT_			BIT

#define TWI_BUSY()		( !(I2CONSET & BIT_SI) )

#define MAX_TWI_TIME	        1000L*BSP_PLL 


// --------   DATA   ------------

static UINT16 maxTestTime;



// -------- PROTOTYPES ----------


//============================
//    GLOBAL CODE
//============================

//=====================================
//    TwoWireInterface TWI ROUTINES
//=====================================

/* theory of operation
We are using a bit rate of 8M/(16 +20) = 41us for 9 bits; so we set
a timeout for 60 * 12clockCycles = 90us in the routines where we
could hang up waiting for a response.

I2C control registers are affected only by writing 1's to bit positions,
and are unaffected by writing 0's.  ORing won't work (compiler REG_bit.n, eg)
and tends to affect any bit read and OR'd back that is a 1.   Therefore 
just say reg = the bits you want to affect.
*/



void initTwiHdwr( void )
{
  // ENABLE I2C ctrl, set bit rate = 10MHz /4 / (23+23) = 96KHz   
  //   battery chip 27200 is 100KHz max
  //Only one I2C port, so must support minimum speed, even though temp sensor ADC could operate at 400kHz
  
  
  PINSEL0_bit.P0_2 = 1;
  PINSEL0_bit.P0_3 = 1;
  
  I2SCLH = I2SCLL = 13*BSP_PLL;  // 10MHz /4 / (13+13) = 96KHz ]
  
  //I2CONCLR = BIT_STAC | BIT_SIC | BIT_AAC;
  //I2CONSET = BIT_I2EN; //JML now enabling and disabling in start and stop functions, to fix occasional bus errors

  
}


UINT8 startTwi(UINT8 type)
{
  UINT16 timeout=0;
  UINT8 status = 0;
  
  I2CONCLR = BIT_STAC | BIT_SIC | BIT_AAC; //clear  all
  I2CONSET = BIT_I2EN; //JML test
  I2CONSET = BIT_STA; //set start bit to send start condtition
 
  while( TWI_BUSY() )
  {
    if( timeout++ > MAX_TWI_TIME )
    {
      if(stopTwi())
        return TWI_TIMEOUT_UNKNOWN;
      else
        return TWI_TIMEOUT_STOPPED;
    }
  }
  
  status = I2STAT;
   
  //Expect 0x08 or 0x10 response for start and repeated start 
  if ( !((status == 0x08 && type == TWI_START) || (status == 0x10 && type == TWI_REPEATSTART)))
  {
      
      if(stopTwi())
        return TWI_ERROR_UNKNOWN;
      else
        return TWI_ERROR_STOPPED;
  }
  
  //this is just to determine what MAX_TWI_TIME should be set to
  if( timeout > maxTestTime ) 
    maxTestTime = timeout;
  
  return TWI_NO_ERROR;
  
}


UINT8 stopTwi()
{
  UINT16 timeout=0;
  UINT8 status = 0;

  I2CONCLR = BIT_STAC | BIT_SIC | BIT_AAC; //clear  all  
  I2CONSET = BIT_STO; //set stop bit
  //I2CONCLR = BIT_SIC; 
  

  while( I2CONSET & BIT_STO  ) //wait for STOP to be sent
  {
    if( timeout++ > MAX_TWI_TIME )
    {
      I2CONCLR = BIT_I2EN; //JML test
      return TWI_TIMEOUT_UNKNOWN;
    }
    
  }
  
  status = I2STAT;
    
  //this is just to determine what MAX_TWI_TIME should be set to
  if( timeout > maxTestTime ) 
    maxTestTime = timeout;
  
  I2CONCLR = BIT_I2EN; //JML test
  return TWI_NO_ERROR;
}

UINT8 sendTwiAddr( UINT8 addr, UINT8 type )
{
  
  UINT16 timeout = 0;
  UINT8 status = 0;
     
  if(type == TWI_ADDR_READ) 
    I2DAT = (addr << 1) | 1; 
  else
    I2DAT = addr << 1;

  I2CONCLR = BIT_SIC | BIT_STAC | BIT_SIC;  //clear bits to TX data
  
  while( TWI_BUSY() )
  {
    if( timeout++ > MAX_TWI_TIME )
    {
    if(stopTwi())
      return TWI_TIMEOUT_UNKNOWN;
    else
      return TWI_TIMEOUT_STOPPED ;
    }
  }
  
  status = I2STAT;   
  
 //Status will be 0x18 for SLA+W, 0x40 for SLA+R
  if ( !((status == 0x18 && type == TWI_ADDR_WRITE) || (status == 0x40 && type == TWI_ADDR_READ)))
  {
    if(stopTwi())
      return TWI_ERROR_UNKNOWN;
    else
      return TWI_ERROR_STOPPED;
  }
       
  //this is just to determine what MAX_TWI_TIME should be set to
  if( timeout > maxTestTime ) 
    maxTestTime = timeout;
  
  return TWI_NO_ERROR;       
}

UINT8 sendTwiData( UINT8 *data, UINT8 n )
{
  
  UINT16 timeout = 0;
  UINT8 status = 0;
  
  while( n-- > 0 )
  {
    I2DAT = *data++ ;  //load data
    I2CONCLR = BIT_SIC | BIT_STAC | BIT_SIC;  //clear bits to TX data
    
    while( TWI_BUSY() )
    {
      if( timeout++ > MAX_TWI_TIME )
      {
      if(stopTwi())
        return TWI_TIMEOUT_UNKNOWN;
      else
        return TWI_TIMEOUT_STOPPED ;
      }
    }
    
    status = I2STAT;   
    
   //Status 0x28 for write data
    if ( !status == 0x28 )
    {
      if(stopTwi())
        return TWI_ERROR_UNKNOWN;
      else
        return TWI_ERROR_STOPPED;
    }
         
    //this is just to determine what MAX_TWI_TIME should be set to
    if( timeout > maxTestTime ) 
      maxTestTime = timeout;
  }
  return TWI_NO_ERROR;       
}


UINT8 readTwiData( UINT8 *data, UINT8 n)
{
  
  UINT16 timeout = 0;
  UINT8 status = 0;
  
  while( n-- > 0 )
  {
    /* return ACK with all but last data */
    
    if( n > 0 )
      I2CONSET = BIT_AA ;  //Send ACK
    else
      I2CONCLR = BIT_AAC ; //last byte, send NACK
    
    
    I2CONCLR = BIT_SIC ; //clear SI to start RX
    
    while( TWI_BUSY() )
    {
      if( timeout++ > MAX_TWI_TIME )
      {
        if(stopTwi())
          return TWI_TIMEOUT_UNKNOWN;
        else 
          return TWI_TIMEOUT_STOPPED ;
      }
    }
    
    *data++ = I2DAT ; 
    status = I2STAT;
    
    //Status will be 0x50 for read or 0x58 for last byte
    if (!(( status == 0x50 && n > 0) || (status == 0x58 && n == 0)))
    {
      if(stopTwi())
        return TWI_ERROR_UNKNOWN;
      else
        return TWI_ERROR_STOPPED;
    }
    
    //this is just to determine what MAX_TWI_TIME should be set to
    if( timeout > maxTestTime ) 
      maxTestTime = timeout;
  }
  return TWI_NO_ERROR;
}



//============================
//    LOCAL CODE
//============================


//============================
//    INTERRUPT SERVICE ROUTINES
//============================


//============================
//    HARDWARE SPECIFIC CODE
//============================


