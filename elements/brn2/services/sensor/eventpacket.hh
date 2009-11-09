#ifndef EVENTPACKET_HH
#define EVENTPACKET_HH

CLICK_DECLS

struct event_header {
  uint8_t  _src[6];
  uint16_t _id;
};

CLICK_ENDDECLS
#endif
