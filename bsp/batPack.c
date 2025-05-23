//    file: .c    .

/* theory of operation
	The 27200 chip sits on the I2C bus; with clk routed via cpld for addressing
	multiple chips.  Right now the chip is configured for default operation.

	Usage is a polled mode, where these routines run to completion.
*/


#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "sys.h"
#include "batPack.h"
#include "i2c.h"
#include "ObjDict.h"



// -------- DEFINITIONS ----------

#define BATTPK_ADDR                      0x55
#define VSYSCTRL_ADDR                    0x17 //AD5247BKSZ100-1RLZ
//#define BATTPK_WR			(0x55 << 1)
//#define BATTPK_RD			(BATTPK_WR | 1)
//window average for analog data
#define AVG_SAMPLES                     10

typedef struct
{
	UINT16 *tempr, *count, *current, *volts, *nac;
	UINT8  *rsoc, *csoc, *status;
        UINT16 *lmd;
	
} PACK_OD_VAR;


typedef struct
{
	UINT16 *interval, *noCharge, *noStep;
	UINT16 *nfVoltage, *systemVoltage, *networkVoltage, *moduleLoad;
	UINT8  *control, *step, *isEnabled, *request, *command;
	
} CHARGER_OD_VAR;



// --------   DATA   ------------

static UINT16 chargerTimer;

/* pointers to OD variables */

static CHARGER_OD_VAR const battChargerOdVar =
{
	&BatteryControl_BatteryChargingInterval, 
	&BatteryControl_ChargeNFLinkNoChargeLimit,
	&BatteryControl_ChargeNFLinkNoChargeStepLimit, 
	&BatteryControl_VREC,
	&BatteryControl_VSYS,
	&BatteryControl_VNET,
        &BatteryControl_LOAD,
	&BatteryControl_PowerControl, 
	&BatteryControl_BatteryChargingStepIncrement, 
	&BatteryControl_BatteryChargeRun, 
        &BatteryControl_RequestedChargingCurrent,
        &BatteryControl_CommandedChargingCurrent
};


static PACK_OD_VAR const battPackOdVar[3] =
{
	{	(UINT16 *)&Battery1Charge_Status[0], 
		(UINT16 *)&Battery1Charge_Status[2],
		(UINT16 *)&Battery1Charge_Status[4],
		(UINT16 *)&Battery1Charge_Status[6],
		(UINT16 *)&Battery1Charge_Status[8],
		&Battery1Charge_Status[10],
		&Battery1Charge_Status[11],
		&Battery1Charge_Status[12],
                (UINT16 *)&Battery1Charge_Status[14]
	},
	{	(UINT16 *)&Battery2Charge_Status[0], 
		(UINT16 *)&Battery2Charge_Status[2],
		(UINT16 *)&Battery2Charge_Status[4],
		(UINT16 *)&Battery2Charge_Status[6],
		(UINT16 *)&Battery2Charge_Status[8],
		&Battery2Charge_Status[10],
		&Battery2Charge_Status[11],
		&Battery2Charge_Status[12],
                (UINT16 *)&Battery2Charge_Status[14]
	},
	{	(UINT16 *)&Battery3Charge_Status[0], 
		(UINT16 *)&Battery3Charge_Status[2],
		(UINT16 *)&Battery3Charge_Status[4],
		(UINT16 *)&Battery3Charge_Status[6],
		(UINT16 *)&Battery3Charge_Status[8],
		&Battery3Charge_Status[10],
		&Battery3Charge_Status[11],
		&Battery3Charge_Status[12],
                (UINT16 *)&Battery3Charge_Status[14]
	}

};



// -------- PROTOTYPES ----------

static UINT8 getBattGaugeReg( UINT8 regNo, UINT8 *data, UINT8 n );
//static UINT8 putBattGaugeReg( UINT8 regNo, UINT8 *data, UINT8 n );
static void selectBattGauge( UINT8 battNum );

//static void runBattCharger( void );
static void connBattToLoad( UINT8 battNum, UINT8 onOff );
static void connBattToCharger( UINT8 battNum, UINT8 onOff );
static void setChargerPwm( UINT16 t2 );
static UINT16 getModuleLoad( void );
static UINT16 getNearFieldVoltage( void );
static UINT16 getSystemVoltage( void );
static UINT16 getNetworkVoltage( void );

 
//============================
//    GLOBAL CODE
//============================

CPU_BOOLEAN LowSystemVoltage( void )
{
  return(getSystemVoltage()/100 < BatteryControl_MinVSYS);
}

void initBatteryPack( void )
{
	selectBattGauge( 3 ); //disable I2C communication to gauges
	
	/* enable /shdnxx pins */
	IO0DIR |= BIT24 | BIT13 | BIT11 | BIT23 | BIT12 | BIT10 ;

//        //JML Temporary >>
//        IO0CLR = BIT8;
//        connBattToCharger( 2, BIT4 ); 
//        //<<
        
        /* assign P0.8 as PWM4 */
	PINSEL0 |= BIT17;       
        PINSEL0 &=~BIT16;       
        	
        /* CONFIG charger pwm timer */  
	PWMTCR = BIT1;					// reset the counter/prescaler
	PWMPR = 24; 					// 10MHz / 25 = 100KHz timer
	PWMMCR = BIT1;					// reset on match0 reg
	PWMMR0 = 100;					// 1KHz rate
									
	PWMPCR = BIT12; // enable PWM4 output (single edge controlled mode) 
	
	PWMTCR = BIT3 | BIT0;			// start the counter/prescaler, pwm mode
	
        DisableCharging();
        
        setBattVSYS(0); //Set AD5247 to minimal resistance, max VSYS setting to be compatible with previous PMs
        

}

/**
 * @brief Maintain the 3-cell battery pack
 * @detail this function assumes a call rate of 100msec
 * @param battNum = 0,1,2 SELECTS cell # 1,2,3; else skip.
 *	the skip mechanism lets us update all packs at a reduced interval.
 */

void UpdateFuelGauges( UINT8 battNum )
{
  UINT8 retry = TWI_MAX_TRIES;
  UINT8 err = 0;
	if( battNum > 2 )
	{
		return;
	}	
        
        struct
	{
		UINT16 tempr;		// x .25 degK (C = K - 273.15)
		UINT16 volt;		// x 1mV
		UINT8  status;
		UINT8  rsoc;
		UINT16 nac;
                UINT16 cacd;
                UINT16 cact;
                UINT16 lmd;
                UINT16 avgCurrent;
		
	} batt1;
	
	struct
	{
		UINT16 cycleCnt;
		UINT8  csoc;
		
	} batt2;
        

	//UINT8 eeprom[10]; //JML DEBUG ONLY
	CPU_SR    cpu_sr;
	PACK_OD_VAR const *odVar;
		
		
	/* read battery pack status */
        selectBattGauge( battNum ); //enable I2C communication with gauge 
        while (retry--)
        {
          err = getBattGaugeReg( 0x06, (UINT8 *)&batt1, 16 );
          if(err == 0)
          {
              err = getBattGaugeReg( 0x2a, (UINT8 *)&batt2, 3 );
              if (err==0)
                break; //don't retry
          }
        }
        
        if(err != 0) 
        {
          batt1.tempr = 0xFFFF; //set error values
          batt1.avgCurrent = 0xFFFF;
          batt1.volt = 0xFFFF;
          batt1.nac = 0xFFFF;
          batt1.lmd = 0xFFFF;
          batt1.rsoc = 0xFF;
          batt1.status = 0xFF;
          batt2.cycleCnt =0xFFFF; //set error values
          batt2.csoc = 0xFF;
        }
          
        //getBattGaugeReg( 0x76, (UINT8 *)&eeprom, 10 ); //JML DEBUG ONLY
	selectBattGauge( 3 ); //disable I2C communication with gauge
	
        /* update ObjDictionary */
        
        odVar = &battPackOdVar[ battNum ];
        
        CPU_CRITICAL_ENTER();
        
        *odVar->tempr   = batt1.tempr;
        *odVar->count   = batt2.cycleCnt;
        *odVar->current = batt1.avgCurrent;
        *odVar->volts	  = batt1.volt;
        *odVar->nac     = batt1.nac;
        *odVar->rsoc    = batt1.rsoc;
        *odVar->csoc    = batt2.csoc;
        *odVar->status  = batt1.status;
        *odVar->lmd     = batt1.lmd;
        
        CPU_CRITICAL_EXIT();

}

/**
 * @brief Runs Battery Charger state chart
 * @details Near Field Voltage (VREC), e.g. noCharge =15V, noStep=20V
 *            if VREC < noCharge, charging disabled
 *            if noCharge < VREC < noStep, charging enabled but will not step up
 *            if noStep < VREC, charging enabled and will step up to meet request,
 *            In all cases if new request < current commad, command will be set to request
 * Only one PWM controls the command for all three batteries
 */
void MaintainBatteryCharger( void )
{
// assumes a call rate of 100msec
	UINT8  control, enableTimer=0;
	CHARGER_OD_VAR const *charger;
	
        static UINT8 avgCounter = AVG_SAMPLES-1;
        static UINT8 avgStartCounter = 1;
        UINT32 sum = 0;
        UINT16 value;
        static UINT32 nfVoltageSum_Prev = 0, sysVoltageSum_Prev = 0, netVoltageSum_Prev = 0, moduleLoadSum_Prev = 0;
        static UINT16 nfVoltageBuffer[AVG_SAMPLES] = {0}, sysVoltageBuffer[AVG_SAMPLES] = {0},
                      netVoltageBuffer[AVG_SAMPLES] = {0}, moduleLoadBuffer[AVG_SAMPLES] = {0};
	
	charger = &battChargerOdVar;

        //JML changed this: increased to UINT32:
        //apply AVG_SAMPLES sample windowed average on analog values to reduce noise 
        //Highest Analog value is ~400: AVG_SAMPLES must be < 163 to insure sum fits in UINT16  
        //appropriate: 5 < AVG_SAMPLES < 40
        //higher AVG_SAMPLES results in more RAM consumption
        //first AVG_SAMPLES are averaged across only as many points as have been collected
        
        if(BatteryControl_LowPowerStatus & BIT7) //Only get VREC. VSYS monitored elsewhere
        {
          
          //average together AVG_SAMPLES readings each time it wakes up
          for(avgCounter = 0; avgCounter < AVG_SAMPLES; avgCounter++)
          {
            sum += getNearFieldVoltage();
          }

          *charger->nfVoltage = sum/AVG_SAMPLES;
          
          //reset other variables for next time we come out of Low power mode
          avgCounter = AVG_SAMPLES-1;
          
          if(avgStartCounter > 1)
          {
            avgStartCounter = 1;
            nfVoltageSum_Prev = 0;
            sysVoltageSum_Prev = 0;
            netVoltageSum_Prev = 0; 
            moduleLoadSum_Prev = 0;
            memset(nfVoltageBuffer,  0, sizeof(nfVoltageBuffer));
            memset(sysVoltageBuffer, 0, sizeof(sysVoltageBuffer));
            memset(netVoltageBuffer, 0, sizeof(netVoltageBuffer));
            memset(moduleLoadBuffer, 0, sizeof(moduleLoadBuffer));
          }
        }
        else
        {
          //timer is ~100ms per count when not in low power
          chargerTimer++; 
        
          value = getNearFieldVoltage();
          sum = nfVoltageSum_Prev + value - nfVoltageBuffer[avgCounter];
          nfVoltageSum_Prev = sum;
          nfVoltageBuffer[avgCounter] = value; 
          if (avgStartCounter < AVG_SAMPLES)
            *charger->nfVoltage = sum/avgStartCounter/100;
          else
            *charger->nfVoltage = sum/AVG_SAMPLES/100;

          value = getSystemVoltage();
          sum = sysVoltageSum_Prev + value - sysVoltageBuffer[avgCounter];
          sysVoltageSum_Prev = sum;
          sysVoltageBuffer[avgCounter] = value; 
          if (avgStartCounter < AVG_SAMPLES)
            *charger->systemVoltage = sum/avgStartCounter/100;
          else
            *charger->systemVoltage = sum/AVG_SAMPLES/100;
          

          value = getNetworkVoltage();
          sum = netVoltageSum_Prev + value - netVoltageBuffer[avgCounter];
          netVoltageSum_Prev = sum;
          netVoltageBuffer[avgCounter] = value; 
          if (avgStartCounter < AVG_SAMPLES)
            *charger->networkVoltage = sum/avgStartCounter/100;
          else
            *charger->networkVoltage = sum/AVG_SAMPLES/100;

          
          value =  getModuleLoad();
          sum = moduleLoadSum_Prev + value - moduleLoadBuffer[avgCounter];
          moduleLoadSum_Prev = sum;
          moduleLoadBuffer[avgCounter] = value; 
          *charger->moduleLoad = sum/AVG_SAMPLES;
          
          if (avgStartCounter < AVG_SAMPLES)
          {
            *charger->moduleLoad = sum/avgStartCounter;
            avgStartCounter++;
          }
          else
            *charger->moduleLoad = sum/AVG_SAMPLES;
                    
          if(++avgCounter == AVG_SAMPLES)
            avgCounter = 0;
        }
	
        //*charger->nfVoltage = getNearFieldVoltage();
	//*charger->systemVoltage = getSystemVoltage();
	//*charger->networkVoltage = getNetworkVoltage();
        //*charger->moduleLoad = getModuleLoad();
	
        control = (*charger->control);
	
	if( *charger->nfVoltage <= *charger->noCharge )
	{
            /* take battery off of charge */
            connBattToCharger( 0, 0 );
            connBattToCharger( 1, 0 );
            connBattToCharger( 2, 0 );
            
            *charger->command = 0;
            *charger->isEnabled = 0;
	}
        else
        {
            /* if enabled, connect battery to charger, else disconnect */
            connBattToCharger( 0, control & BIT0 );  
            connBattToCharger( 1, control & BIT2 );  
            connBattToCharger( 2, control & BIT4 );  
            
            if(control & (BIT0|BIT2|BIT4))
              *charger->isEnabled = 1;
        }
        
        //if enabled connect battery to load
        connBattToLoad( 0, control & BIT1 );  
        connBattToLoad( 1, control & BIT3 );  
        connBattToLoad( 2, control & BIT5 );  
	
	if( *charger->isEnabled)
        {
          /* NF Voltage is above stepping threshold and command ~= request*/
          if(	*charger->nfVoltage > *charger->noStep  &&  *charger->command != *charger->request  )
          {
                  /* special case: if step=0, always set command to request*/
                  if( *charger->step == 0 )
                  {
                          *charger->command = *charger->request;
                          
                  }
                  /*If the timer > interval (In Low Power Mode, interval is fixed at 1s), */
                  else if( (BatteryControl_LowPowerStatus & BIT7) || chargerTimer >= *charger->interval )
                  {
                          /* command < request, so step up command to meet request*/
                          if( *charger->command < *charger->request )
                          {
                                  *charger->command += *charger->step ;                                    
                          }                         
                  }
                  else
                          enableTimer = 1;
          }
          /*In any case, if command > request, immediately drop command to match request*/
          if ( *charger->command > *charger->request )
          {
            *charger->command = *charger->request;
          }
        }
	/* if timer isn't enabled, set the timer back to 0*/
	if( !enableTimer )
		chargerTimer = 0;
	
	/*apply changes to the PWM*/
	setChargerPwm( *charger->command );
	
}

void DisableCharging( void )
{
  connBattToCharger( 0, 0 );
  connBattToCharger( 1, 0 );
  connBattToCharger( 2, 0 );
  
  setChargerPwm( 0 );
  BatteryControl_CommandedChargingCurrent = 0;
}


//============================
//    LOCAL 27200 CHIP CODE
//============================

/*
static UINT8 putBattGaugeReg( UINT8 regNo, UINT8 *data, UINT8 n )
{
	UINT8 status = 0;
	UINT8 header[2];
	
	
	startTwi( &status );
	
	header[0] = BATTPK_WR;
	header[1] = regNo;
	
	sendTwiData( header, 2, &status );
	
	sendTwiData( data, n, &status );
	
	stopTwi( &status );
	
	return status;
	
}
*/
UINT8 setBattVSYS( UINT8 val )
{

	UINT8 err = 0;
        if (val>127){ return 1;}
                
	err = startTwi( TWI_START );
        if(err) return(err); 
        
	err = sendTwiAddr( VSYSCTRL_ADDR, TWI_ADDR_WRITE);
        if(err) return(err); 

        err = sendTwiData( &val, 1);
        if(err) return(err);  

	err = stopTwi();
	return err;

}


static UINT8 getBattGaugeReg( UINT8 regNo, UINT8 *data, UINT8 n )
{

	UINT8 err = 0;
        
	err = startTwi( TWI_START );
        if(err) return(err); 
        
	err = sendTwiAddr( BATTPK_ADDR, TWI_ADDR_WRITE);
        if(err) return(err); 

        err = sendTwiData( &regNo, 1);
        if(err) return(err);  
                
	err = startTwi( TWI_REPEATSTART );
        if(err) return(err); 
	
	err = sendTwiAddr( BATTPK_ADDR, TWI_ADDR_READ );
        if(err) return(err); 
        
	err = readTwiData( data, n);
        if(err) return(err); 
          
	err = stopTwi();
	
	return err;

}







#define I2CS0   BIT21   //P1.21
#define I2CS1   BIT22   //P1.22

static void selectBattGauge( UINT8 battNum )
{

        //default (none selected): I2CS0=1, I2CS1=1
        IO1SET = I2CS0 | I2CS1;
        
        if(battNum == 0)        //I2CS0=0, I2CS1=1: BAT1 
        {
          IO1CLR = I2CS0;
        }
        else if(battNum == 1)   //I2CS0=1, I2CS1=0: BAT2 
        {
          IO1CLR = I2CS1;
        }
        else if(battNum == 2)   //I2CS0=0, I2CS1=0: BAT3 
        {
          IO1CLR = I2CS0 | I2CS1;
        }   
        
        asm("NOP"); asm("NOP");  //JML: delay at least 30ns after selecting gauge
}



//============================
//    LOCAL BATTERY CHARGER CODE
//============================





/* hardware bit, pwm, adc routines */

static UINT32 const chrgBit[3] = {BIT24, BIT13, BIT11};
static UINT32 const loadBit[3] = {BIT23, BIT12, BIT10};

static void connBattToLoad( UINT8 battNum, UINT8 onOff )
{
	UINT32 const *ctrlBit;
	
	
	ctrlBit = &loadBit[ battNum ];
	
	if( onOff != 0 )
		IO0SET = *ctrlBit;
	
	else
		IO0CLR = *ctrlBit;
	
}


static void connBattToCharger( UINT8 battNum, UINT8 onOff )
{
	UINT32 const *ctrlBit;
	
	
	ctrlBit = &chrgBit[ battNum ];
	
	if( onOff != 0 )
		IO0SET = *ctrlBit;
	
	else
		IO0CLR = *ctrlBit;
	
}



static void setChargerPwm( UINT16 t2 )
{
	// pwm pin goes high at timer=rollover, off at t2
	
	PWMMR4 = (UINT32)t2;
	
	PWMLER = BIT4;				// enable pwm update
	
}


/* adc config is polled, chan1, enable and software start */
/* JML Note: PCLK (peripheral clock) is set to CLK/4,  
 * ADC_CLK must be < 4.5MHz
 * with PLL at 6 (60MHz), PCLK = 15 MHz,  ADC_CLKDIV >= 4
 * with PLL at 5 (50MHz), PCLK = 12.5 MHz, ADC_CLKDIV >= 3
 * with PLL at 4 (40MHz), PCLK = 10 MHz, ADC_CLKDIV >= 3
 * with PLL at 3 (30MHz), PCLK =  7.5 MHz, ADC_CLKDIV >= 2
 * with PLL at 2 (20MHz), PCLK =  5 MHz, ADC_CLKDIV >= 2
 * with PLL at 1 (10MHz), PCLK =  2.5 MHz, , ADC_CLKDIV >= 1
 Set CLKDIV > (ADC_CLKDIV-1)
 Since ADC isn't used very fast, set CLKDIV=4 (set BIT10 of ADCR)
 */
enum 
{
        ADC_CH_LOAD = BIT0,
        ADC_CH_VREC  = BIT1,
	ADC_CH_VSYS = BIT2,
	ADC_CH_VNET = BIT3
};
                             //JML:requires prescalar for conversion time to be within spec
#define START_ADC_MEAS(ch)		{ADCR = BIT24 | BIT21 | BIT10| (ch);} 
#define IS_ADC_MEAS_DONE()		(ADGDR_bit.DONE == 1)


static UINT16 getNearFieldVoltage( void )
{
	// measured value on AIN1 P0.28 is 1/12 of actual voltage;
	// adc has result as counts<<6 (x64); so at mV resolution, we have:
	// VREC = counts *64 / 1024 / 64 * 3300mV * 12; 
	//     = counts * 64 / 65536 * 39600
	
	UINT32 adc;
	
	
	START_ADC_MEAS( ADC_CH_VREC );
	
	while( !IS_ADC_MEAS_DONE() );
	
	adc = ADGDR & (0x3ffL << 6) ;
	
	return (adc * 39600) >> 16;
	
}


static UINT16 getSystemVoltage( void )
{
	// measured value on AIN2 P0.29 is 0.6 of actual voltage;
	// adc has result as counts<<6 (x64); so at mV resolution, we have:
	// VSYS = counts *64 / 1024 / 64 * 3300mV * 1.67; 
	//     = counts * 64 / 65536 * 5511
	
	UINT32 adc;
	
	
	START_ADC_MEAS( ADC_CH_VSYS );
	
	while( !IS_ADC_MEAS_DONE() );
	
	adc = ADGDR & (0x3ffL << 6) ;
	
	return (adc * 5511) >> 16;
	
}


static UINT16 getNetworkVoltage( void )
{
	// measured value on AIN3 P0.30 is 1/5 of actual voltage;
	// adc has result as counts<<6 (x64); so at mV resolution, we have:
	// VNET = counts *64 / 1024 / 64 * 3300mV * 5; 
	//     = counts * 64 / 65536 * 16500
	
	UINT32 adc;
	
	
	START_ADC_MEAS( ADC_CH_VNET );
	
	while( !IS_ADC_MEAS_DONE() );
	
	adc = ADGDR & (0x3ffL << 6) ;
	
	return (adc * 16500) >> 16;
	
}

static UINT16 getModuleLoad( void )
{
	// measured value on AIN3 P0.27 is 1/201 of actual current;
	// adc has result as counts<<6 (x64); so at mA resolution, we have:
	// LOAD = counts *64 / 1024 / 64 * 3.3V * 201; 
	//     = counts * 64 / 65536 * 663
	
	UINT32 adc;
	
	
	START_ADC_MEAS( ADC_CH_LOAD );
	
	while( !IS_ADC_MEAS_DONE() );
	
	adc = ADGDR & (0x3ffL << 6) ;
	
	return (adc * 663) >> 16;
	
}



//============================
//    INTERRUPT SERVICE ROUTINES
//============================


//============================
//    HARDWARE SPECIFIC CODE
//============================


