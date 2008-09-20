/* Copyright (C) 2008 Ulf Hermann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 */

#ifndef NCENCODER_HH
#define NCENCODER_HH

#include <click/element.hh>
#include <click/hashmap.hh>
#include <elements/brn/brn.h>
#include <elements/brn/common.hh>
#include <elements/standard/simplequeue.hh>
#include "netcoding.h"
#include "nccache.hh"
#include "packetarithmetics.hh"
#include "hashset.hh"
#include "notificationclient.hh"
#include "batchidfilter.hh"

#define NCENCODER_INPUT_DATA 0
#define NCENCODER_INPUT_STOP 1

#define NCENCODER_OUTPUT_DATA 0
#define NCENCODER_OUTPUT_START 1

// how often per second is the send trigger called when nothing else is sent
#define SEND_TRIGGER_RATE 100

CLICK_DECLS


typedef HashSet<uint32_t> IdSet;
typedef DEQueue<NCEncodingBatch *> BatchQueue;


class NetcodingEncoder : public NotificationClient
{

public:
  enum WaitState {SENDING, SHORT, LONG};
  NetcodingEncoder(): _debug(BrnLogger::DEBUG), timerHits(0), cache(NULL), 
  	trigger(this), longWait(0), shortWait(0), waitState(SENDING) {}
  ~NetcodingEncoder() {}
  int configure(Vector<String> &conf, ErrorHandler *errh);
  int initialize(ErrorHandler *errh);
  const char* class_name() const { return "NetcodingEncoder"; }
  const char *port_count() const  { return "2/2"; }
  const char *processing() const { return "PUSH"; }
  void *cast(const char * name);
  void run_timer(Timer *timer);
  void push(int, Packet *);
  void notify();

private:
  int _debug;
  unsigned long timerHits;
  unsigned fragmentsInPacket;
  NetcodingCache * cache;
  SimpleQueue * sendQueue;
  NodeIdentity * me;
  Timer trigger;
  BatchQueue schedule;
  IdSet unknownStarts;
  Timestamp longWait; // time to wait when START for a different node is received
  Timestamp shortWait; // time to wait after transmitting MAX_PACKETS_IN_BATCH packets 
  WaitState waitState;

  

  void purgeBatch(NCEncodingBatch * batch);
  bool isUpstream(const NCEncodingBatch * b, const click_ether * ether);
  void triggerSend();
  void trySchedule(NCEncodingBatch * batch);
  void replacePackets(NCEncodingBatch * batch);
  int directSend(NCEncodingBatch * batch);
  void sendStart(NCDecodingBatch * batch);
  void sendStop(NCDecodingBatch * batch); 
  Packet * getStartOrStop(NCForwardBatch * in_batch, uint8_t numPackets, uint8_t startNext = 0);
  NCEncodingBatch * handleStop(NCEncodingBatch * batch);
  void handleStart(NCEncodingBatch * batch);
  void wait(WaitState time);
  void handleStartStop(Packet * p);
  void checkUnknownStarts(NCEncodingBatch * batch);
  void propagateStop(uint32_t batchId, bool startNext);
};

CLICK_ENDDECLS

#endif
