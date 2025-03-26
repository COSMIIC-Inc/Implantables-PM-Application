//    pwrnet: .c    .


#include "pwrnet.h"





// -------- DEFINITIONS ----------


#define DIGPOT_ADDR                     0x2E
//#define DIGPOT_WR					(0x5c)
#define DIGPOT_MIN_VOLTAGE_SETTING      127     //7 bit digital pot
#define VNET_DEFAULT                     82


// --------   DATA   ------------



// -------- PROTOTYPES ----------

static UINT8 putDigPot( UINT8 potSet );


 
//============================
//    GLOBAL CODE
//============================

void InitPowerNetwork( void )
{
  UINT8 err;
  UINT8 retry = TWI_MAX_TRIES;
  
  DISABLE_PWR_NETWORK();
  
  if (FALSE) // Control_SystemControl & 0x01 == 0x01
  {
    
    NetworkPowerControl |= 0x01; // turn on Bit 0
    
    if( updatePowerNetwork() ) // wait until network comes on
    {
      WakeRemoteModule( 0 );  // wakes the remote nodes from BL 
    }
    
  }
  else
  {   
    while (retry--)
    {
        err = putDigPot( DIGPOT_MIN_VOLTAGE_SETTING); //set network voltage to minimum 
        if(err==0)
          break;
    }
  }
}
    

/**
 * @brief Enables or disables the network (power and CAN) and sets Network Voltage via digital pot
 * @details resistor divider values are incorporated into the digital pot formula.   
 *      If resistor values change, the formula should be updated.  This function should be called
 *      every 100ms - 1sec to update Network Voltage based on SDOs.  It can also be called directly
 *      to enable/disable the network given an NMT command.  The Network Voltage is always 
 *      set to a minimum value when the network is off to save power and to ensure network voltage
 *      is below ~6.3V as network is enabled.  
 *      
 * @param returns 1 if network is ON, 0 if Off  
 */

UINT8 updatePowerNetwork( void )
{
	UINT8  potSet;
        static UINT8 vNet;
        UINT8  runCanEnable = 0;
        UINT8 err;
        UINT8 retry = TWI_MAX_TRIES;
        OS_ERR errOS;
       

	if( IS_PWR_NETWORK_ENABLED()  ) //Network is on
        {
            if( !IS_PWR_NET_CTRL_ON() ) //received command to turn OFF network
            {
                canDisable();           //disable CAN
                DISABLE_PWR_NETWORK(); //turns off network and reduces network voltage
                
                while (retry--)
                {
                    err = putDigPot( DIGPOT_MIN_VOLTAGE_SETTING); //set network voltage to minimum 
                    if(err==0)
                      break;
                }
                return 0;
            }
        }
        else                            //Network is off 
	{
          if( IS_PWR_NET_CTRL_ON() )    //received command to turn ON network
          {
                //Network should already be at minimum voltage (~4.7), becuase it is initialized
                //on startup and every time network is turned off.
                DISABLE_PWR_NETWORK(); //turns off network and reduces network voltage
                while (retry--)
                {
                    err = putDigPot( DIGPOT_MIN_VOLTAGE_SETTING); //set network voltage to minimum 
                    if(err==0)
                      break;
                }     
                ENABLE_PWR_NETWORK(); 
                runCanEnable = 1;
          }
          else
          {
            return 0;                   //Network should stay off, and does not need to set dig pot
          }
	}
	
        //Set the network Voltage if value in OD has changed or if network was just powered up
        if( vNet != NetworkVoltage || runCanEnable)
        {
          if (NetworkVoltage < 47 || NetworkVoltage > 96) // restore failed (or voltage out of range), set to default
          {
            NetworkVoltage = VNET_DEFAULT;  
          }
          vNet = NetworkVoltage;
             
          // 129-> ~4.7V, 0-> ~9.6V, for R1 (-27-R4) = 470kOhm, R2 (-27-R5) = 69.8kOhm	
          potSet = 755008 /(100*(UNS32)vNet-1255) - 90;
          
          if( potSet > DIGPOT_MIN_VOLTAGE_SETTING )	//as pot value goes up, voltage vale goes down
             potSet = DIGPOT_MIN_VOLTAGE_SETTING;
          
          retry = TWI_MAX_TRIES;
          while (retry--)
          {
              err = putDigPot( potSet);
              if(err==0)
                break;
          }
        }
        
        if( runCanEnable )
        {
          //delay 100ms to make sure all nodes on Network have stable power rails
          OSTimeDlyHMSM(0, 0, 0, 100, OS_OPT_TIME_HMSM_STRICT, &errOS); 
          canEnable();
        }
        
        return 1;
}

//============================
//    LOCAL CODE
//============================

static UINT8 putDigPot( UINT8 potSet )
{
	UINT8 err = 0;
	
	err = startTwi(TWI_START);
	if(err) return(err);
	
	err = sendTwiAddr( DIGPOT_ADDR, TWI_ADDR_WRITE );
        if(err) return(err);
        
        err = sendTwiData( &potSet, 1);
        if(err) return(err);
	
	err = stopTwi();
	
        return err;
	
}


//============================
//    INTERRUPT SERVICE ROUTINES
//============================


//============================
//    HARDWARE SPECIFIC CODE
//============================


