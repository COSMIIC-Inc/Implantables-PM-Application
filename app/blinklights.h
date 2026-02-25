// Doxygen
/*!
** @file   blinklights.h
** @author Coburn
** @date   2/14/2010
**
** @brief Blinks LED at P1_16 for Kiel MCB2100 board
** @ingroup iotasks
**
*/
#ifndef BLINKLIGHTS_H
#define BLINKLIGHTS_H

// --------- ADDRESSES-----------------
//
#define PINSEL0        (*((volatile unsigned long *) 0xE002C000))
#define PINSEL1        (*((volatile unsigned long *) 0xE002C004))
#define PINSEL2        (*((volatile unsigned long *) 0xE002C014))
#define IOSET1         (*((volatile unsigned long *) 0xE0028014))
#define IODIR1         (*((volatile unsigned long *) 0xE0028018))
#define IOCLR1         (*((volatile unsigned long *) 0xE002801C))
#define ADCR           (*((volatile unsigned long *) 0xE0034000))
#define ADDR_AD        (*((volatile unsigned long *) 0xE0034004))

// -------- PROTOTYPES ----------
void BlinkLights( void );
void DelayLights(void);
void BlinkLightsInit(void);


#endif