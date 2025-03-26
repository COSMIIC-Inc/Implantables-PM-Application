//    file: sys.h    System (common) HEADER FILE.

#ifndef _SYS_H
#define _SYS_H


// -------- DEFINITIONS ----------
typedef signed char 		INT8;
typedef short 				INT16;
typedef int 				INT32;


typedef unsigned char 		UINT8;
typedef unsigned short 		UINT16;
typedef unsigned int 		UINT32;

typedef union
{	UINT8  b[2];
	UINT16 s;
	INT16  i;
}BW16;

typedef union
{	UINT8  b[4];
	UINT16 s[2];
	UINT32 w;
}BW32;


#define TRUE    1
#define FALSE   0
 
#define ON      1
#define OFF     0

#define CLR_BITS(a,b)     ((a)&=~(b))
#define SET_BITS(a,b)     ((a)|=(b))
#define BITS_TRUE(a,b)    ((a)&(b))

#define B(n)			(1<<(n))

#define BIT0			B(0)
#define BIT1			B(1)
#define BIT2			B(2)
#define BIT3			B(3)
#define BIT4			B(4)
#define BIT5			B(5)
#define BIT6			B(6)
#define BIT7			B(7)

#define BIT8			B(8)
#define BIT9			B(9)
#define BIT10			B(10)
#define BIT11			B(11)
#define BIT12			B(12)
#define BIT13			B(13)
#define BIT14			B(14)
#define BIT15			B(15)

#define BIT16			B(16)
#define BIT17			B(17)
#define BIT18			B(18)
#define BIT19			B(19)
#define BIT20			B(20)
#define BIT21			B(21)
#define BIT22			B(22)
#define BIT23			B(23)

#define BIT24			B(24)
#define BIT25			B(25)
#define BIT26			B(26)
#define BIT27			B(27)
#define BIT28			B(28)
#define BIT29			B(29)
#define BIT30			B(30)
#define BIT31			B(31)


/******************************************************************************
	PROJECT SPECIFIC HEADER
******************************************************************************/ 


#endif
 
