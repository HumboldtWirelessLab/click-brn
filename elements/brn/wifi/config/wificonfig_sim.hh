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

    Router *_router;

    int set_txpower(int txpower);
    int get_txpower();
    int get_max_txpower();
    int get_rates(Vector<MCS> &rates);
};

CLICK_ENDDECLS
#endif
