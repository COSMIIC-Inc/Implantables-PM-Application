// Doxygen
/*!
** @file   scripts.h
** @author Coburn
** @date   5/14/2011
**
** @brief 
** @ingroup iotasks
**
*/
#ifndef RMBOOTLOADER_H
#define RMBOOTLOADER_H


#include <includes.h>
#include "gateway.h"
#include "sys.h"
#include "ObjDict.h"
#include "can.h"
#include "canfestival.h"
#include "pwrnet.h"


// prototypes
CPU_INT08U RMBootDownload( PACKET_HEADER *pkt, CPU_INT08U * data, CPU_INT08U * bufLen );
void ProcessRMBOOT(CO_Data* d, Message * m);
CPU_INT08U selectBootloaderNode(CPU_INT08U nodeNumber);
CPU_INT08U scanBootloaderNode(CPU_INT08U nodeNumber);

#endif