/* Copyright (C) 2005 BerlinRoofNet Lab
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
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/confparse.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brnsetgatewayonflow.hh"
#include "brnpacketbuffer.hh"


CLICK_DECLS

BRNPacketBuffer::BRNPacketBuffer() :
  _debug(0),
  _stamp(0),
  _trigger(NULL)
{
}

BRNPacketBuffer::~BRNPacketBuffer() {}

int
BRNPacketBuffer::configure(Vector<String> &conf, ErrorHandler *errh) {

  if (cp_va_kparse(conf, this, errh,
                  "SETGATEWAYONFLOW", cpkP+cpkM, cpElement, &_trigger,
                  "BUFFERTIME", cpkP+cpkM, cpUnsigned,/* "time (ms) to buffer packets of a bucket",*/ &_stamp,
                  cpEnd) < 0)
    return -1;

  if (_stamp <= 0)
    return errh->error("time (ms) must be > 0");

  if (_trigger->cast("BRNSetGatewayOnFlow") == 0) {
    return errh->error("No element of type BRNSetGatewayOnFlow specified.");
  }

  return 0;
}

int
BRNPacketBuffer::initialize(ErrorHandler *errh) {
  (void) (errh);

  return 0;
}

void
BRNPacketBuffer::push(int, Packet *p) {
  BRN_DEBUG("Getting packet.");

  // test, if packet needs to be buffered
  if (_trigger->buffer_packet(p))
  {
    // put it in bucket
    uint32_t bucket = _trigger->get_bucket(p);
    PacketBuffer *buffer;

    if ((buffer = _buffer.findp(bucket)) != NULL) {
      // bucket already known => just store packet
      buffer->push_back(BufferedPacket(p));

      BRN_DEBUG("Buffering packet with pointer 0x%x.", p);
    }
    else {
      // create buffer
      if ( ! _buffer.insert(bucket, PacketBuffer()) ) {
        BRN_ERROR("Insert of packet into buffer failed");
      }

      // ... and insert packet
      _buffer.findp(bucket)->push_back(BufferedPacket(p));
      
      // ... and start timer
      Timer *t = new Timer(this);
      t->initialize(this);
      t->schedule_after_msec(_stamp);
      _buckettimer.push_back(BucketTimer(t, bucket));
      
      BRN_DEBUG("Created bucket %u.", bucket);
    }
    
    return;
  }
  else {
    BRN_DEBUG("Pushing packet to port 0.");
    output(0).push(p);
    return;
  }
}

void
BRNPacketBuffer::flush_bucket(uint32_t bucket) {
  PacketBuffer *buffer;
  if ((buffer = _buffer.findp(bucket)) != NULL) {
    // push all packets from buffer
    while (buffer->size() > 0) {
      BRN_INFO("Flushing packet for bucket %u. %u packets in bucket.", bucket, buffer->size());
      BufferedPacket q = buffer->front();
      buffer->pop_front();
      
      BRN_DEBUG("Flushing packet with pointer 0x%x.", q._p);
      output(0).push(q._p);
    }

    // .. and remove bucket
    BRN_INFO("Flushing packets => Removed bucket %u.", bucket);
    _buffer.remove(bucket);
  }
  else {
    BRN_INFO("Bucket %u is unknown. Have no packets in this bucket.", bucket);
  }
}

void
BRNPacketBuffer::run_timer(Timer *t) {
    assert(_buckettimer.size() > 0);

    // get timed bucket
    BucketTimer buckettimer = _buckettimer.front();
    uint32_t bucket = buckettimer.get_bucket();

    BRN_INFO("Timer for bucket %u is scheduled.", bucket);

    // this must be the same timer we scheduled
    assert(t == buckettimer.get_timer());
    // clean up timer
    _buckettimer.erase(_buckettimer.begin());
    delete t;

    if (_buffer.findp(bucket) == NULL) {
      BRN_INFO("Bucket %u has already been flushed.", bucket);
      return;
    }

    flush_bucket(bucket);
}

EXPORT_ELEMENT(BRNPacketBuffer)
CLICK_ENDDECLS
