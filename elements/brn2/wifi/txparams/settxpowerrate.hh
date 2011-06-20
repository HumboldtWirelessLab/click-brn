#ifndef CLICK_SETTXPOWERRATE_HH
#define CLICK_SETTXPOWERRATE_HH
#include <click/element.hh>
#include <clicknet/ether.h>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/wifi/channelstats.hh"
#include "elements/brn2/wifi/brnavailablerates.hh"
#include "rateselection/rateselection.hh"

CLICK_DECLS

struct power_rate_info {
  bool is_ht;
  bool ht40;
  bool sgi;
  uint8_t rate;
  uint8_t power;
};

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

class SetTXPowerRate : public BRNElement { public:

  class DstInfo {
    public:

      EtherAddress _eth;

      Vector<uint8_t> _non_ht_rates;
      int8_t _ht_rates[4][2];

      uint8_t _max_power;
      uint8_t _power;

      void *_rs_data;

      DstInfo() {
      }

      ~DstInfo() {}

      DstInfo(EtherAddress eth, Vector<uint8_t> non_ht_rates, int8_t *ht_rates, uint8_t max_power ) {
        _eth = eth;
        _non_ht_rates = non_ht_rates;
        memcpy(_ht_rates, ht_rates, sizeof(_ht_rates));

        _max_power = max_power;
        _power = max_power;
      }
  };

  SetTXPowerRate();
  ~SetTXPowerRate();

  const char *class_name() const  { return "SetTXPowerRate"; }
  const char *port_count() const  { return "3/3"; }

  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  void push (int, Packet *);
  Packet *pull(int);

  Packet *handle_packet( int port, Packet *packet );

  void add_handlers();

  String getInfo();

  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

 private:

  DstInfo* getDstInfo(EtherAddress ea);

  BrnAvailableRates *_rtable;
  RateSelection *_rate_selection;

  Vector<uint8_t> _non_ht_rates;
  int8_t _ht_rates[4][2];

  int _max_power;

  ChannelStats *_cst;

  NeighborTable _neighbors;

};

CLICK_ENDDECLS
#endif
