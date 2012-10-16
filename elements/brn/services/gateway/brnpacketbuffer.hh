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

#ifndef BRNPACKETBUFFER_HH
#define BRNPACKETBUFFER_HH

#include <click/bighashmap.hh>
#include <click/deque.hh>
#include <click/vector.hh>

CLICK_DECLS

class Timer;
class Timestamp;
class BRNSetGatewayOnFlow;

class BRNPacketBuffer : public Element {
public:

    BRNPacketBuffer();
    ~BRNPacketBuffer();

    const char *class_name() const { return "BRNPacketBuffer"; }
    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);
    const char *port_count() const { return "1/1"; }
    const char *processing() const { return PUSH; }
	
    const char *flow_code () const { return "x/x"; }
    
    void run_timer(Timer *);
   
    void push(int port, Packet *p);

    /*
     * Flush all packets from the specified bucket
     * 
     */
    void flush_bucket(uint32_t bucket);

    int _debug;
private:

    class BufferedPacket
    {
      public:
      Packet *_p;

      BufferedPacket(Packet *p) {
        assert(p);
        _p=p;
      }
      void check() const { assert(_p); }
    };

    typedef Deque<BufferedPacket> PacketBuffer;
    typedef HashMap<uint32_t, PacketBuffer> BucketPacketBuffer;
    typedef BucketPacketBuffer::iterator BucketPacketBufferIter;
    
    class BucketTimer
    {
      private:
      Timer *_t;
      uint32_t _bucket;

      public:
      BucketTimer(Timer *t, uint32_t bucket) {
        assert(t != NULL);
        assert(bucket != 0);
        _t = t;
        _bucket = bucket;
      }
      
      Timer* get_timer() {
        return _t;
      }
      
      uint32_t get_bucket() {
        return _bucket;
      }
      
    };
    
    typedef Vector<BucketTimer> BucketTimers; // sorted list of bucket scheduled via a Timer; first scheduled timer first
    
    BucketPacketBuffer _buffer;
    
    BucketTimers _buckettimer;
    uint32_t _stamp;
    
    BRNSetGatewayOnFlow* _trigger;
    
};

CLICK_ENDDECLS
#endif
