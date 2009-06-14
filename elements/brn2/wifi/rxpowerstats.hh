#ifndef CLICK_RXPOWERSTATS_HH
#define CLICK_RXPOWERSTATS_HH

#include <click/etheraddress.hh>
#include <clicknet/ether.h>

CLICK_DECLS

class RXPowerStats
{

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
  class DstInfo
  {

   public:

    class RateInfo
    {
     public:
      uint8_t _rate;
      uint8_t _power;

      uint32_t _packets;
      uint32_t _retries;
      uint32_t _success;

      Timestamp _last_choose;
      Timestamp _last_success;

      RateInfo(uint8_t rate, uint8_t power)
      {
        _rate = rate;
        _power = power;
        _packets = _retries = _success = 0;
        _last_choose = _last_success = Timestamp::now();
      }

      ~RateInfo()
      {
      }

      void send()
      {
        _packets++;
        _last_choose = Timestamp::now();
      }

      void receive(bool succ, uint32_t retries)
      {
        if (succ) {
          _retries += retries;  //TODO: think about this; retries for not successful tx-packet maybe make no sense ??
          _success++;
          _last_success = Timestamp::now();
        }
      }

      void reset(int factor)
      {
        if ( factor != 0 ) {
          _packets /= factor;
          _success /= factor;
          _retries /= factor;
        }
      }

      int getProbability()
      {
        if ( _packets == 0 ) return 100;
        return ( _success * 100 / _packets );
      }

      int getRetries()
      {
        if ( _success == 0 ) return 100;
        return ( _retries * 100 / _success );
      }
    }; // End of class RateInfo


    EtherAddress _dst;
    Vector<RateInfo *> _ratelist;

    DstInfo(unsigned char *ea)
    {
      _dst = EtherAddress(ea);
    }

    ~DstInfo()
    {
      _ratelist.clear();
    }

    RateInfo *getRateInfo(int rate, int power)
    {
      for(int i = 0; i < _ratelist.size(); i++)
        if (( _ratelist[i]->_rate == rate ) && ( _ratelist[i]->_power == power ))
          return _ratelist[i];
      return NULL;
    }

    void txfeedback(bool succ, int rate, int power, int retries)
    {
      RateInfo *rateinfo = getRateInfo(rate,power);
      if ( rateinfo != NULL ) rateinfo->receive(succ,retries);
    }

    void send(int rate,int power)
    {
      RateInfo *rateinfo = getRateInfo(rate,power);
      if ( rateinfo == NULL ) {
        rateinfo = new RateInfo(rate,power);
        _ratelist.push_back(rateinfo);
      }

      rateinfo->send();
    }

    int getMaxRate()
    {
     int maxrate = 0;
     //int power = 0; //TODO: include power in some way

      for(int i = 0; i < _ratelist.size(); i++)
      {
        if (( _ratelist[i]->_rate > maxrate ))
          maxrate = _ratelist[i]->_rate;
      }

      return maxrate;
    }

    int getProbability(int rate, int power)
    {
      RateInfo *rateinfo = getRateInfo(rate,power);
      if ( rateinfo == NULL ) return 100;
      return rateinfo->getProbability();
    }

    int getRetries(int rate, int power)
    {
      RateInfo *rateinfo = getRateInfo(rate,power);
      if ( rateinfo == NULL ) return 0;
      return rateinfo->getRetries();
    }

  }; // End of class DstInfo

  Vector<DstInfo *> _dstlist;

  public:

    RXPowerStats() {};
    ~RXPowerStats() {};

    DstInfo *getDst(EtherAddress *ea);
    DstInfo *getDst(EtherAddress ea);
    DstInfo *getDst(unsigned char *ea);

    DstInfo *newDst(unsigned char *ea);

    int maxpower;

};

CLICK_ENDDECLS
#endif
