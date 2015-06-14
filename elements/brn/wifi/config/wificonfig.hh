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
    virtual ~WifiConfig();

    virtual int set_txpower(int txpower) = 0;
    virtual int get_txpower() = 0;
    virtual int get_max_txpower() = 0;

    virtual int get_rates(Vector<MCS> &rates) = 0;

    virtual int get_channel() = 0;
    virtual int set_channel(int channel) = 0;

    virtual void get_cca(int *cs_threshold, int *rx_threshold, int *cp_threshold) = 0;
    virtual void set_cca(int cs_threshold, int rx_threshold, int cp_threshold) = 0;

    virtual uint32_t set_backoff(uint32_t *_queue_info) = 0;
    virtual uint32_t get_backoff(uint32_t **_queue_info) = 0;
};

CLICK_ENDDECLS
#endif
