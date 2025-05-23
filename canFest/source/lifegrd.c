/*
  This file is part of CanFestival, a library implementing CanOpen
  Stack.

  Copyright (C): Edouard TISSERANT and Francis DUPIN
  modified by JDC Consulting

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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
  USA
*/

/*!
** @file   lifegrd.c
** @author Edouard TISSERANT
** @date   Mon Jun  4 17:19:24 2007
**
** @brief
**
**
*/

#include <data.h>
#include "lifegrd.h"
#include "canfestival.h"
#include "sysdep.h"

/*******************Data*****************************/
UNS16 serialNumberTable[ACTIVE_NODE_COUNT];
UNS8 duplicateNode = 0;

void ConsumerHearbeatAlarm(CO_Data* d, UNS32 id);
void ProducerHearbeatAlarm(CO_Data* d, UNS32 id);
UNS32 OnHearbeatProducerUpdate(CO_Data* d, const indextable * unsused_indextable, UNS8 unsused_bSubindex);

/*!
**
**
** @param d
** @param nodeId
**
** @return
**/
e_nodeState getNodeState (CO_Data* d, UNS8 nodeId)
{
  e_nodeState networkNodeState = d->NMTable[nodeId];
  return networkNodeState;
}

/*! 
** The Consumer Timer Callback
**
** @param d
** @param id
**/
void ConsumerHearbeatAlarm(CO_Data* d, UNS32 id)
{
  /*MSG_WAR(0x00, "ConsumerHearbeatAlarm", 0x00);*/
  
  UNS8 nodeID;
  /* timer has been notified and is now free (non periodic)*/
  /* -> avoid deleting re-assigned timer if message is received too late*/
  d->ConsumerHeartBeatTimers[id]=TIMER_NONE;
  /*! call heartbeat error with NodeId */
  // reset table entry to 0x0F
  nodeID = (UNS8)( ((d->ConsumerHeartbeatEntries[id]) & (UNS32)0x00FF0000) >> (UNS8)16 );
  d->NMTable[nodeID] = Unknown_state; 
}

/*!
**
**
** @param d
** @param m
**/
void processNODE_GUARD(CO_Data* d, Message* m )
{
  UNS8 nodeId = (UNS8) GET_NODE_ID((*m));

  if((m->rtr == 1) )  // not used by NNP
    /*!
    ** Notice that only the master can have sent this
    ** node guarding request
    */
    {
      /*!
      ** Receiving a NMT NodeGuarding (request of the state by the
      ** master)
      ** Only answer to the NMT NodeGuarding request, the master is
      ** not checked (not implemented)
      */
      if (nodeId == *d->bDeviceNodeId )
        {
          Message msg;
          UNS16 tmp = *d->bDeviceNodeId + 0x700;
          msg.cob_id = UNS16_LE(tmp);
          msg.len = (UNS8)0x01;
          msg.rtr = 0;
          msg.data[0] = d->nodeState;
          if (d->toggle)
            {
              msg.data[0] |= 0x80 ;
              d->toggle = 0 ;
            }
          else
            d->toggle = 1 ;
          /* send the nodeguard response. */
          MSG_WAR(0x3130, "Sending NMT Nodeguard to master, state: ", d->nodeState);
          canSend(d->canHandle,&msg );
        }

    }
  else
    { /* incoming slave heartbeat -- update the node table */
      MSG_WAR(0x3110, "Received NMT nodeId : ", nodeId);
      /* the slave's state receievd is stored in the NMTable */
      /* The state is stored on 7 bit */
      d->NMTable[nodeId] = (e_nodeState) ((*m).data[0] & 0x7F) ;
      
           /* load the Serial Number table   */
      if (serialNumberTable[nodeId] == 0)
        serialNumberTable[nodeId] = (*m).data[1] + (*m).data[2] * 256;
      else if (serialNumberTable[nodeId] != (*m).data[1] + (*m).data[2] * 256)
        duplicateNode = nodeId;
      else
        duplicateNode = 0;
      
      /* check for consumer heartbeat   */
      if( d->NMTable[nodeId] != Unknown_state ) 
      {
        UNS8 index, ConsummerHeartBeat_nodeId ;
        for( index = (UNS8)0x00; index < *d->ConsumerHeartbeatCount; index++ )
          {
            ConsummerHeartBeat_nodeId = (UNS8)( ((d->ConsumerHeartbeatEntries[index]) & (UNS32)0x00FF0000) >> (UNS8)16 );
            if ( nodeId == ConsummerHeartBeat_nodeId )
              {
                TIMEVAL time = ( (d->ConsumerHeartbeatEntries[index]) & (UNS32)0x0000FFFF ) ;
                /* Renew alarm for next heartbeat. */
                DelAlarm(d->ConsumerHeartBeatTimers[index]);
                d->ConsumerHeartBeatTimers[index] = SetAlarm(d, index, &ConsumerHearbeatAlarm, MS_TO_TIMEVAL(time), 0);
              }
          }
      }
    }
}

/*! The Consumer Timer Callback
**
**
** @param d
** @param id
**/
void ProducerHearbeatAlarm(CO_Data* d, UNS32 id)
{
  //return;
  if(*d->ProducerHeartBeatTime)
    {
      Message msg;
      /* Time expired, the heartbeat must be sent immediately
      ** generate the correct node-id: this is done by the offset 1792
      ** (decimal) and additionaly
      ** the node-id of this device.
      */
      UNS16 tmp = *d->bDeviceNodeId + 0x700;
      msg.cob_id = UNS16_LE(tmp);
      msg.len = (UNS8)0x01;
      msg.rtr = 0;
      msg.data[0] = d->nodeState; /* No toggle for heartbeat !*/
      /* send the heartbeat */
      MSG_WAR(0x3130, "Producing heartbeat: ", d->nodeState);
      canSend(d->canHandle,&msg );

    }
  else
    {
      d->ProducerHeartBeatTimer = DelAlarm(d->ProducerHeartBeatTimer);
    }
}

/*! This is called when Index 0x1017 is updated.
**
**
** @param d
** @param unsused_indextable
** @param unsused_bSubindex
**
** @return
**/
UNS32 OnHeartbeatProducerUpdate(CO_Data* d, const indextable * unsused_indextable, UNS8 unsused_bSubindex)
{
  heartbeatStop(d);

  if (!(*(d->iam_a_slave))) { return 0; } // ^^^replace heartbeatInit -- slave is part of node structure
  heartbeatInit(d);
  return 0;
}

/*!
**
**
** @param d
**/
void heartbeatInit(CO_Data* d)
{
  INTEGER16 test = 0;
  UNS8 index; /* Index to scan the table of heartbeat consumers */
  RegisterSetODentryCallBack(d, 0x1017, 0x00, &OnHeartbeatProducerUpdate);

  d->toggle = 0;

  for( index = (UNS8)0x00; index < *d->ConsumerHeartbeatCount; index++ )
    {
      TIMEVAL time = (UNS16) ( (d->ConsumerHeartbeatEntries[index]) & (UNS32)0x0000FFFF ) ;
      /* MSG_WAR(0x3121, "should_time : ", should_time ) ; */
      if ( time )
        {
           test = SetAlarm(d, index, &ConsumerHearbeatAlarm, MS_TO_TIMEVAL(time), 0);
          d->ConsumerHeartBeatTimers[index] = test;
        }
    }

  if ( *d->ProducerHeartBeatTime )
    {
      TIMEVAL time = *d->ProducerHeartBeatTime;
      d->ProducerHeartBeatTimer = SetAlarm(d, 0, &ProducerHearbeatAlarm, MS_TO_TIMEVAL(time), MS_TO_TIMEVAL(time));
    }
}

/*!
**
**
** @param d
**/
void heartbeatStop(CO_Data* d)
{
  UNS8 index;
  for( index = (UNS8)0x00; index < *d->ConsumerHeartbeatCount; index++ )
    {
      d->ConsumerHeartBeatTimers[index] = DelAlarm(d->ConsumerHeartBeatTimers[index]);
    }

  d->ProducerHeartBeatTimer = DelAlarm(d->ProducerHeartBeatTimer);
}

/*!
**
**
** @param heartbeatID
**/
void _heartbeatError(CO_Data* d, UNS8 heartbeatID){}
void _post_SlaveBootup(CO_Data* d, UNS8 SlaveID){}
