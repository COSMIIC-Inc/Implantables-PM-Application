// Doxygen
/*!
** @defgroup scanmanagement Scan Management of CAN link
** @ingroup userapi
*/
/*!
** defgroup canscanner CAN Scanner
** @ingroup scanmanagement
*/
/*!
** @file   runCANserver.h
** @author Coburn
** @date   2/14/2010
** 
**
** @brief Header file for runCANserver. 
** @ingroup canscanner
**
*/      
#ifndef RUNCANSERVER_H
#define RUNCANSERVER_H

/**********************************************/
/*              Prototypes                    */
/**********************************************/

void InitCANServerTask( void );
void RunCANServerTask( void );
void RunCANTimerTask( void );
void SleepTask( void );
void InitIOScanTask (void );
void RunIOScanTask( void );
void UpdateBatteryODValues(void);
//void LowPowerChargingUpdate();
void LowPowerBatteryThermScan();
//void LowPowerChargingExit();

extern OS_SEM CanTimerSem;
extern OS_SEM SleepSem;

extern void setTaskStackUsage(void);
#endif