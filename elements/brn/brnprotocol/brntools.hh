#ifndef CLICK_BRNTOOLS_HH
#define CLICK_BRNTOOLS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

class BRNTools {
  public:

    inline unsigned long diff_in_ms(timeval t1, timeval t2);
    inline int getBitposition(uint32_t x);

#define BITPOSITION(x) BRNTools::getBitposition(x)

};

CLICK_ENDDECLS
#endif
