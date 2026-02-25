//    file: .h     HEADER FILE.

#ifndef BATPACK_H
#define BATPACK_H


// -------- DEFINITIONS ----------


// --------   DATA   ------------


// -------- PROTOTYPES ----------

void initBatteryPack( void );
void UpdateFuelGauges( UINT8 battNum );
void MaintainBatteryCharger( void );
CPU_BOOLEAN LowSystemVoltage( void );
void DisableCharging( void );
UINT8 setBattVSYS( UINT8 val );

#endif
 
