#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include <click/router.hh>

#include "iwlib.h"
#include "wificonfig_sim.hh"

CLICK_DECLS

WifiConfigSim::WifiConfigSim(String dev_name, Router *router)
{
  device_name = dev_name;
  _router = router;
}

WifiConfigSim::~WifiConfigSim()
{
}

int
WifiConfigSim::get_txpower()
{
  int txpower;
  simclick_sim_command(_router->simnode(), SIMCLICK_WIFI_GET_TXPOWER, &txpower);

  return txpower;
}

int
WifiConfigSim::set_txpower(int power)
{
  simclick_sim_command(_router->simnode(), SIMCLICK_WIFI_SET_TXPOWER, &power);

  return power;
}

int
WifiConfigSim::get_max_txpower()
{
  int save_power = get_txpower();

  set_txpower(100);
  int max_power = get_txpower();

  set_txpower(save_power);

  return max_power;
}

int
WifiConfigSim::get_rates(Vector<MCS> &rates)
{
  int rate_array[21];

  rates.clear();

  rate_array[0] = 20;

  simclick_sim_command(_router->simnode(), SIMCLICK_WIFI_GET_RATES, &rate_array);

  if ( rate_array[0] <= 20 ) {
    for (int r = 1; r <= rate_array[0]; r++) {
      click_chatter("Rate %d: %d",r,rate_array[r]);
      rates.push_back(MCS((uint8_t)rate_array[r]));
    }
  }

  return 0;
}

ELEMENT_PROVIDES(WifiConfigSimIwLib)
ELEMENT_REQUIRES(ns)
CLICK_ENDDECLS
