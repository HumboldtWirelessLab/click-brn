#ifndef EVENTPACKET_HH
#define EVENTPACKET_HH

CLICK_DECLS

#define DEFAULT_EVENT_MAX_HOP_COUNT 100

struct event_header {
  uint8_t  _src[6];
  uint16_t _id;
} CLICK_SIZE_PACKED_ATTRIBUTE;


#define EVENT_DATA_TYPE_TIMESTAMP 1
#define EVENT_DATA_TYPE_STRING    2
#define EVENT_DATA_TYPE_INT       3


#define EVENT_DATA_MARKER 0x05060509 /*VIDI*/

struct event_data {
  uint8_t _event_data_type;
  uint8_t _event_data_size;
} CLICK_SIZE_PACKED_ATTRIBUTE;

CLICK_ENDDECLS
#endif
