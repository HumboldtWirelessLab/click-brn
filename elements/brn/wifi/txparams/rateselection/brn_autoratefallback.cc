#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>

#include <elements/wifi/availablerates.hh>

#include "elements/brn/brn2.h"
#include "rateselection.hh"
#include "brn_autoratefallback.hh"

CLICK_DECLS


BrnAutoRateFallback::BrnAutoRateFallback()
  : _stepup(10),
    _stepdown(1)
{
  mcs_zero = MCS(0);
}

BrnAutoRateFallback::~BrnAutoRateFallback()
{
}

void *
BrnAutoRateFallback::cast(const char *name)
{
  if (strcmp(name, "BrnAutoRateFallback") == 0)
    return (BrnAutoRateFallback *) this;
  else if (strcmp(name, "RateSelection") == 0)
    return (RateSelection *) this;
  else
    return NULL;
}

int
BrnAutoRateFallback::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _adaptive_stepup = true;
  int ret = Args(conf, this, errh)
      .read("ADAPTIVE_STEPUP", _adaptive_stepup)
      .read("STEPUP", _stepup)
      .read("STEPDOWN", _stepdown)
      .read("DEBUG", _debug)
      .complete();
  return ret;
}

void
BrnAutoRateFallback::process_feedback(click_wifi_extra *eh, NeighbourRateInfo *nri)
{
  bool success = !(eh->flags & WIFI_EXTRA_TX_FAIL);
  bool used_alt_rate = (eh->flags & WIFI_EXTRA_TX_USED_ALT_RATE);

  MCS mcs = MCS(eh, 0);
  DstInfo *nfo = (DstInfo *)nri->_rs_data;

  if (!nfo || !nri->_rates[nfo->_current_index].equals(mcs)) {
    return;
  }

  if (used_alt_rate || !success) {
    /* step down 1 or 2 rates */
    int down = eh->retries / 2;
    if (down > 0) {
      down--;
    }
    int next_index = WIFI_MAX(0, nfo->_current_index - down);
    BRN_DEBUG("%{element} stepping down for %s from %d to %d\n",
                    this, nri->_eth.unparse().c_str(), nri->_rates[nfo->_current_index].get_rate(),
                          nri->_rates[next_index].get_rate());

    if (nfo->_wentup && _adaptive_stepup) {
      /* backoff the stepup */
      nfo->_stepup *= 2;
      nfo->_wentup = false;
    } else {
      nfo->_stepup = _stepup;
    }

    nfo->_successes = 0;
    nfo->_current_index = next_index;

    return;
  }

  if (nfo->_wentup) {
    /* reset adaptive stepup on a success */
    nfo->_stepup = _stepup;
  }
  nfo->_wentup = false;

  if (eh->retries == 0) {
    nfo->_successes++;
  } else {
    nfo->_successes = 0;
  }

  if (nfo->_successes > nfo->_stepup &&
      nfo->_current_index != nri->_rates.size() - 1) {

    BRN_DEBUG("%{element} steping up for %s from %d to %d\n",
              this, nri->_eth.unparse().c_str(), nri->_rates[nfo->_current_index].get_rate(),
                    nri->_rates[WIFI_MIN(nri->_rates.size() - 1, nfo->_current_index + 1)].get_rate());

    nfo->_current_index = WIFI_MIN(nfo->_current_index + 1, nri->_rates.size() - 1);
    nfo->_successes = 0;
    nfo->_wentup = true;
  }
  return;
}

void
BrnAutoRateFallback::assign_rate(click_wifi_extra *eh, NeighbourRateInfo *nri)
{
  if (nri->_eth.is_group()) {
    if (nri->_rates.size()) {
      nri->_rates[0].setWifiRate(eh,0);
    } else {  //TODO: Shit default. 1MBit not available for pure g and a
      BrnWifi::setMCS(eh,0,0);
      eh->rate = 2;
    }
    return;
  }

  DstInfo *nfo = (DstInfo*)nri->_rs_data;

  if (!nfo) {
    sort_rates_by_data_rate(nri);

    nfo = new DstInfo();
    nri->_rs_data = (void*)nfo;

    nfo->_successes = 0;
    nfo->_wentup = false;
    nfo->_stepup = _stepup;
    /* start at the highest rate */
    nfo->_current_index = nri->_rates.size() - 1;
    BRN_DEBUG("initial rate for %s is %d\n",
               nri->_eth.unparse().c_str(), nri->_rates[nfo->_current_index].get_rate());
  }

  eh->magic = WIFI_EXTRA_MAGIC;
  int ndx = nfo->_current_index;
  nri->_rates[ndx].setWifiRate(eh,0);
  if (ndx - 1 >= 0) nri->_rates[WIFI_MAX(ndx - 1, 0)].setWifiRate(eh,1);
  else mcs_zero.setWifiRate(eh,1);
  if (ndx - 2 >= 0) nri->_rates[WIFI_MAX(ndx - 2, 0)].setWifiRate(eh,2);
  else mcs_zero.setWifiRate(eh,2);
  if (ndx - 3 >= 0) nri->_rates[WIFI_MAX(ndx - 3, 0)].setWifiRate(eh,3);
  else mcs_zero.setWifiRate(eh,3);

  eh->max_tries = 4;
  eh->max_tries1 = (ndx - 1 >= 0) ? 2 : 0;
  eh->max_tries2 = (ndx - 2 >= 0) ? 2 : 0;
  eh->max_tries3 = (ndx - 3 >= 0) ? 2 : 0;
  return;

}

String
BrnAutoRateFallback::print_neighbour_info(NeighbourRateInfo *nri, int tabs)
{
  StringAccum sa;

  DstInfo *nfo = (DstInfo*) nri->_rs_data;

  for ( int i = 0; i < tabs; i++ ) sa << "\t";

  uint32_t rate = nri->_rates[nfo->_current_index].get_rate();
  uint32_t rate_l = rate % 10;
  uint32_t rate_h = rate / 10;

  sa << "<neighbour addr=\"" << nri->_eth.unparse() << "\" rate=\"" << rate_h << "." << rate_l;
  sa << "\" successes=\"" << (int)nfo->_successes << "\" stepup=\"" << (int)nfo->_stepup;
  sa << "\" wentup=\"" << (int)nfo->_wentup << "\" >\n";

  for ( int i = 0; i < tabs; i++ ) sa << "\t";

  sa << "\t<rates>";

  for ( int i = 0; i < nri->_rates.size(); i++) {
    if (i > 0)
        sa << ",";
    if (i % 20 == 0)
        sa << "\n\t\t\t\t";

    sa << nri->_rates[i]._data_rate;
  }

  sa << "\n\t\t\t</rates>\n";

  for ( int i = 0; i < tabs; i++ ) sa << "\t";
  sa << "</neighbour>\n";

  return sa.take_string();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

enum {H_STEPUP, H_STEPDOWN };

static String
BrnAutoRateFallback_read_param(Element *e, void *thunk)
{
  BrnAutoRateFallback *td = (BrnAutoRateFallback *)e;
  switch ((uintptr_t) thunk) {
    case H_STEPDOWN:
      return String(td->_stepdown) + "\n";
    case H_STEPUP:
      return String(td->_stepup) + "\n";
    default:
      return String();
  }
}

static int
BrnAutoRateFallback_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BrnAutoRateFallback *f = (BrnAutoRateFallback *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
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
  }
  return 0;
}

void
BrnAutoRateFallback::add_handlers()
{
  RateSelection::add_handlers();

  add_read_handler("stepup", BrnAutoRateFallback_read_param, (void *) H_STEPUP);
  add_read_handler("stepdown", BrnAutoRateFallback_read_param, (void *) H_STEPDOWN);

  add_write_handler("stepup", BrnAutoRateFallback_write_param, (void *) H_STEPUP);
  add_write_handler("stepdown", BrnAutoRateFallback_write_param, (void *) H_STEPDOWN);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnAutoRateFallback)
