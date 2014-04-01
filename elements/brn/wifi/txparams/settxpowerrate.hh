#ifndef CLICK_SETTXPOWERRATE_HH
#define CLICK_SETTXPOWERRATE_HH
#include <click/element.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/brnavailablerates.hh"

#include "elements/brn/wifi/rxinfo/channelstats/channelstats.hh"
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
  const char *port_count() const  { return "1-3/1-3"; }

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
  RateSelection *get_rateselection(uint32_t rateselection_strategy);
  int _rate_selection_strategy;

  SchemeList _scheme_list;

  ChannelStats *_cst;

  NeighborTable _neighbors;
  uint32_t _max_power;

  Timer _timer;

  int _offset;
  bool _has_wifi_header;

};

CLICK_ENDDECLS
#endif
