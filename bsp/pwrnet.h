//    file: .h     HEADER FILE.

#ifndef PWRNET_H
#define PWRNET_H

#include <stdio.h>
#include <stdlib.h>
#include <includes.h>
#include "sys.h"
#include "i2c.h"
#include "ObjDict.h"
#include "can_cpu.h"
#include "gateway.h"

// -------- DEFINITIONS ----------
#define ENABLE_PWR_NETWORK()		(IO1CLR = BIT19)
#define DISABLE_PWR_NETWORK()		(IO1SET = BIT19)
#define IS_PWR_NETWORK_ENABLED()	(IO1PIN_bit.P1_19 == 0)

#define IS_PWR_NET_CTRL_ON()		(NetworkPowerControl & BIT0)

// --------   DATA   ------------


// -------- PROTOTYPES ----------

void InitPowerNetwork( void );
UINT8 updatePowerNetwork( void );




#endif
 
