#ifndef CLICK_RXPOWERSTATS_HH
#define CLICK_RXPOWERSTATS_HH

#include <click/element.hh>
#include <click/string.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/packet.hh>

CLICK_DECLS

class RXPowerStats : public Element {

  class RxInfo {
   public:
    uint16_t _size;
    uint8_t  _power;
    uint8_t  _state;

    RxInfo( uint16_t size, uint8_t  power, uint8_t  state )
    {

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

    RxSourceInfoList _rxinfolist;
};

CLICK_ENDDECLS
#endif
