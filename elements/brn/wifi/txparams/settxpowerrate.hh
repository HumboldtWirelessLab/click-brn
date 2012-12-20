#ifndef CLICK_SETTXPOWERRATE_HH
#define CLICK_SETTXPOWERRATE_HH
#include <click/element.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/channelstats.hh"
#include "elements/brn/wifi/brnavailablerates.hh"

#include "elements/brn/wifi/txparams/neighbourrateinfo.hh"
#include "rateselection/rateselection.hh"

CLICK_DECLS

/*
 * Input:
 * 0 : packets to send
 * 1 : txfeedback packets
 * 2 : received packets
 *
 * Output:
 * 0 : packets to send
 * 1 : txfeedback packets
 * 2 : received packets
 */

class SetTXPowerRate : public BRNElement {

public:

  SetTXPowerRate();
  ~SetTXPowerRate();

  const char *class_name() const  { return "SetTXPowerRate"; }
  const char *port_count() const  { return "2-3/2-3"; }

  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  int initialize(ErrorHandler *);
  void run_timer(Timer *);

  void push (int, Packet *);
  Packet *pull(int);

  Packet *handle_packet( int port, Packet *packet );

  void add_handlers();

  String getInfo();

private:

  NeighbourRateInfo* getDstInfo(EtherAddress ea);

  BrnAvailableRates *_rtable;
  RateSelection *_rate_selection;

  int _max_power;

  ChannelStats *_cst;

  NeighborTable _neighbors;

  Timer _timer;

  unsigned _packet_size_threshold;

  int _offset;

};

CLICK_ENDDECLS
#endif
