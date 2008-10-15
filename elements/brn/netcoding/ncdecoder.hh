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

#ifndef NCDECODER_HH
#define NCDECODER_HH

#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/brn/brn.h>
#include "netcoding.h"
#include "nccache.hh"
#include "ncdecodingbatch.hh"
#include "tracecollector.hh"

#define NCDECODER_OUTPUT_DATA 0
#define NCDECODER_OUTPUT_STOP 1
#define NCDECODER_INPUT_DATA 0
#define NCDECODER_INPUT_FEEDBACK 1



CLICK_DECLS

/**
 * Decoder puts encoded fragments into the cache and retrieves plain fragments.
 * Also it sends STOP when it recognizes a batch as fully decoded (or decodable)
 * and resends both START and STOP if it gets a negative TX feedback
 * 
 * The decoder needs to keep fully decoded packets around for some time, 
 * so that STOP packets can be resent if needed. A queue where the batches are
 * sorted by age is used for that.
 */
typedef DEQueue<NCDecodingBatch *> BatchList;

class NetcodingDecoder : public Element
{

public:
  NetcodingDecoder() : cache(NULL), hardTimeout(0) {}
  ~NetcodingDecoder() {}
  int configure(Vector<String> &conf, ErrorHandler *errh);
  const char* class_name() const { return "NetcodingDecoder"; }
  const char *port_count() const  { return "2/2"; }
  const char *processing() const { return "PUSH"; }
  void push(int, Packet *);

private:
  NetcodingCache * cache;
  Packet * assembleStopPacket(NCDecodingBatch * batch, uint8_t numPackets);
  void handleStopFeedback(Packet * p);
  void trace(const click_brn_dsr * dsrHeader, char pos, bool noninnovative);
  BatchList batches;
  Timestamp hardTimeout;
  TraceCollector * tracer;
  static int _debug;
};

CLICK_ENDDECLS

#endif
