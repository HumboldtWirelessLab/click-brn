#ifndef CLICK_RXPOWERSTATS_HH
#define CLICK_RXPOWERSTATS_HH

#include <click/element.hh>
#include <click/string.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/packet.hh>

#define STATE_UNKNOWN  0
#define STATE_OK       1
#define STATE_CRC      2
#define STATE_PHY      3

CLICK_DECLS

/**
 TODO: testing
       reset
*/

class RXPowerStats : public Element {

  class RxInfo {
   public:
    uint16_t _size;
    uint8_t  _power;
    uint8_t  _silence;
    uint8_t  _state;

    Timestamp _now;

    RxInfo( uint16_t size, uint8_t  power, uint8_t silence, uint8_t  state )
    {
      _size = size;
      _power = power;
      _silence = silence;
      _state = state;
      _now = Timestamp::now();
    }
  };

  typedef Vector<RxInfo> RxInfoList;
  typedef RxInfoList::const_iterator RxInfoListIter;

  class RxRateInfo {
   public:
    uint8_t _rate;
    RxInfoList _rxinfo;

    RxRateInfo(uint8_t rate) {
      _rate = rate;
      _rxinfo.clear();
    }
  };

  typedef Vector<RxRateInfo> RxRateInfoList;
  typedef RxRateInfoList::const_iterator RxRateInfoListIter;

  class RxSourceInfo {
   public:
    EtherAddress _src;
    RxRateInfoList _rxrateinfo; 

    RxSourceInfo( EtherAddress src)
    {
      _src = src;
      _rxrateinfo.clear();
    }
  };

  typedef Vector<RxSourceInfo> RxSourceInfoList;
  typedef RxSourceInfoList::const_iterator RxSourceInfoListIter;

  public:

    RXPowerStats() {};
    ~RXPowerStats() {};

    const char *class_name() const { return "RXPowerStats"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    void add_handlers();

    void push(int, Packet *p);
    void reset();
    String stats();
    bool _debug;

    RXPowerStats::RxRateInfoList *getRxRateInfoList(EtherAddress src);
    RXPowerStats::RxRateInfo *getRxRateInfo(RXPowerStats::RxRateInfoList *rxinfo, uint8_t rate);
    RxSourceInfoList _rxinfolist;
};

CLICK_ENDDECLS
#endif
