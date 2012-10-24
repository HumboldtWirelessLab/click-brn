#ifndef EVENTPACKET_HH
#define EVENTPACKET_HH

CLICK_DECLS

#define DEFAULT_EVENT_MAX_HOP_COUNT 100

struct event_header {
  uint8_t  _src[6];
  uint16_t _id;
} CLICK_SIZE_PACKED_ATTRIBUTE;

CLICK_ENDDECLS
#endif
