#ifndef PACKETSENDBUFFER_HH
#define PACKETSENDBUFFER_HH

#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#define DEFAULT_SENDPUFFER_TIMEOUT 10000

CLICK_DECLS

class PacketSendBuffer
{
  public:

  class BufferedPacket
  {
    public:
      Packet *_p;
      struct timeval _send_time;
      int _port;

    public:

      BufferedPacket(Packet *p, int time_diff, int port)
      {
        assert(p);
        _p=p;
        _port = port;
        _send_time = Timestamp::now().timeval();
        _send_time.tv_sec += ( time_diff / 1000 );
        _send_time.tv_usec += ( ( time_diff % 1000 ) * 1000 );
        while( _send_time.tv_usec >= 1000000 )  //handle timeoverflow
        {
          _send_time.tv_usec -= 1000000;
          _send_time.tv_sec++;
        }
      }

      BufferedPacket(Packet *p, int time_diff)
      {
        BufferedPacket(p, time_diff, 0);
      }

      void check() const { assert(_p); }
  };

  typedef Vector<BufferedPacket*> PacketQueue;

  public:

    PacketSendBuffer();
    ~PacketSendBuffer();

    void addPacket_s(Packet *p, int time_diff_s, int port);
    void addPacket_ms(Packet *p, int time_diff_ms, int port);

    int getTimeToNext();          //millisec to next Packet
    BufferedPacket *getNextPacket();

  private:
    PacketQueue queue;

};

CLICK_ENDDECLS
#endif
