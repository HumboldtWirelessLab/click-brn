#ifndef PACKETFRAGMENTATIONELEMENT_HH
#define PACKETFRAGMENTATIONELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

CLICK_SIZE_PACKED_STRUCTURE(
struct fragmention_header {,
  uint16_t packet_id;
  uint8_t  no_fragments;
  uint8_t  fragment_id;
});

#define PACKET_FRAGMENT_SIZE (_max_packet_size - (sizeof(struct fragmention_header) + sizeof(struct click_brn)))
#define MAX_FRAGMENTS 32


class PacketFragmentation : public BRNElement
{

 public:
  //
  //methods
  //
  PacketFragmentation();
  ~PacketFragmentation();

  const char *class_name() const  { return "PacketFragmentation"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "1/1-2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:

  uint16_t _max_packet_size;
  uint16_t _packet_id;

};

CLICK_ENDDECLS
#endif
