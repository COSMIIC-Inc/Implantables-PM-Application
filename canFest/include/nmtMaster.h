/*
This file is part of CanFestival, a library implementing CanOpen Stack. 

Copyright (C): Edouard TISSERANT and Francis DUPIN

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/** @defgroup networkmanagement Network Management
 *  @ingroup userapi
 */
/** @defgroup nmtmaster NMT Master
 *  @brief NMT master provides mechanisms that control and monitor the state of nodes and their behavior in the network.
 *  @ingroup networkmanagement
 */
 
#ifndef __nmtMaster_h__
#define __nmtMaster_h__

#include "data.h"

/** 
 * @ingroup nmtmaster
 * @brief Transmit a NMT message on the network to the slave whose nodeId is node ID.
 * 
 * @param *d Pointer to a CAN object data structure
 * @param nodeId Id of the slave node
 * @param cs The order of state changement \n\n
 * 
 * @return errorcode
 *                   - 0 if the NMT message was send
 *                   - 1 if an error occurs 
 */
UNS8 masterSendNMTstateChange (CO_Data* d, UNS8 nodeId, UNS8 cs[]);

/**
 * @ingroup nmtmaster 
 * @brief Transmit a NodeGuard message on the network to the slave whose nodeId is node ID
 * 
 * @param *d Pointer to a CAN object data structure
 * @param nodeId Id of the slave node
 * @return
 *         - 0 is returned if the NodeGuard message was send.
 *         - 1 is returned if an error occurs.
 */
UNS8 masterSendNMTnodeguard (CO_Data* d, UNS8 nodeId);

/** 
 * @ingroup nmtmaster
 * @brief Ask the state of the slave node whose nodeId is node Id.
 * 
 * To ask states of all nodes on the network (NMT broadcast), nodeId must be equal to 0
 * @param *d Pointer to a CAN object data structure
 * @param nodeId Id of the slave node
 */
void masterRequestNodeState (CO_Data* d, UNS8 nodeId);
void MasterResetNodeTable(CO_Data* d );
void MasterRequestNodeTable(CO_Data* d, UNS8* temp);
void ProcessNMTLocalStateChange(CO_Data* d, UNS8 Command[]);




#endif /* __nmtMaster_h__ */
