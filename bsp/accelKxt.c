//    file: accelKxt.c    .


/* theory of operation
	The kxt accelerometer sits on the I2C bus.  Right now it is configured for
	default operation, reading x,y,z axisss.

	Using i2c in a polled mode, where these routines run to completion.
*/



#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "sys.h"
#include "accel.h"
#include "ObjDict.h"
#include "i2c.h"



// -------- DEFINITIONS ----------

#define ACCEL_ADDR			0x0F
//#define ACCEL_WR			(0x0F << 1)
//#define ACCEL_RD			(ACCEL_WR | 1)
#define XOUT				0x12
#define INT_SRC_REG1                    0x16
#define INT_REL			        0x1A
#define CTRL_REG1			0x1B
#define CTRL_REG2			0x1C
#define CTRL_REG3			0x1D
#define INT_CTRL_REG1			0x1E
#define INT_CTRL_REG2			0x1F
#define WUF_TIMER                       0x29
#define B2S_TIMER                       0x2A
#define WUF_THRESH                      0x5A
#define B2S_THRESH                      0x5B
#define WHO_AM_I                        0x0F


#define IMU_ADDR			0x6A //or 6B
#define IMU_WHO_AM_I                    0x0F
#define IMU_OUTX_L_G        	        0x22
#define IMU_OUTX_L_A                    0x28
#define IMU_CTRL6_C                     0x15
#define IMU_CTRL1_XL                    0x10
#define IMU_CTRL2_G			0x11
#define IMU_CTRL3_C			0x12
#define IMU_CTRL4_C			0x13
#define IMU_CTRL5_C			0x14
#define IMU_CTRL6_C			0x15
#define IMU_CTRL7_G			0x16
#define IMU_CTRL9_C			0x18
#define IMU_CTRL10_C		        0x19
#define STATUS_REG		0x1E
#define	MD1_CFG			0x5E

#define SELECT_IO_PIN_AS_EINT2()		        (PINSEL0_bit.P0_15 = 2)
#define SELECT_EINT2_PIN_AS_IO()		        (PINSEL0_bit.P0_15 = 0)
#define CLR_ACCEL_IRQ_FLAG()				(EXTINT_bit.EINT2 = 1)
#define IS_ACCEL_IRQ()				        (EXTINT_bit.EINT2 == 1)
          
#define ENABLE_WAKE_ON_EINT2()				(EXTWAKE_bit.EXTWAKE2 = 1)

#define ENABLE_ACCEL_IRQ()				(VICIntEnable  = (1 << VIC_EINT2))
#define DISABLE_ACCEL_IRQ()			        (VICIntEnClear = (1 << VIC_EINT2))


// --------   DATA   ------------


// -------- PROTOTYPES ----------

static UINT8 getAccelReg( UINT8 regNo, UINT8 *data, UINT8 n );
static UINT8 putAccelReg( UINT8 regNo, UINT8 *data, UINT8 n );
static UINT8 putAccelRegSingle( UINT8 regNo, UINT8 data);
//static void configAccelInterrupt( void );
void Accel_ISR(void);

static UINT8 accel_addr; //Global used to switch between ISM330IS IMU and KXTE9-2050 accelerometer

static UINT8 gyroenabled = FALSE;

//============================
//    GLOBAL CODE
//============================
void enableGyro( void)
{
  if(accel_addr == IMU_ADDR)
  {
    putAccelRegSingle( IMU_CTRL7_G, 0x80 ); //low power mode Gyro
    putAccelRegSingle( IMU_CTRL2_G, 0x20); //26Hz, +/-250dps, Gyro
    
    gyroenabled = TRUE;
  }
}

void disableGyro( void)
{
  if(accel_addr == IMU_ADDR)
  {
    putAccelRegSingle( IMU_CTRL2_G, 0x00); //Power down Gyro
    
    gyroenabled = FALSE;
  }
}


void initAccelerometer( void )
{
    
    UINT8 err;
    UINT8 result = 0xFF;

    //Check if ISM330IS IMU is available
    accel_addr = IMU_ADDR;
    err = getAccelReg( IMU_WHO_AM_I, &result, 1);
    if(err==0 && result==0x22){       
      putAccelRegSingle( IMU_CTRL6_C, 0x10 ); //low power mode Acc
      putAccelRegSingle( IMU_CTRL1_XL, 0x20); //26Hz, 2g, Acc
    }  

    else
    {
      //Check if KXTE9-2050 accelerometer is available
      accel_addr = ACCEL_ADDR;
      err = getAccelReg( WHO_AM_I, &result, 1);
      if(err==0 && result==0x00){     
        sleepAccelerometer();
      }
      else
      {
        accel_addr = 0;
      }
    }

}


void updateAccelerometer( void )
{
	UINT8 accelData[6], gyroData[6];
	CPU_SR    cpu_sr;
	UINT8 err = 1;
	UINT8 retry = TWI_MAX_TRIES;
        
	/* read accelerometer x,y,z */
	while(retry--)
        {
            if(accel_addr == IMU_ADDR)
            {
              err = getAccelReg( IMU_OUTX_L_A, accelData, 6 );
              if(gyroenabled)
              {
                err = getAccelReg( IMU_OUTX_L_G, gyroData, 6 );
              }
            }
            else if(accel_addr == ACCEL_ADDR)
            {
              err = getAccelReg( XOUT, accelData, 3 );
            }
            else 
              return;
            
            if(err==0)
               break;
            
        }
	
	CPU_CRITICAL_ENTER();
	
	if(err== 0)  
        {
          if(accel_addr == IMU_ADDR)
          {
            //JML TODO: use all 16-bits - currently just high bytes and for backwards compatibility
            if(accelData[1]>=0x80)
              AccelerometersPM[0] = accelData[1]-0x80;  
            else
              AccelerometersPM[0] = 0x80+accelData[1]; 
            if(accelData[3]>=0x80)
              AccelerometersPM[1] = accelData[3]-0x80;  
            else
              AccelerometersPM[1] = 0x80+accelData[3]; 
            if(accelData[5]>=0x80)
              AccelerometersPM[2] = accelData[5]-0x80;
            else
              AccelerometersPM[2] = 0x80+accelData[5]; 
             
            if(gyroenabled)
            {
              memmove( &GyrosPM[0], gyroData, 6 );
            }
            
          }
          else if (accel_addr == ACCEL_ADDR)
          {
            memmove( &AccelerometersPM[0], accelData, 3 );
          }
          AccelerometersPM[3]++;
        }
	else
        {
          memset( &AccelerometersPM[0], 0xFF, 4 ); //set error values
        }
          
	CPU_CRITICAL_EXIT();
	
}

void sleepAccelerometer( void )
{
    UINT8 d;

    //d = BIT3 | BIT2 | BIT1 | BIT0; //set OB2S and OWUF to 40Hz
    d = BIT3 | BIT1; //set OB2S and OWUF to 10Hz
    putAccelReg( CTRL_REG3, &d, 1 );
    
    d = BIT4 | BIT3; //enable interrupt pin, active high, latching
     putAccelReg( INT_CTRL_REG1, &d, 1 );
    
    d = BIT7 | BIT6 | BIT5; //enable interrupt on all axes
    putAccelReg( INT_CTRL_REG2, &d, 1 );
    
    //d = 0x01; //1 consecutive samples (1/OWUF) must be above WUF_THRESH to wake up (=0.0125s)
    d=0x05; //1 consecutive samples (1/OWUF) must be above WUF_THRESH to wake up (=0.5s)
    putAccelReg( WUF_TIMER, &d, 1 );
    
    //d = 0x0A; //10 consecutive samples (16/OB2S) must be below B2S_THRESH to go back to sleep (=0.8s)
    d = 0x05; //5 consecutive samples (16/OB2S) must be below B2S_THRESH to go back to sleep (8s)
    putAccelReg( B2S_TIMER, &d, 1 );

    d = 0x20; //WUF_THRESH = 0.5g*4*16counts/g
    //d = 0x05;
    putAccelReg( WUF_THRESH, &d, 1 );
    
    d = 0x60; //B2S_THRESH = 1.5g*4*16counts/g
    //d = 0x80;
    putAccelReg( B2S_THRESH, &d, 1 );
    
    //d = BIT7 | BIT4 | BIT3 | BIT2 | BIT1;  //accel in 40Hz operation, enable B2S and WUF int
    d = BIT7 | BIT4 | BIT2 | BIT1;  //accel in 10Hz operation, enable B2S and WUF int
    //d = BIT7 | BIT0;  //accel in operation 1Hz, enable orientaion detection
    putAccelReg( CTRL_REG1, &d, 1 );
    
    getAccelReg( INT_REL, &d, 1 ); //clear latched interrupt in accelerometer
    
    //configAccelInterrupt();
    //ENABLE_ACCEL_IRQ();
    
    
}



#if 0
static void configAccelInterrupt( void )
{
	// Usage: Accelerometer is configured to have a latched rising edge interrupt when it detects activity
	
	 UINT32 apbdiv;
	
	 
	SELECT_IO_PIN_AS_EINT2();
        ENABLE_WAKE_ON_EINT2();
	
	
	/* Config IntPin using BUG WORKAROUND SEE EP_21---.PDF ERRATA SHEET */
	
	apbdiv = APBDIV;
	
	APBDIV = 0;				// bug fix
	EXTMODE_bit.EXTMODE2 = 1;			// edge sensitivity for EINT2
	APBDIV = 1;				// bug fix
	
        APBDIV = 0;				// bug fix
	EXTPOLAR_bit.EXTPOLAR2 = 1;			// rising edge sensitive EINT2
	APBDIV = 1;				// bug fix
        
	APBDIV = apbdiv;		// bug fix
	
	CLR_ACCEL_IRQ_FLAG();		//clear appropriate EXTINT bit after setting mode or polatrity
        /************ Setup ISR for receive interrupt using EINT2 on PO.15 ********************/
        /**************************************************************************************/
        
        VICIntSelect &= ~(1 << VIC_EINT2);                         /* select as not FIQ   */
        VICVectAddr15  = (CPU_INT32U)Accel_ISR;              /* Set the vector address        */
        VICVectCntl15  = 0x20 | VIC_EINT2;                          /* Enable vectored interrupts    */
	//interrupt will be enabled separately
}
#endif

//============================
//    LOCAL CODE
//============================

static UINT8 putAccelReg( UINT8 regNo, UINT8 *data, UINT8 n )
{
        UINT8 err = 0;
        
	err = startTwi(TWI_START);
        if(err) return(err);
	
	err = sendTwiAddr( accel_addr, TWI_ADDR_WRITE );
        if(err) return(err);
	
        err = sendTwiData( &regNo, 1 );
        if(err) return(err);
        
	err = sendTwiData( data, n );
        if(err) return(err);
	
	err = stopTwi();
        if(err) return(err);
        
	return 0;
	
}

static UINT8 putAccelRegSingle( UINT8 regNo, UINT8 data )
{
  return putAccelReg( regNo, &data, 1 );
}


static UINT8 getAccelReg( UINT8 regNo, UINT8 *data, UINT8 n )
{
        UINT8 err = 0;
        
        err = startTwi( TWI_START );
        if(err) return(err);
	
	err = sendTwiAddr( accel_addr, TWI_ADDR_WRITE );
        if(err) return(err);
        
        err = sendTwiData( &regNo, 1);
        if(err) return(err);
	
	err = startTwi(TWI_REPEATSTART);
        if(err) return(err);
	
	err = sendTwiAddr( accel_addr, TWI_ADDR_READ );
        if(err) return(err);
	
	err = readTwiData( data, n);
        if(err) return(err);
		
	err = stopTwi();
	
	return err;

}






//============================
//    INTERRUPT SERVICE ROUTINES
//============================
void Accel_ISR(void)
{
    UINT8 d;
    UINT8 s;
    
    DISABLE_ACCEL_IRQ();
    CLR_ACCEL_IRQ_FLAG();
    //SELECT_EINT2_PIN_AS_IO(); 
    
    getAccelReg( INT_SRC_REG1, &s, 1 ); //get interrupt source
    
//    if(s & BIT2) //B2S
//    {
//      //IO0SET = BIT0;
//    }
//    if(s & BIT1) //WUF
//    {
//      //IO0SET = BIT1;
//    }
    
    getAccelReg( INT_REL, &d, 1 ); //clear latched interrupt in accelerometer
    ENABLE_ACCEL_IRQ();
   

    //initAccelerometer(); //reconfigure acclerometer for regular sampling
    
    
    IO0CLR = BIT0 | BIT1;
    
    VICVectAddr = 0x0; // Acknowledge that ISR has finished execution
}

//============================
//    HARDWARE SPECIFIC CODE
//============================


