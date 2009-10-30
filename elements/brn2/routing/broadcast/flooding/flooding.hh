#ifndef FLOODING_HH
#define FLOODING_HH

#include "elements/brn2/brnprotocol/brnprotocol.hh"

CLICK_DECLS

struct click_brn_bcast {
  uint16_t      bcast_id;
  hwaddr        dsr_dst;
  hwaddr        dsr_src;
};

struct click_flooding_header {
  uint16_t      bcast_id;
  hwaddr        _src;
};

CLICK_ENDDECLS

#endif
