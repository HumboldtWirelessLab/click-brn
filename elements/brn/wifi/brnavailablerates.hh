#ifndef BRN_AVAILABLERATES_HH
#define BRN_AVAILABLERATES_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/brnwifi.hh"

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

#define INVALID_RIDX 255

/* TODO: use int32_t for MCS, for _data_rate use lookup table */

class MCS {
  public:
    uint32_t _data_rate; //datarate * 10 (e.g. 5.5MBit/s -> 11)

    uint8_t _rate;       //used for click_wifi
                         // for non_ht: datarate * 2 (e.g. 1MBit/s -> 2)
                         // for ht: see BrnWifi::fromMCS()
    bool _is_ht;

    uint8_t _sgi;
    uint8_t _ht40;

    uint8_t _ridx;

    uint8_t _packed8;
    uint16_t _packed16;

    MCS() : _data_rate(10), _rate(2), _is_ht(false), _sgi(0), _ht40(0), _ridx(INVALID_RIDX) {
    }

    MCS(uint8_t rate) : _data_rate(10), _rate(2), _is_ht(false), _sgi(0), _ht40(0), _ridx(INVALID_RIDX) {
      _rate = rate;
      _data_rate = _rate * 5;
    }

    MCS(uint8_t ridx, uint8_t ht40, uint8_t sgi) : _data_rate(10), _rate(2), _is_ht(false), _sgi(0), _ht40(0), _ridx(INVALID_RIDX) {
      set(ridx, ht40, sgi);
    }

    MCS(click_wifi_extra *ceh, int8_t idx) : _data_rate(10), _rate(2), _is_ht(false), _sgi(0), _ht40(0), _ridx(INVALID_RIDX) {
      getWifiRate(ceh,idx);
    }

    inline void set(uint8_t ridx, uint8_t ht40, uint8_t sgi) {
      _is_ht = true;
      _ridx = ridx;
      _ht40 = ht40;
      _sgi = sgi;
      _data_rate = BrnWifi::getMCSRate(ridx, _ht40, _sgi);
      BrnWifi::fromMCS(_ridx, _ht40, _sgi, &_rate);
    }

    inline void set(uint8_t rate) {
      _is_ht = false;
      _ridx = 0;
      _ht40 = 0;
      _sgi = 0;
      _rate = rate;
      _data_rate = _rate * 5;
    }

    /* Set rate to header */
    inline void setWifiRate(struct click_wifi_extra *ceh, int index) {
      BrnWifi::setMCS(ceh, index, _is_ht?1:0);

      switch (index) {
        case 0: ceh->rate = _rate; break;
        case 1: ceh->rate1 = _rate; break;
        case 2: ceh->rate2 = _rate; break;
        case 3: ceh->rate3 = _rate; break;
        default: click_chatter("Invalid index");
      }
    }

    inline void getWifiRate(struct click_wifi_extra *ceh, int index) {
      switch (index) {
        case 0: _rate = ceh->rate; break;
        case 1: _rate = ceh->rate1; break;
        case 2: _rate = ceh->rate2; break;
        case 3: _rate = ceh->rate3; break;
        default: return;
      }

      if ((_is_ht = (BrnWifi::getMCS(ceh, index))) == 1 ) {
        BrnWifi::toMCS(&_ridx, &_ht40, &_sgi, _rate);
        _data_rate = BrnWifi::getMCSRate(_ridx, _ht40, _sgi);
      } else {
        _data_rate = _rate * 5;
      }
    }

    inline bool equals(MCS mcs) {
      return ((mcs._is_ht == _is_ht) && (mcs._rate == _rate));
    }

    uint32_t get_rate() {
      return _data_rate;
    }

/* packet_mcs
---------------------------------------------------------------------
|   0   |   1   |   2   |   3   |   4   |   5   |     6    |  7     |
|                  MCS INDEX            |   BW  | GUARDINT | IS N   |
---------------------------------------------------------------------
*/
    uint8_t get_packed_8() {
      if ( _is_ht ) {
        uint8_t _small_ht = ( _ht40 > 0 )?1:0;
        return (uint8_t)128 | _ridx | (_small_ht << 5) | (_sgi << 6);
      } else {
        return _rate;
      }
    }

    void set_packed_8(uint8_t packet_mcs) {
      if ( (packet_mcs & 128) == 0 ) {
        set(packet_mcs & 127); //clear highest bit and set rate (non_ht)
      } else {
        set(packet_mcs & 31, (packet_mcs >> 5) & 1, (packet_mcs >> 6) & 1);
      }
    }

    uint16_t get_packed_16() {
      if ( _is_ht ) {
        return (uint16_t)32768 | (uint16_t)_rate;
      } else {
        return _rate;
      }
    }

    void set_packed_16(uint16_t packet_mcs) {
      if ( (packet_mcs & (uint16_t)32768) == 0 ) {
        set(packet_mcs & (uint16_t)32767); //clear highest bit and set rate (non_ht)
      } else {
        set(packet_mcs & 31, (packet_mcs >> 5) & 3, (packet_mcs >> 7) & 1);
      }
    }

    String to_string() {
      StringAccum sa;
      sa << _data_rate / 10;
      if ( ( _data_rate % 10 ) != 0 ) {
        sa << "." << ( _data_rate % 10 );
      }
      return sa.take_string();
    }

    inline bool operator==(MCS mcs) {
       return equals(mcs);
    }

};

inline unsigned hashcode(MCS mcs){
  return (unsigned)mcs.get_packed_16();
}

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

  class DstInfo {
   public:
    Timestamp _settime;
    EtherAddress _eth;
    Vector<MCS> _rates;

    DstInfo(): _settime(Timestamp::now()), _eth() {
      _rates.clear();
    }

    DstInfo(EtherAddress eth): _settime(Timestamp::now()), _eth(eth)  {
      _rates.clear();
    }

    ~DstInfo() {
      _rates.clear();
    }

  };

  typedef HashMap<EtherAddress, DstInfo> RTable;
  typedef RTable::const_iterator RIter;

  Vector<MCS> lookup(EtherAddress eth);
  Timestamp get_timestamp(EtherAddress eth);

  int insert(EtherAddress eth, Vector<MCS>);
  bool includes_node(EtherAddress eth) { return _rtable.findp(eth) != NULL; };

  int parse_and_insert(String s, ErrorHandler *errh);

  RTable _rtable;
  Vector<MCS> _default_rates;
  Timestamp _settime;

  //HashMap<MCS> _default_rates_map;

};

CLICK_ENDDECLS
#endif
