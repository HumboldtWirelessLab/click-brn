#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "bo_target_packetloss.hh"
#include "tos2qm_data.hh"

CLICK_DECLS

BoTargetPacketloss::BoTargetPacketloss()
  : _cst(NULL),
    _target_packetloss(0)
{
  BRNElement::init();
  _default_strategy = BACKOFF_STRATEGY_TARGET_PACKETLOSS;
}


void * BoTargetPacketloss::cast(const char *name)
{
  if (strcmp(name, "BoTargetPacketloss") == 0)
    return (BoTargetPacketloss *) this;

  return BackoffScheme::cast(name);
}

int BoTargetPacketloss::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
      "TARGETPL", cpkP, cpInteger, &_target_packetloss,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

void BoTargetPacketloss::add_handlers()
{
}

int BoTargetPacketloss::get_cwmin(Packet *p, uint8_t tos)
{
  (void) tos;

  int32_t number_of_neighbours = 1;
  int32_t index_search_rate = -1;
  int32_t index_search_msdu_size = -1;
  int32_t index_search_likelihood_collision = -1;
  int32_t index_no_neighbours = -1;
  uint32_t backoff_window_size = 0;

  // Get Destination Address from the current packet
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  //Get Number of Neighbours from the Channelstats-Element (_cst)
  struct airtime_stats *as = _cst->get_latest_stats(); //get airtime statisics
  number_of_neighbours = as->no_sources;
  uint32_t busy = as->hw_busy;

  BRN_DEBUG("BoTargetPL.get_cwmin():\n");
  BRN_DEBUG("    busy: %d\n", busy);

  BRN_DEBUG("    rate: %d\n", ceh->rate);
  BRN_DEBUG("    msdu size: %d\n", p->length());
  BRN_DEBUG("    target pkt loss: %d\n", _target_packetloss);
  BRN_DEBUG("    no. of nbs: %d\n", number_of_neighbours);

  index_search_rate = Tos2QmData::find_closest_rate_index(ceh->rate);
  index_search_msdu_size = Tos2QmData::find_closest_size_index(p->length() - 38); // msdu size = 1538, tos2qm_data checks for 1500 ...
  index_search_likelihood_collision = Tos2QmData::find_closest_per_index(_target_packetloss);
  index_no_neighbours = Tos2QmData::find_closest_no_neighbour_index(number_of_neighbours);

  BRN_DEBUG("");
  BRN_DEBUG("    idx rate: %d\n", index_search_rate);
  BRN_DEBUG("    idx msdu_size: %d\n",index_search_msdu_size);
  BRN_DEBUG("    idx likelihood: %d\n", index_search_likelihood_collision);
  BRN_DEBUG("    idx nbs: %d\n", index_no_neighbours);


  // Tests what is known
  backoff_window_size = _backoff_matrix_tmt_backoff_collision_probability[index_no_neighbours][index_search_likelihood_collision];

  BRN_DEBUG("    backoffwin: %d\n\n", backoff_window_size);

  return backoff_window_size;
}


void BoTargetPacketloss::handle_feedback(uint8_t retries)
{
  (void) retries;
}

CLICK_ENDDECLS

EXPORT_ELEMENT(BoTargetPacketloss)
ELEMENT_MT_SAFE(BoTargetPacketloss)
