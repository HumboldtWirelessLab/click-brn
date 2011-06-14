#ifndef BRN_AVAILABLERATES_HH
#define BRN_AVAILABLERATES_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/wifi/brnwifi.hh"

CLICK_DECLS

/*
=c

BrnAvailableRates()

=s Wifi, Wireless Station, Wireless AccessPoint

Tracks bit-rate capabilities of other stations.

=d

Tracks a list of bitrates other stations are capable of.

=h insert write-only
Inserts an ethernet address and a list of bitrates to the database.

=h remove write-only
Removes an ethernet address from the database.

=h rates read-only
Shows the entries in the database.

=a BeaconScanner
 */

#define RATE_HT_NONE       -1
#define RATE_HT20           0
#define RATE_HT20_SGI       1
#define RATE_HT40           2
#define RATE_HT40_SGI       3
#define RATE_HT20_BLOCK     4
#define RATE_HT20_SGI_BLOCK 5
#define RATE_HT40_BLOCK     6
#define RATE_HT40_SGI_BLOCK 7

class BrnAvailableRates : public BRNElement { public:

  BrnAvailableRates();
  ~BrnAvailableRates();

  const char *class_name() const		{ return "BrnAvailableRates"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);
  void *cast(const char *n);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();
  void take_state(Element *e, ErrorHandler *);

#define INVALID_RIDX 255

  class MCS {
    public:
      int _data_rate; //datarate * 10

      uint8_t _rate;

      bool _is_ht;

      bool _sgi;
      bool _ht40;

      uint8_t _ridx;

      bool _is_rate_block;
      uint8_t _start_ridx;
      uint8_t _end_ridx;

      MCS(): _data_rate(10), _rate(2), _is_ht(false), _sgi(false), _ht40(false),
             _ridx(INVALID_RIDX), _is_rate_block(false), _start_ridx(INVALID_RIDX), _end_ridx(INVALID_RIDX) {
        click_chatter("MCS constructor");
      }

      MCS(uint8_t rate) {
        _rate = rate;
        _data_rate = _rate * 5;
      }

      MCS(uint8_t ridx, bool ht40, bool sgi ) {
        _rate = 0;
        _ridx = ridx;
        _ht40 = ht40;
        _sgi = sgi;
        _data_rate = BrnWifi::getMCSRate(ridx, ht40?1:0, sgi?1:0);
      }

       MCS(uint8_t start_ridx, int8_t /*end_ridx*/, bool ht40, bool sgi ) {
        _rate = 0;
        _ht40 = ht40;
        _sgi = sgi;
        _data_rate = BrnWifi::getMCSRate(start_ridx, ht40?1:0, sgi?1:0);
      }
  };

  class DstInfo {
  public:
    EtherAddress _eth;
    Vector<MCS> _rates;
    DstInfo() {
      memset(this, 0, sizeof(*this));
    }

    DstInfo(EtherAddress eth) {
      memset(this, 0, sizeof(*this));
      _eth = eth;
    }

    ~DstInfo() {
      _rates.clear();
    }
  };

  typedef HashMap<EtherAddress, DstInfo> RTable;
  typedef RTable::const_iterator RIter;

  Vector<MCS> lookup(EtherAddress eth);
  int insert(EtherAddress eth, Vector<MCS>);

  EtherAddress _bcast;

  int parse_and_insert(String s, ErrorHandler *errh);

  RTable _rtable;
  Vector<MCS> _default_rates;

};

CLICK_ENDDECLS
#endif
