
/*
 * $Revision: 30870 $
 */
#include <stdio.h>
#include <intrinsics.h>
#include <NXP/iolpc2129.h>
#include "lpc212x.h"

#define BAUDRATE    9600
#define BAUDRATEDIVISOR (PCLKFREQ/(BAUDRATE*16))

//Masks out the leds that must be driven from pins in the range p0.16-p0.31
//when ROM-monitor is used.
#define ROM_MONITOR_DEBUG_LEDMASK     0x8003

//Masks out the pins in the range p0.16-p0.31 that should be used to drive leds
//when ROM-monitor is used.
#define ROM_MONITOR_DEBUG_LEDPINMASK  0xc000000f

#define VIC_TIMER0_bit (1 << VIC_TIMER0)
#define VIC_EINT0_bit  (1 << VIC_EINT0)
#define VIC_EINT1_bit  (1 << VIC_EINT1)
#define VIC_EINT2_bit  (1 << VIC_EINT2)
#define VIC_UART1_bit  (1 << VIC_UART1)

// Pointers to interrupt callback functions.
static void (*timer_function)();
static void (*eint0_function)();
static void (*eint1_function)();
static void (*eint2_function)();
static void (*uart1rx_function)(unsigned char);
static void (*uart1tx_function)();

void txstr(char *str);

//
// Interrupt handlers.
//

//Dummy interrupt handler, called as default in irqHandler() if no other
//vectored interrupt is called.
static void DefDummyInterrupt()
{
}

// Timer interrupt handler
static void TimerInterrupt()
{
    (*timer_function)(); // Call timer callback function.

    T0IR = 0xff; // Clear timer 0 interrupt line.
}


// The external interrupt handlers should call their callback functions
// only once for every button press. If the button is pressed the
// corresponding flag will not be reset when it is written to.
static void ExternalInterrupt0()
{
    EXTINT_bit.EINT0 = 1;   // Try to reset external interrupt flag.
    if(!EXTINT_bit.EINT0)   // Check if flag was reset (button not pressed).
        (*eint0_function)();    // Call external interrupt callback function.
}

static void ExternalInterrupt1()
{
    EXTINT_bit.EINT1 = 1;   // Try to reset external interrupt flag.
    if(!EXTINT_bit.EINT1)   // Check if flag was reset (button not pressed).
        (*eint1_function)();    // Call external interrupt callback function.
}

static void ExternalInterrupt2()
{
    EXTINT_bit.EINT2 = 1;   // Try to reset external interrupt flag.
    if(!EXTINT_bit.EINT2)   // Check if flag was reset (button not pressed).
        (*eint2_function)();    // Call external interrupt callback function.
}


//UART1 interrupt handler
static void UART1Interrupt()
{
    switch(U1IIR_bit.IID)
    {
        case 0x1:  //THRE interrupt
            (*uart1tx_function)(); //Call tx buffer empty callback function
            break;
        case 0x2:  //Receive data available
            (*uart1rx_function)(U1RBR);    //Call received byte callback function
            break;
        case 0x0:  //Modem interrupt
        case 0x3:  //Receive line status interrupt (RDA)
        case 0x6:  //Character time out indicator interrupt (CTI)
        default:
            break;
    }
}

// IRQ exception handler. Calls the interrupt handlers.
__irq __arm void IRQ_Handler(void)
{
    void (*interrupt_function)();
    unsigned int vector;
//  static  unsigned long leds = 0x10000;

//  LPC212xLedSet(leds,0xff0000);
//  leds += 0x010000;
//  leds &= 0xff0000;

    vector = VICVectAddr;   // Get interrupt vector.
    interrupt_function = (void(*)())vector;
    (*interrupt_function)();  // Call vectored interrupt function.

    VICVectAddr = 0;        // Clear interrupt in VIC.
}

//
// System initialization.
//
void LPC212xSystemInit()
{
#ifdef JTAG_DEBUG
    // Map lowest 64 bytes of the address space to bottom of internal RAM.
    // This will move the exception vectors in place.
//  MEMMAP = 2;
#endif
}


//
// Interrupt controller initalization.
//

void LPC212xInitVIC()
{
    // Setup interrupt controller.
    VICProtection = 0;
    // Disable all interrupts but the one used for the ROM monitor
    VICIntEnClear = 0xffffffbf;
    VICDefVectAddr = (unsigned int)&DefDummyInterrupt;
    VICVectAddr = 0;        // Clear interrupt in VIC.
}

void LPC212xInitTimerInterrupt(void(*timer_func)())
{
    // Setup timer callback function.
    timer_function = timer_func;

    VICIntSelect &= ~VIC_TIMER0_bit; // IRQ on timer 0 line.
    VICVectAddr1 = (unsigned int)&TimerInterrupt;
    VICVectCntl1 = 0x20 | VIC_TIMER0; // Enable vector interrupt for timer 0.
    VICIntEnable = VIC_TIMER0_bit;    // Enable timer 0 interrupt.
}

void LPC212xInitExternalInterrupt0(void(*eint0_func)())
{
    // Setup eint0 callback function.
    eint0_function = eint0_func;

    VICIntSelect &= ~VIC_EINT0_bit;  // IRQ on external int 0.
    VICVectAddr2 = (unsigned int)&ExternalInterrupt0;
    VICVectCntl2 = 0x20 | VIC_EINT0; // Enable vector interrupt for EINT 0.
    VICIntEnable = VIC_EINT0_bit;    // Enable EINT 0 interrupt.
}

void LPC212xInitExternalInterrupt1(void(*eint1_func)())
{
    // Setup eint1 callback function.
    eint1_function = eint1_func;

    VICIntSelect &= ~VIC_EINT1_bit;  // IRQ on external int 1.
    VICVectAddr3 = (unsigned int)&ExternalInterrupt1;
    VICVectCntl3 = 0x20 | VIC_EINT1; // Enable vector interrupt for EINT1.
    VICIntEnable = VIC_EINT1_bit;    // Enable EINT 1 interrupt.
}

void LPC212xInitExternalInterrupt2(void(*eint2_func)())
{
    // Setup eint2 callback function.
    eint2_function = eint2_func;

    VICIntSelect &= ~VIC_EINT2_bit;  // IRQ on external int 2.
    VICVectAddr4 = (unsigned int)&ExternalInterrupt2;
    VICVectCntl4 = 0x20 | VIC_EINT2; // Enable vector interrupt for EINT2.
    VICIntEnable = VIC_EINT2_bit;    // Enable EINT 2 interrupt.
}

void LPC212xInitUART1Interrupt(void(*uart1rx_func)(unsigned char),
                               void(*uart1tx_func)())
{
    // Setup uart1 callback functions.
    uart1rx_function = uart1rx_func;
    uart1tx_function = uart1tx_func;

    VICIntSelect &= ~VIC_UART1_bit;  // IRQ on UART1.
    VICVectAddr5 = (unsigned int)&UART1Interrupt;
    VICVectCntl5 = 0x20 | VIC_UART1; // Enable vector interrupt for UART1.
    VICIntEnable = VIC_UART1_bit;    // Enable UART 1 interrupt.
}


//
// Timer functions.
//
void LPC212xInitTimer()
{
    T0TCR = 0; // Disable timer 0.
    T0PC = 0; // Prescaler is set to no division.
    T0MR0 = PCLKFREQ/1000; // Count up to this value. Generate 1000 Hz interrupt.
    T0MCR = 3; // Reset and interrupt on MR0 (match register 0).
    T0CCR = 0; // Capture is disabled.
    T0EMR |= 0x30;
    PINSEL0 |= 0x80;
}


void LPC212xStartTimer()
{
    T0TCR = 1; // Enable timer 0.
}


//
// External interrupt functions
//

void LPC212xInitEINT0()
{
    PINSEL1_bit.P0_16=0x1; // Set pin function to EINT0
}

void LPC212xInitEINT1()
{
    PINSEL0_bit.P0_14=0x2; // Set pin function to EINT1
}

void LPC212xInitEINT2()
{
    PINSEL0_bit.P0_15=0x2; // Set pin function to EINT2
}

unsigned char EINT0PinIsLow()
{
    return !IO0PIN_bit.P0_16;
}

unsigned char EINT1PinIsLow()
{
    return !IO0PIN_bit.P0_14;
}

unsigned char EINT2PinIsLow()
{
    return !IO0PIN_bit.P0_15;
}


//
// Parallel I/O functions.
//

void LPC212xInitPIO()
{
#ifdef ROM_MONITOR_DEBUG
    unsigned int dir;
    //Uses some of the GPIO pins P0.0-P0.15
    PINSEL0 &= ROM_MONITOR_DEBUG_LEDPINMASK;
    //Uses some of the GPIO pins P0.16-P0.31
    PINSEL1 &= ~ROM_MONITOR_DEBUG_LEDPINMASK;

    // Set LED pins on GPIO to output.
    dir =   (ROM_MONITOR_DEBUG_LEDMASK << 16)
        | ~ROM_MONITOR_DEBUG_LEDMASK;
    IODIR |= dir;
#else
    IO1DIR |= 0x00ff0000;
#endif
}


//
// LED output drivers.
//

void LPC212xLedSet(unsigned long setmask, unsigned long clearmask)
{
    // Set LED DS1 to DS16.
#ifdef ROM_MONITOR_DEBUG
    // When the ROM-monitor is used some LED:s are driven by pins in the range
    // P0.0-P0.15 and some by pins in the range P0.16-P0.31.
    unsigned int clrhi  = (ROM_MONITOR_DEBUG_LEDMASK  & clearmask) << 16;
    unsigned int clrlow = (~ROM_MONITOR_DEBUG_LEDMASK & clearmask);
    unsigned int sethi  = (ROM_MONITOR_DEBUG_LEDMASK  & setmask) << 16;
    unsigned int setlow = (~ROM_MONITOR_DEBUG_LEDMASK & setmask);
    IOCLR = clrhi | clrlow;
    IOSET = sethi | setlow;
#else
    //When the program is run with J-Link or standalone, the LED:s are driven by
    //pins P0.0-P0.15.
    IO1CLR = clearmask;
    IO1SET = setmask;
#endif
}


//
// UART functions
//

void LPC212xInitUART1()
{
    PINSEL0 |= (0x5 << 16);

    //Set the FIFO enable bit in the FCR register. This bit must be set for
    //proper UART operation.
    U1FCR = 1;

    //Set baudrate
    U1LCR_bit.DLAB = 1;
    U1DLL = BAUDRATEDIVISOR & 0x00ff;
    U1DLM = (BAUDRATEDIVISOR >> 8) & 0x00ff;
    U1LCR_bit.DLAB = 0;

    //Set mode
    U1LCR_bit.WLS = 0x3;   //8 bit word length
    U1LCR_bit.SBS = 0x0;   //1 stop bit
    U1LCR_bit.PE  = 0x0;   //No parity

    //Enable UART1 interrupts
    U1IER_bit.RDAIE  = 1;  //Enable byte received interrupt
    U1IER_bit.THREIE = 1;  //Enable tx buf empty interrupt
}

//Transmits one byte via UART1
//Note: The UART1 THRE register must be empty before the call to this
//      function. Otherwise data can be lost.
void LPC212xUART1TxByte(unsigned char byte)
{
    U1THR = byte;
}


void txbyte(char byte)
{
    while (!(U1LSR &0x20));
    U1THR = byte;
}

void txstr(char *str)
{
    while (*str)
        txbyte (*str++);
}
