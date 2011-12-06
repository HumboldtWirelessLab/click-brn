#ifndef CLICK_SETRTS_HH
#define CLICK_SETRTS_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

/*
=c

SetRTS(Bool)

=s Wifi

Enable/disable RTS/CTS for a packet

=d

Enable/disable RTS/CTS for a packet

=h rts read/write
Enable/disable rts/cts for a packet.

=a ExtraEncap, ExtraDecap
*/

class PacketLossReason : public Element { public:

  PacketLossReason();
  ~PacketLossReason();

  const char *class_name() const		{ return "PacketLossReason"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  //int configure(Vector<String> &, ErrorHandler *);
  //Packet *simple_action(Packet *);

  void add_handlers();

int overall_fract(int depth);

typedef enum _PacketLossReason {
  INTERFERENCE, 
  CHANNEL_FADING,
  WIFI,
  NON_WIFI,
  WEAK_SIGNAL,
  SHADOWING,
  MULTIPATH_FADING,
  CO_CHANNEL,
  ADJACENT_CHANNEL,
  G_VS_B,
  NARROWBAND,
  BROADBAND,
  IN_RANGE,
  HIDDEN_NODE,
  NARROWBAND_COOPERATIVE,
  NARROWBAND_NON_COOPERATIVE,
  BROADBAND_COOPERATIVE,
 BROADBAND_NON_COOPERATIVE
} Packet_Loss_Reason;

};

CLICK_ENDDECLS
#endif
