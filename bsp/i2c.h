//    file: .h     HEADER FILE.

#ifndef I2C_H
#define I2C_H


// -------- DEFINITIONS ----------
#define TWI_NO_ERROR            0
#define TWI_ERROR_STOPPED       1
#define TWI_TIMEOUT_STOPPED     2
#define TWI_ERROR_UNKNOWN       3
#define TWI_TIMEOUT_UNKNOWN     4

#define TWI_ADDR_WRITE          0
#define TWI_ADDR_READ           1

#define TWI_START               0
#define TWI_REPEATSTART         1

#define TWI_MAX_TRIES         2

// --------   DATA   ------------


// -------- PROTOTYPES ----------

void initTwiHdwr( void );
UINT8 startTwi( UINT8 type );
UINT8 stopTwi( void );
UINT8 sendTwiAddr( UINT8 addr, UINT8 type);
UINT8 sendTwiData( UINT8 *data, UINT8 n);
UINT8 readTwiData( UINT8 *data, UINT8 n);
UINT8 restoreI2C(void);



#endif
 
