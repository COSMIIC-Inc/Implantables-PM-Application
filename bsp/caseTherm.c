//    caseTherm: .c    .


/* theory of operation
	The ADS1110 adc w/pga sits on the I2C bus, measuring a 6K thermistor in a 
	voltage divider circuit.  Vin = 1.49v @ 10degC, 0.682v @ 50degC.

	0.1 degC >= 1.7mV so use >12bit resolution.  

	System uses 100msec Accelr rate, so settle for >20msec, 
	use DR=2 for <= 46ms/sample and 14bit resol.  
	Leave ckt on for 200ms/1000ms while sampling.

	(Therefore system accelr rate could be as fast as just 50-60 msec)
	
	Using i2c in a polled mode, where these routines run to completion.

*/



#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "sys.h"
#include "caseTherm.h"
#include "ObjDict.h"
#include "i2c.h"


// -------- DEFINITIONS ----------

#define ADS_ADDR                0x48
//#define ADS_WR			0x90
//#define ADS_RD			(ADS_WR | 1)
//#define ADS_CONVERT		0x98				//PGA=1, 30SPS, SGL START
#define ADS_CONVERT		0x90				//PGA=1, 240SPS, SGL START





// --------   DATA   ------------

typedef struct 
{
	UINT16 degC;
	UINT16 adsCounts;
	
} THERMTBL;


static THERMTBL const thermTbl[] =
{
	//degC(100=10.0), adsCounts   based on 16383 FS
	
//	100,	11898,
//	110,	11741,
//	120,	11581,
//	130,	11419,
//	140,	11255,
//	150,	11089,
//	160,	10921,
//	170,	10752,
//	180,	10582,
//	190,	10410,
//	200,	10237,
//	210,	10063,
//	220,	9888,
//	230,	9713,
//	240,	9538,
//	250,	9362,
//	260,	9186,
//	270,	9010,
//	280,	8835,
//	290,	8659,
//	300,	8485,
//	310,	8311,
//	320,	8138,
//	330,	7966,
//	340,	7796,
//	350,	7626,
//	360,	7457,
//	370,	7292,
//	380,	7128,
//	390,	6965,
//	400,	6804,
//	410,	6645,
//	420,	6488,
//	430,	6334,
//	440,	6181,
//	450,	6031,
//	460,	5882,
//	470,	5736,
//	480,	5595,
//	490,	5454,
//	500,	5316
  
      //degC(100=10.0), adsCounts   based on 2048 FS
        100,	1487,
        110,	1468,
        120,	1448,
        130,	1427,
        140,	1407,
        150,	1386,
        160,	1365,
        170,	1344,
        180,	1323,
        190,	1301,
        200,	1280,
        210,	1258,
        220,	1236,
        230,	1214,
        240,	1192,
        250,	1170,
        260,	1148,
        270,	1126,
        280,	1104,
        290,	1082,
        300,	1061,
        310,	1039,
        320,	1017,
        330,	996,
        340,	975,
        350,	953,
        360,	932,
        370,	912,
        380,	891,
        390,	871,
        400,	851,
        410,	831,
        420,	811,
        430,	792,
        440,	773,
        450,	754,
        460,	735,
        470,	717,
        480,	699,
        490,	682,
        500,	665

};



// -------- PROTOTYPES ----------

static UINT8 putAdsReg( UINT8 data );
static UINT8 getAdsReg( UINT8 *data, UINT8 n );
static UINT16 calcCaseTempr( UINT16 counts );


 
//============================
//    GLOBAL CODE
//============================

void initCaseThermometer( void )
{
	
	// done elsewhere: initTwiHdwr();

}


void updateCaseThermometer( UINT8 state )
{
	// state=0 turn on voltage divider; =1 start conversion; 
	// =2 read ads; =else skip
	
	UINT8 adsData[3];
	BW16 d;
	UINT16 t;
        UINT8 err = 0;
        UINT8 retry = TWI_MAX_TRIES;
	
	
	if( state == 0 )
		TURN_ON_THERMISTOR();
	
	else if( state == 1 )
        {
	  while (retry--)
          {	
            err = putAdsReg( ADS_CONVERT );
            if (err == 0)
              break;
          }
        }
	
	
	else if( state == 2 )
	{
          while (retry--)
          {	
            err = getAdsReg( adsData, 3 );
            if (err == 0)
              break;
          }
		
	
		TURN_OFF_THERMISTOR();
		
		if(err == 0)
                {
                  d.b[0] = adsData[1];
                  d.b[1] = adsData[0];
                  
                  t = calcCaseTempr( d.s );
                  
                  Status_TemperatureCount = d.s;
                  Temperature = t;
                }
                else
                {
                  Temperature = 0xFFFF; //set error values
                }
                
                  
		
	}
	
}





//============================
//    LOCAL CODE
//============================

static UINT8 putAdsReg( UINT8 data )
{

	UINT8 err = 0;
        
	err = startTwi( TWI_START );
	if(err) return(err);
	
	err = sendTwiAddr( ADS_ADDR, TWI_ADDR_WRITE );
        if(err) return(err);
	
        err = sendTwiData( &data, 1 );
        if(err) return(err);
        
        err = stopTwi();
	
	return err;
	
}


static UINT8 getAdsReg( UINT8 *data, UINT8 n )
{
        UINT8 err = 0;
	
	
	err = startTwi( TWI_START );
	if(err) return(err);
        
	err = sendTwiAddr( ADS_ADDR, TWI_ADDR_READ );
        if(err) return(err);
	
	err = readTwiData( data, n);
	if(err) return(err);
	
	err = stopTwi();

	return err;
}


static UINT16 calcCaseTempr( UINT16 counts )
{
	// interpolate temperature from table of equiv ads counts
	
	THERMTBL const *p1, *p2;
	UINT16 tempr;
	
	
	for( p1 = thermTbl ; counts < p1->adsCounts  ; p1++ )
	{
		if( p1->degC >= 500 )
			break;
	}
	
	
	if( p1->degC <= 100 )
		tempr = 100;
	
	else if( p1->degC >= 500 )
		tempr = 500;
	
	else
	{
		p2 = p1 - 1;
		
		tempr = p1->degC - (10 * (counts - p1->adsCounts) / (p2->adsCounts - p1->adsCounts) );
		
	}
	
	return tempr;
	
}



//============================
//    INTERRUPT SERVICE ROUTINES
//============================


//============================
//    HARDWARE SPECIFIC CODE
//============================


