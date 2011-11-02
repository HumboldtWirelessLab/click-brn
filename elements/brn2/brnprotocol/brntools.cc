#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "brnprotocol.hh"

CLICK_DECLS

inline unsigned long
BRNTools::diff_in_ms(timeval t1, timeval t2)
{
  unsigned long s, us, ms;

  while (t1.tv_usec < t2.tv_usec) {
    t1.tv_usec += 1000000;
    t1.tv_sec -= 1;
  }

  assert(t1.tv_sec >= t2.tv_sec);

  s = t1.tv_sec - t2.tv_sec;
  assert(s < ((unsigned long)(1<<31))/1000);

  us = t1.tv_usec - t2.tv_usec;
  ms = s * 1000L + us / 1000L;

  return ms;
}

inline int
BRNTools::getBitposition(uint32_t x)
{
  int pos = 0;
  while(x >>= 1) ++pos;
  return pos;
}

CLICK_ENDDECLS
PROVIDES_ELEMENT(BRNTools)

