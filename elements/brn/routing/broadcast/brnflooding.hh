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

#ifndef BRNFLOODINGELEMENT_HH
#define BRNFLOODINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>


CLICK_DECLS
/*
 * =c
 * BrnFlooding()
 * =s
 * displays dhcp packets
 * =d
 */

#define MAX_QUEUE_SIZE 1500

class BrnFlooding : public Element {

 public:

  class BrnBroadcast
  {
    public:
      uint16_t      bcast_id;
      uint8_t       dsr_dst[6];
      uint8_t       dsr_src[6];

      BrnBroadcast( uint16_t _id, uint8_t *_src )
      {
        bcast_id = _id;
        memcpy(&dsr_src[0], _src, 6);
      }

      ~BrnBroadcast()
      {}
  };

  class BufferedPacket
  {
    public:
     Packet *_p;
     struct timeval _send_time;

     BufferedPacket(Packet *p, int time_diff)
     {
       assert(p);
       _p=p;
       click_gettimeofday(&_send_time);
       _send_time.tv_sec += ( time_diff / 1000 );
       _send_time.tv_usec += ( ( time_diff % 1000 ) * 1000 );
       while( _send_time.tv_usec >= 1000000 )  //handle timeoverflow
       {
         _send_time.tv_usec -= 1000000;
         _send_time.tv_sec++;
       }
     }
     void check() const { assert(_p); }
  };

  typedef Vector<BufferedPacket> SendBuffer;

  //
  //methods
  //
  BrnFlooding();
  ~BrnFlooding();

  const char *class_name() const  { return "BrnFlooding"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/2"; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *timer);

 private:
  //
  //member
  //
  int _min_jitter,_jitter,_min_dist;
  long diff_in_ms(timeval t1, timeval t2);
  Timer _sendbuffer_timer;
  SendBuffer packet_queue;
  void queue_timer_hook();
  unsigned int get_min_jitter_in_queue();
  Vector<BrnBroadcast> bcast_queue;
  uint16_t bcast_id;

 public:
  int _debug;
};

CLICK_ENDDECLS
#endif
