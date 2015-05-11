#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include <click/router.hh>

#include "wificonfig_sim.hh"

CLICK_DECLS

WifiConfigSim::WifiConfigSim(String dev_name, Router *router)
{
  device_name = dev_name;
  _router = router;
  _ifid = simclick_sim_command(_router->simnode(), SIMCLICK_IFID_FROM_NAME, dev_name.c_str());
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

int
WifiConfigSim::get_channel()
{
  int channel = simclick_sim_command(_router->simnode(), SIMCLICK_CHANGE_CHANNEL, _ifid, -1);

  return channel;
}

int
WifiConfigSim::set_channel(int channel)
{
  if (channel >= 0)
    simclick_sim_command(_router->simnode(), SIMCLICK_CHANGE_CHANNEL, _ifid, channel);

  return 0;
}

void
WifiConfigSim::get_cca(int *cs_threshold, int *rx_threshold, int *cp_threshold)
{
  int cca[4];
  memset(cca, 0, sizeof(cca));

  simclick_sim_command(_router->simnode(), SIMCLICK_CCA_OPERATION, &cca);

  *rx_threshold = cca[1];
  *cs_threshold = cca[2];
  *cp_threshold = cca[3];
}

void
WifiConfigSim::set_cca(int cs_threshold, int rx_threshold, int cp_threshold)
{
  int cca[4];
  cca[0] = 1; //command set
  cca[1] = cs_threshold;
  cca[2] = rx_threshold;
  cca[3] = cp_threshold;

  simclick_sim_command(_router->simnode(), SIMCLICK_CCA_OPERATION, &cca);
}

uint32_t
WifiConfigSim::set_backoff(uint32_t *queue_info)
{
  simclick_sim_command(_router->simnode(), SIMCLICK_WIFI_SET_BACKOFF, queue_info);

  return 0;
}

uint32_t
WifiConfigSim::get_backoff(uint32_t **queue_info)
{
  if ( *queue_info == NULL ) {
    int boq_info[2];
    boq_info[1] = boq_info[0] = 0;
    simclick_sim_command(_router->simnode(), SIMCLICK_WIFI_GET_BACKOFF, boq_info);
    int no_queues = boq_info[1];
    if (no_queues == 0) no_queues = 1;
    *queue_info = new uint32_t[2 + 3 * no_queues];
    memset(*queue_info, 0, (2 + 3 * no_queues)*sizeof(uint32_t));
    (*queue_info)[0] = no_queues;
  }

  (*queue_info)[1] = 0;

  simclick_sim_command(_router->simnode(), SIMCLICK_WIFI_GET_BACKOFF, *queue_info);

  return 0;
}


ELEMENT_PROVIDES(WifiConfigSimIwLib)
ELEMENT_REQUIRES(ns)
CLICK_ENDDECLS
