/*
 * availablerates.{cc,hh} -- Poor man's arp table
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>

#include "brnavailablerates.hh"

CLICK_DECLS

BrnAvailableRates::BrnAvailableRates()
{
  BRNElement::init();
}

BrnAvailableRates::~BrnAvailableRates()
{
}

void *
BrnAvailableRates::cast(const char *n)
{
  if (strcmp(n, "BrnAvailableRates") == 0)
    return (BrnAvailableRates *) this;
  else
    return 0;
}

int
BrnAvailableRates::parse_and_insert(String s, ErrorHandler *errh)
{
  EtherAddress e;
  Vector<MCS> rates;
  Vector<String> args;
  MCS mcs;

  int ht_rate = RATE_HT_NONE;
  bool sgi = false;
  bool ht = false;

  cp_spacevec(s, args);

  if (args.size() < 2) {
    return errh->error("error param %s must have > 1 arg", s.c_str());
  }

  bool default_rates = false;
  if (args[0] == "DEFAULT") {
    default_rates = true;
    _default_rates.clear();
  } else {
     if (!EtherAddressArg().parse(args[0], e))
     return errh->error("error param %s: must start with ethernet address", s.c_str());
  }

  int r = 0;

  for (int x = 1; x < args.size(); x++) {
    if (args[x] == "HT20") {
      ht_rate = RATE_HT20;
      sgi = false;
      ht = false;
    } else if (args[x] == "HT20_SGI") {
      ht_rate = RATE_HT20_SGI;
      sgi = true;
      ht = false;
    } else if (args[x] == "HT40") {
      ht_rate = RATE_HT40;
      sgi = false;
      ht = true;
    } else if (args[x] == "HT40_SGI") {
      ht_rate = RATE_HT40_SGI;
      sgi = true;
      ht = true;
    } else {
      IntArg().parse(args[x], r);

      if ( ht_rate == RATE_HT_NONE ) {
        mcs = MCS(r);
      } else {
        mcs = MCS(r, ht, sgi);
      }
      if (default_rates) {
        _default_rates.push_back(mcs);
      } else {
        rates.push_back(mcs);
      }
    }
  }

  if ( ! default_rates) {
    DstInfo d = DstInfo(e);
    d._rates = rates;
    d._eth = e;
    _rtable.insert(e, d);
  }

  return 0;
}

int
BrnAvailableRates::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = 0;
  for (int x = 0; x < conf.size(); x++) {
    res = parse_and_insert(conf[x], errh);
    if (res != 0)
    {
        return res;
    }
  }

  return res;
}

void
BrnAvailableRates::take_state(Element *e, ErrorHandler *)
{
  BrnAvailableRates *q = (BrnAvailableRates *)e->cast("BrnAvailableRates");
  if (!q) return;
  _rtable = q->_rtable;
  _default_rates = _default_rates;

}

Vector<MCS>
BrnAvailableRates::lookup(EtherAddress eth)
{
  if (!eth) {
    click_chatter("%s: lookup called with NULL eth!\n", name().c_str());
    return Vector<MCS>();
  }

  DstInfo *dst = _rtable.findp(eth);
  if (dst) {
    return dst->_rates;
  }

  if (_default_rates.size()) {
    return _default_rates;
  }

  return Vector<MCS>();
}

int
BrnAvailableRates::insert(EtherAddress eth, Vector<MCS> rates)
{
  if (!(eth)) {
    BRN_DEBUG("BrnAvailableRates %s: You fool, you tried to insert %s\n",
      name().c_str(), eth.unparse().c_str());
    return -1;
  }
  DstInfo *dst = _rtable.findp(eth);
  if (!dst) {
    _rtable.insert(eth, DstInfo(eth));
    dst = _rtable.findp(eth);
  }
  dst->_eth = eth;
  dst->_rates.clear();
  if (_default_rates.size()) {
    /* only add rates that are in the default rates */
    for (int x = 0; x < rates.size(); x++) {
      for (int y = 0; y < _default_rates.size(); y++) {
        if (rates[x]._data_rate == _default_rates[y]._data_rate) {
          dst->_rates.push_back(rates[x]);
        }
      }
    }
  } else {
    dst->_rates = rates;
  }
  return 0;
}


enum {H_INSERT, H_REMOVE, H_RATES};

static String
BrnAvailableRates_read_param(Element *e, void *thunk)
{
  BrnAvailableRates *td = (BrnAvailableRates *)e;
  switch ((uintptr_t) thunk) {
  case H_RATES: {
    StringAccum sa;
    sa << "<available_rates>\n\t<default rates=\"";
    if (td->_default_rates.size()) {
      for (int x = 0; x < td->_default_rates.size(); x++) {
        if ( x != 0 ) {
          sa << ",";
          if (x % 20 == 0) sa << "\n\t\t\t";
        }

        sa << td->_default_rates[x]._data_rate;
      }
      sa << "\" />\n\t<destination>\n";
    }
    for (BrnAvailableRates::RIter iter = td->_rtable.begin(); iter.live(); iter++) {
      BrnAvailableRates::DstInfo n = iter.value();
      sa << "\t\t<node addr=\"" << n._eth.unparse() << "\" rates=\"";
      for (int x = 0; x < n._rates.size(); x++) {
        if ( x != 0 ) sa << " ";
        sa << n._rates[x]._data_rate;
      }
      sa << "\" />\n";
    }
    sa << "\t</destination>\n</available_rates>";

    return sa.take_string();
  }
  default:
    return String();
  }
}
static int
BrnAvailableRates_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BrnAvailableRates *f = (BrnAvailableRates *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_INSERT:
    return f->parse_and_insert(in_s, errh);
  case H_REMOVE: {
    EtherAddress e;
    if (!EtherAddressArg().parse(s, e))
      return errh->error("remove parameter must be ethernet address");
    f->_rtable.erase(e);
    break;
  }

  }
  return 0;
}

void
BrnAvailableRates::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("rates", BrnAvailableRates_read_param, (void *) H_RATES);

  add_write_handler("insert", BrnAvailableRates_write_param, (void *) H_INSERT);
  add_write_handler("remove", BrnAvailableRates_write_param, (void *) H_REMOVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnAvailableRates)

