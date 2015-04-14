#ifndef WIFI_CONFIG_HH
#define WIFI_CONFIG_HH

#include "elements/brn/wifi/brnavailablerates.hh"

CLICK_DECLS

class WifiConfig
{
  public:
    String device_name;

    WifiConfig();
    WifiConfig(String dev_name);
    ~WifiConfig();

    virtual int set_txpower(int txpower) = 0;
    virtual int get_txpower() = 0;
    virtual int get_max_txpower() = 0;
    virtual int get_rates(Vector<MCS> &rates) = 0;
};

CLICK_ENDDECLS
#endif
