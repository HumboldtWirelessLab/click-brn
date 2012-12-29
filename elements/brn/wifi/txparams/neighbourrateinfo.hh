#ifndef NEIGHBOURRATEINFO_HH
#define NEIGHBOURRATEINFO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn/wifi/brnavailablerates.hh"

CLICK_DECLS

class NeighbourRateInfo {
  public:

    EtherAddress _eth;

    Vector<MCS> _rates;

    uint8_t _max_power;
    uint8_t _power;

    void *_rs_data;


    NeighbourRateInfo();
    ~NeighbourRateInfo();

    NeighbourRateInfo(EtherAddress eth, Vector<MCS> rates, uint8_t max_power) {
      _eth       = eth;
      _rates     = rates;
      _max_power = max_power;
      _power     = max_power;
      _rs_data   = NULL;
    }

    int rate_index(MCS rate) {
      int ndx = 0;
      for (int x = 0; x < _rates.size(); x++) {
        if (rate.equals(_rates[x])) {
          ndx = x;
          break;
        }
      }
      return (ndx == _rates.size()) ? -1 : ndx;
    }

    int rate_index(uint32_t rate) {
      int ndx = 0;
      for (int x = 0; x < _rates.size(); x++) {
        if ((!_rates[x]._is_ht) && (_rates[x]._rate == rate)) {
          ndx = x;
          break;
        }
      }
      return (ndx == _rates.size()) ? -1 : ndx;
    }

    MCS pick_rate(uint32_t index) {
      if (_rates.size() == 0) {
        return MCS(2);
      }

      if (index > 0 && index < (uint32_t)_rates.size()) {
        return _rates[index];
      }
      return _rates[0];
    }

};

typedef HashMap<EtherAddress, NeighbourRateInfo*> NeighborTable;
typedef NeighborTable::const_iterator NIter;

CLICK_ENDDECLS
#endif
