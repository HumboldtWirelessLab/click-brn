#ifndef PACKETSENDBUFFER_HH
#define PACKETSENDBUFFER_HH

#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#define DEFAULT_SENDPUFFER_TIMEOUT 10000

CLICK_DECLS

class PacketSendBuffer
{
  public:

  static long diff_in_ms(timeval t1, timeval t2)
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

  class BufferedPacket
  {
    public:
      Packet *_p;
      Timestamp _send_time_b;
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

      bool timeout()
      {
        struct timeval time_now;
        time_now = Timestamp::now().timeval();

        return ((_send_time.tv_sec < time_now.tv_sec) || (( _send_time.tv_sec == time_now.tv_sec) && ( _send_time.tv_usec < time_now.tv_usec)));
      }

      int diff_time(struct timeval curr_time) {
        return diff_in_ms( _send_time, curr_time );
      }
  };

  typedef Vector<BufferedPacket*> PacketQueue;

  public:

    PacketSendBuffer();
    ~PacketSendBuffer();

    void addPacket_s(Packet *p, int time_diff_s, int port);
    void addPacket_ms(Packet *p, int time_diff_ms, int port);
    void addPacket(Packet *p, int time_diff_ms) { addPacket_ms(p, time_diff_ms, 0); }

    int getTimeToNext();          //millisec to next Packet
    Packet *getNextPacket();
    BufferedPacket *getNextBufferedPacket();

  private:
    PacketQueue queue;

  public:
    int size() { return queue.size(); }
    PacketSendBuffer::BufferedPacket *get(int i) { return queue[i]; }
    void del(int i) {
      delete queue[i];
      queue.erase(queue.begin() + i);
    }

};

CLICK_ENDDECLS
#endif
