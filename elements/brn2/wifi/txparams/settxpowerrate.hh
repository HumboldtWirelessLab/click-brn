#ifndef CLICK_SETTXPOWERRATE_HH
#define CLICK_SETTXPOWERRATE_HH
#include <click/element.hh>
#include <clicknet/ether.h>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/wifi/channelstats.hh"
#include "elements/brn2/wifi/brnavailablerates.hh"

CLICK_DECLS

struct power_rate_info {
  bool is_ht;
  bool ht40;
  bool sgi;
  uint8_t rate;
  uint8_t power;
};

class SetTXPowerRate : public BRNElement { public:

  class DstInfo {
    public:

      EtherAddress _eth;

      Vector<uint8_t> _non_ht_rates;
      int8_t _ht_rates[4][2];

      int _current_mode;
      int _current_index;
      int _successes;

      int _stepup;
      bool _wentup;

      uint8_t _max_power;
      uint8_t _power;

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

  typedef HashMap<EtherAddress, DstInfo> DstMap;
  typedef DstMap::const_iterator DstMapIter;

 private:

  DstInfo* getDstInfo(EtherAddress ea);

  BrnAvailableRates *_rtable;

  Vector<uint8_t> _non_ht_rates;
  int8_t _ht_rates[4][2];

  int _max_power;

  ChannelStats *_cst;

  DstMap _dst_map;

};

CLICK_ENDDECLS
#endif
