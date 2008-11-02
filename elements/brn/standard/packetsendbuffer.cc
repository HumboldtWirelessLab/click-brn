#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "packetsendbuffer.hh"

CLICK_DECLS
PacketSendBuffer::PacketSendBuffer()
{
}

PacketSendBuffer::~PacketSendBuffer()
{
}

void
PacketSendBuffer::addPacket_s(Packet *p, int time_diff_s, int port)
{
  addPacket_ms(p,1000 * time_diff_s, port);
}

void
PacketSendBuffer::addPacket_ms(Packet *p, int time_diff_ms, int port)
{
  queue.push_back( new BufferedPacket(p,time_diff_ms, port) );
}

static long
diff_in_ms(timeval t1, timeval t2)
{
  long s, us, ms;

  while (t1.tv_usec < t2.tv_usec) {
    t1.tv_usec += 1000000;
    t1.tv_sec -= 1;
  }

  s = t1.tv_sec - t2.tv_sec;

  us = t1.tv_usec - t2.tv_usec;
  ms = s * 1000L + us / 1000L;

  return ms;
}

int
PacketSendBuffer::getTimeToNext()
{
  struct timeval _next_send;
  struct timeval _time_now;
  long next_jitter;

  if ( queue.size() == 0 ) return( -1 );
  else
  {
    _next_send.tv_sec = queue[0]->_send_time.tv_sec;
    _next_send.tv_usec = queue[0]->_send_time.tv_usec;

    for ( int i = 1; i < queue.size(); i++ )
    {
      if ( ( _next_send.tv_sec > queue[i]->_send_time.tv_sec ) ||
             ( ( _next_send.tv_sec == queue[i]->_send_time.tv_sec ) && (  _next_send.tv_usec > queue[i]->_send_time.tv_usec ) ) )
      {
        _next_send.tv_sec = queue[i]->_send_time.tv_sec;
        _next_send.tv_usec = queue[i]->_send_time.tv_usec;
      }
    }

    _time_now = Timestamp::now().timeval();

    next_jitter = diff_in_ms(_next_send, _time_now);

    if ( next_jitter <= 1 ) return( 1 );
    else return( next_jitter );
  }
}

PacketSendBuffer::BufferedPacket*
PacketSendBuffer::getNextPacket()
{
  BufferedPacket *next;
  int index;

  if ( queue.size() == 0 ) return NULL;
  else
  {
    index = 0;
    next = queue[0];

    for ( int i = 1; i < queue.size(); i++ )
    {
      if ( ( next->_send_time.tv_sec > queue[i]->_send_time.tv_sec ) ||
             ( ( next->_send_time.tv_sec == queue[i]->_send_time.tv_sec ) && (  next->_send_time.tv_usec > queue[i]->_send_time.tv_usec ) ) )
      {
        index = i;
        next = queue[i];
      }
    }
  }

  queue.erase(queue.begin() + index);
  return next;
}

#include <click/vector.cc>
template class Vector<PacketSendBuffer::BufferedPacket*>;

CLICK_ENDDECLS

ELEMENT_PROVIDES(PacketSendBuffer)
