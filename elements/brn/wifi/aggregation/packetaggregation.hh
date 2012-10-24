#ifndef PACKETAGGREGATION_HH
#define PACKETAGGREGATION_HH

#include <click/packet.hh>
#include <click/vector.hh>

CLICK_DECLS

class PacketAggregation
{
 public:

  PacketAggregation();
  ~PacketAggregation();

  Packet *amsdu_aggregation(Vector<Packet *> *packets, uint32_t max_size, Vector<Packet *> *used_packets = NULL);
  Packet *amsdu_deaggregation(Vector<Packet *> *packets, uint32_t max_size, Vector<Packet *> *used_packets = NULL);
  Packet *ampdu_aggregation(Vector<Packet *> *packets, uint32_t max_size, Vector<Packet *> *used_packets = NULL);

  int _debug;
};

CLICK_ENDDECLS
#endif
