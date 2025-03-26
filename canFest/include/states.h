/*
This file is part of CanFestival, a library implementing CanOpen Stack. 

Copyright (C): Edouard TISSERANT and Francis DUPIN
modifed by JDC Consulting

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

/** @defgroup statemachine State Machine
 *  @ingroup userapi
 */
 
#ifndef __states_h__
#define __states_h__

#include <applicfg.h>
#include "data.h"
#include "os.h"

/* The nodes states 
 * -----------------
 * values are choosen so, that they can be sent directly
 * for heartbeat messages...
 * Must be coded on 7 bits only
 * */

enum enum_nodeState 
{
  Hibernate                 = 0x00, 
  Waiting                   = 0x01,
  Mode_X_Manual             = 0x02,
  Mode_Y_Manual             = 0x03,
  Stopped                   = 0x04,
  Mode_Patient_Control      = 0x05,
  BootCheckReset            = 0x06,
  Mode_Patient_Manual       = 0x07,
  Mode_Produce_X_Manual     = 0x08,
  Mode_Record_X             = 0x09,
  Mode_Charging             = 0x0A,
  Error_State               = 0x0E,
  Unknown_state             = 0x0F
 
};

typedef enum enum_nodeState e_nodeState;

/* Communication state structure */
typedef struct
{
	INTEGER8 csBoot_Up;
	INTEGER8 csSDO;
	INTEGER8 csEmergency;
	INTEGER8 csSYNC;
	INTEGER8 csHeartbeat;
	INTEGER8 csPDO;
	INTEGER8 csLSS;
} s_state_communication;

    
/** 
 * @brief Function that user app can overload
 * @ingroup statemachine
 */
typedef void (*mode_X_Manual_t)(CO_Data*);
typedef void (*mode_Y_Manual_t)(CO_Data*);
typedef void (*waiting_t)(CO_Data*);
typedef void (*stopped_t)(CO_Data*);
typedef void (*mode_Patient_Manual_t)(CO_Data*);
typedef void (*mode_Patient_Control_t)(CO_Data*);


/** 
 * @ingroup statemachine
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
void _mode_X_Manual(CO_Data* d);

/** 
 * @ingroup statemachine
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
void _mode_Y_Manual(CO_Data* d);

/**
 * @ingroup statemachine 
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
void _waiting(CO_Data* d);

/** 
 * @ingroup statemachine
 * @brief Function that user app can overload
 * @param *d Pointer on a CAN object data structure
 */
void _stopped(CO_Data* d);
void _mode_Patient_Manual(CO_Data* d);
void _mode_Patient_Control(CO_Data* d);
void _mode_Produce_X_Manual(CO_Data* d);


/************************* prototypes ******************************/

/** 
 * @brief Called by driver/app when receiving messages
 * @param *d Pointer on a CAN object data structure
 * @param *m Pointer on a CAN message structure
 */
void canDispatch(CO_Data* d, Message *m);

/** 
 * @ingroup statemachine
 * @brief Returns the state of the node
 * @param *d Pointer on a CAN object data structure
 * @return The node state
 */
e_nodeState getState (CO_Data* d);

/** 
 * @ingroup statemachine
 * @brief Change the state of the node 
 * @param *d Pointer on a CAN object data structure
 * @param newState The state to assign
 * @return 
 */
UNS8 setState (CO_Data* d, e_nodeState newState);

/**
 * @ingroup statemachine 
 * @brief Returns the nodId 
 * @param *d Pointer on a CAN object data structure
 * @return
 */
UNS8 getNodeId (CO_Data* d);

/** 
 * @ingroup statemachine
 * @brief Define the node ID. Initialize the object dictionary
 * @param *d Pointer on a CAN object data structure
 * @param nodeId The node ID to assign
 */
void setNodeId (CO_Data* d, UNS8 nodeId);

/** 
 * @brief Some stuff to do when the node enter in pre-operational mode
 * @param *d Pointer on a CAN object data structure
 */
void initPreOperationalMode (CO_Data* d);

#endif
