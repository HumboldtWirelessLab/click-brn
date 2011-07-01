#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <elements/wifi/availablerates.hh>

#include "elements/brn2/brn2.h"
#include "rateselection.hh"
#include "brn_madwifirate.hh"

CLICK_DECLS

#define CREDITS_FOR_RAISE 10
#define STEPUP_RETRY_THRESHOLD 10

BrnMadwifiRate::BrnMadwifiRate()
  : _stepup(0),
    _stepdown(0)
{
  mcs_zero = MCS(0);
}

void *
BrnMadwifiRate::cast(const char *name)
{
  if (strcmp(name, "BrnMadwifiRate") == 0)
    return (BrnMadwifiRate *) this;
  else if (strcmp(name, "RateSelection") == 0)
    return (RateSelection *) this;
  else
    return NULL;
}

BrnMadwifiRate::~BrnMadwifiRate()
{
}

int
BrnMadwifiRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _alt_rate = false;
  _period = 1000;
  int ret = Args(conf, this, errh)
      .read("ALT_RATE", _alt_rate)
      .read("PERIOD", _period)
      .read("DEBUG", _debug)
      .complete();
  return ret;
}

void
BrnMadwifiRate::adjust_all(NeighborTable *neighbors)
{
  Vector<EtherAddress> n;

  for (NIter iter = neighbors->begin(); iter.live(); iter++) {
    NeighbourRateInfo nri = iter.value();
    n.push_back(nri._eth);
  }

  for (int x =0; x < n.size(); x++) {
    adjust(neighbors, n[x]);
  }
}

void
BrnMadwifiRate::adjust(NeighborTable *neighbors, EtherAddress dst)
{
  NeighbourRateInfo *nri = neighbors->findp(dst);
  DstInfo *nfo = (DstInfo*)nri->_rs_data;

  BRN_DEBUG("Adjust");

  if (!nfo) return;

  bool stepup = false;
  bool stepdown = false;
  if (nfo->_failures > 0 && nfo->_successes == 0) {
    stepdown = true;
  }

  bool enough = (nfo->_successes + nfo->_failures) > 10;

  /* all packets need retry in average */
  if (enough && nfo->_successes < nfo->_retries)
    stepdown = true;

  /* no error and less than 10% of packets need retry */
  if (enough && nfo->_failures == 0 &&
      nfo->_retries < (nfo->_successes * STEPUP_RETRY_THRESHOLD) / 100)
    stepup = true;

  if (stepdown) {
    if ( WIFI_MAX(nfo->_current_index - 1, 0) != nfo->_current_index) {
      BRN_DEBUG("stepping down for %s from %d to %d\n",
                   nri->_eth.unparse().c_str(), nri->_rates[nfo->_current_index].get_rate(),
                   nri->_rates[WIFI_MAX(0, nfo->_current_index - 1)].get_rate());
    }
    nfo->_current_index = WIFI_MAX(nfo->_current_index - 1, 0);
    nfo->_credits = 0;
  } else if (stepup) {
    nfo->_credits++;
    if (nfo->_credits >= CREDITS_FOR_RAISE) {
      BRN_DEBUG("steping up for %s from %d to %d\n",
                   nri->_eth.unparse().c_str(), nri->_rates[nfo->_current_index].get_rate(),
                   nri->_rates[WIFI_MIN(nri->_rates.size() - 1, nfo->_current_index + 1)].get_rate());
      nfo->_current_index = WIFI_MIN(nfo->_current_index + 1, nri->_rates.size() - 1);
      nfo->_credits = 0;
    }
  } else {
    if (enough && nfo->_credits > 0) {
      nfo->_credits--;
    }
  }
  nfo->_successes = 0;
  nfo->_failures = 0;
  nfo->_retries = 0;
}

void
BrnMadwifiRate::process_feedback(click_wifi_extra *ceh, NeighbourRateInfo *nri)
{
  bool success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);
  bool used_alt_rate = ceh->flags & WIFI_EXTRA_TX_USED_ALT_RATE;

  DstInfo *nfo = (DstInfo*)(nri->_rs_data);

  MCS mcs = MCS(ceh, 0);

  if (!nfo || ! nri->pick_rate(nfo->_current_index).equals(mcs)) {
    return;
  }

  if (!success ) {
    BRN_DEBUG("packet failed %s success %d rate %d alt %d\n",
               nri->_eth.unparse().c_str(), success, MCS(ceh->rate).get_rate(), MCS(ceh->rate1).get_rate() );
  }

  if (success && (!_alt_rate || !used_alt_rate)) {
    nfo->_successes++;
    nfo->_retries += ceh->retries;
  } else {
    nfo->_failures++;
    nfo->_retries += 4;
  }

  return;
}

void
BrnMadwifiRate::assign_rate(click_wifi_extra *ceh, NeighbourRateInfo *nri)
{
  if (nri->_eth.is_group()) {
    if (nri->_rates.size()) {
      nri->_rates[0].setWifiRate(ceh,0);
    } else {  //TODO: Shit default. 1MBit not available for pure g and a
      BrnWifi::setMCS(ceh,0,0);
      ceh->rate = 2;
    }
    return;
  }

  DstInfo *nfo = (DstInfo*)nri->_rs_data;

  if (!nfo) {
    click_chatter("New dst info");
    nfo = new DstInfo();
    nri->_rs_data = (void*)nfo;

    nfo->_successes = 0;
    nfo->_retries = 0;
    nfo->_failures = 0;
    /* initial to 24 in g/a, 11 in b */
    int ndx = nri->rate_index((uint32_t)48);
    ndx = ndx > 0 ? ndx : nri->rate_index((uint32_t)22);
    ndx = WIFI_MAX(ndx, 0);
    nfo->_current_index = ndx;
    nfo->_credits = 0;

    BRN_DEBUG("initial rate for %s is %d\n",
              nri->_eth.unparse().c_str(), nri->_rates[nfo->_current_index].get_rate());
  }

  ceh->magic = WIFI_EXTRA_MAGIC;
  int ndx = nfo->_current_index;

  nri->_rates[ndx].setWifiRate(ceh,0);
  if (ndx - 1 >= 0) nri->_rates[WIFI_MAX(ndx - 1, 0)].setWifiRate(ceh,1);
  else mcs_zero.setWifiRate(ceh,1);
  if (ndx - 2 >= 0) nri->_rates[WIFI_MAX(ndx - 2, 0)].setWifiRate(ceh,2);
  else mcs_zero.setWifiRate(ceh,2);
  if (ndx - 3 >= 0) nri->_rates[WIFI_MAX(ndx - 3, 0)].setWifiRate(ceh,3);
  else mcs_zero.setWifiRate(ceh,3);

  ceh->max_tries = 4;
  ceh->max_tries1 = (ndx - 1 >= 0) ? 2 : 0;
  ceh->max_tries2 = (ndx - 2 >= 0) ? 2 : 0;
  ceh->max_tries3 = (ndx - 3 >= 0) ? 2 : 0;

  return;
}

String
BrnMadwifiRate::print_neighbour_info(NeighbourRateInfo *nri, int tabs)
{
  StringAccum sa;

  DstInfo *nfo = (DstInfo*)nri->_rs_data;

  for ( int i = 0; i < tabs; i++ ) sa << "\t";

  uint32_t rate = nri->_rates[nfo->_current_index].get_rate();
  uint32_t rate_l = rate % 10;
  uint32_t rate_h = rate / 10;

  sa << "<neighbour addr=\"" << nri->_eth.unparse() << "\" rate=\"" << rate_h << "." << rate_l;
  sa << "\" successes=\"" << (int)nfo->_successes << "\" failures=\"" << (int)nfo->_failures;
  sa << "\" retries=\"" << (int)nfo->_retries << "\" credits=\"" << (int)nfo->_credits << "\" />\n";

  return sa.take_string();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

enum { H_STEPUP, H_STEPDOWN, H_PERIOD, H_ALT_RATE};

static String
BrnMadwifiRate_read_param(Element *e, void *thunk)
{
  BrnMadwifiRate *td = (BrnMadwifiRate *)e;
  switch ((uintptr_t) thunk) {
   case H_STEPDOWN:
      return String(td->_stepdown) + "\n";
    case H_STEPUP:
      return String(td->_stepup) + "\n";
    case H_ALT_RATE:
      return String(td->_alt_rate) + "\n";
    case H_PERIOD:
      return String(td->_period) + "\n";
    default:
      return String();
  }
}

static int
BrnMadwifiRate_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BrnMadwifiRate *f = (BrnMadwifiRate *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_ALT_RATE: {
      bool alt_rate;
      if (!BoolArg().parse(s, alt_rate))
        return errh->error("alt_rate parameter must be boolean");
      f->_alt_rate = alt_rate;
      break;
    }
    case H_STEPUP: {
      unsigned m;
      if (!IntArg().parse(s, m))
        return errh->error("stepup parameter must be unsigned");
      f->_stepup = m;
      break;
    }
    case H_STEPDOWN: {
      unsigned m;
      if (!IntArg().parse(s, m))
        return errh->error("stepdown parameter must be unsigned");
      f->_stepdown = m;
      break;
    }
    case H_PERIOD: {
      unsigned m;
      if (!IntArg().parse(s, m))
        return errh->error("period parameter must be unsigned");
      f->_period = m;
      break;
    }
  }
  return 0;
}

void
BrnMadwifiRate::add_handlers()
{
  RateSelection::add_handlers();

  add_read_handler("stepup", BrnMadwifiRate_read_param, (void *) H_STEPUP);
  add_read_handler("stepdown", BrnMadwifiRate_read_param, (void *) H_STEPDOWN);
  add_read_handler("alt_rate", BrnMadwifiRate_read_param, (void *) H_ALT_RATE);

  add_write_handler("stepup", BrnMadwifiRate_write_param, (void *) H_STEPUP);
  add_write_handler("stepdown", BrnMadwifiRate_write_param, (void *) H_STEPDOWN);
  add_write_handler("alt_rate", BrnMadwifiRate_write_param, (void *) H_ALT_RATE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnMadwifiRate)

