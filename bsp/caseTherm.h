//    file: .h     HEADER FILE.

#ifndef CASE_THERM_H
#define CASE_THERM_H


// -------- DEFINITIONS ----------
#define TURN_ON_THERMISTOR()		(IO1SET = BIT16)
#define TURN_OFF_THERMISTOR()		(IO1CLR = BIT16)

// --------   DATA   ------------


// -------- PROTOTYPES ----------

void initCaseThermometer( void );
void updateCaseThermometer( UINT8 state );



#endif
 
