#ifndef WIFI_CONFIG_SIM_HH
#define WIFI_CONFIG_SIM_HH

#include <click/router.hh>

#include "wificonfig.hh"

CLICK_DECLS

class WifiConfigSim: public WifiConfig
{
  public:

    WifiConfigSim(String dev_name, Router *router);
    ~WifiConfigSim();

    int set_txpower(int txpower);
    int get_txpower();
    int get_max_txpower();

    int get_rates(Vector<MCS> &rates);

    int get_channel();
    int set_channel(int channel);

  private:

    int _ifid;
    Router *_router;
};

CLICK_ENDDECLS
#endif
