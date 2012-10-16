
/*
 * OriginalVersion is:
 * printWifi.{cc,hh} -- print Wifi packets, for debugging.
 * John Bicket
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
 *
 * Modified to get more information and to change to Printout
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/wifi/brnwifi.hh"
#include "elements/brn2/brn2.h"

#include "collisioninfo.hh"

CLICK_DECLS

CollisionInfo::CollisionInfo():
    _interval(COLLISIONINFO_DEFAULT_INTERVAL),
    _max_samples(COLLISIONINFO_DEFAULT_MAX_SAMPLES),
    _global_rs(NULL)
{
  BRNElement::init();
}

CollisionInfo::~CollisionInfo()
{
}

int
CollisionInfo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "INTERVAL", cpkP, cpInteger, &_interval,
                     "SAMPLES", cpkP, cpInteger, &_max_samples,
                     "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd);

  if ( _interval <= 0 )
    return errh->error("INTERVAL must be greater than 0.");

  return ret;
}

Packet *
CollisionInfo::simple_action(Packet *p)
{
  struct click_wifi *wh = (struct click_wifi *) p->data();
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  EtherAddress dst;

  if ( ceh->flags & WIFI_EXTRA_TX ) {

    dst = EtherAddress(wh->i_addr1);

    if ( !dst.is_broadcast() ) {
      RetryStats *rs = rs_tab.find(dst);
      Timestamp now = Timestamp::now();
      uint8_t q = BrnWifi::getTxQueue(ceh);

      if ( _global_rs == NULL ) {
        rs_tab.insert(brn_etheraddress_broadcast, new RetryStats(COLLISIONINFO_MAX_HW_QUEUES,
                      _max_samples, _interval, &now));
        _global_rs = rs_tab.find(brn_etheraddress_broadcast);
      }

      _global_rs->update(&now, ceh->retries, ceh->flags, q);


      if ( rs == NULL ) {
        rs_tab.insert(dst, new RetryStats(COLLISIONINFO_MAX_HW_QUEUES,
                                          _max_samples, _interval, &(_global_rs->_last_index_inc)));
        rs = rs_tab.find(dst);
      };

      rs->update(&now, ceh->retries, ceh->flags, q);

    }
    }
    return p;

  
}

enum {H_STATS};

String
CollisionInfo::stats_handler(int /*mode*/)
{
  StringAccum sa;

  sa << "<collisioninfo node=\"" << BRN_NODE_NAME << "\" >\n";
  for (RetryStatsTableIter iter = rs_tab.begin(); iter.live(); iter++) {
    RetryStats *rs = iter.value();
    EtherAddress ea = iter.key();

    sa << "\t<nb addr=\"" << ea.unparse() << "\" time=\"" << rs->_last_index_inc.unparse() << "\" >\n";

    for ( int i = 0; i < rs->_no_queues; i++ ) {
      sa << "\t\t<queue no=\"" << i << "\" succ_rate=\"";

      if (rs->_l_unicast_tx[i] == 0) {
        sa << "-1";
      } else {
        sa << ( (100 * rs->_l_unicast_succ[i]) / rs->_l_unicast_tx[i] );
      }
      sa << "\" />\n";
    }

    sa << "\t</nb>\n";
  }

  sa << "</collisioninfo>\n";

  return sa.take_string();
}

static String
CollisionInfo_read_param(Element *e, void *thunk)
{
  CollisionInfo *td = (CollisionInfo *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return td->stats_handler((uintptr_t) thunk);
      break;
  }

  return String();
}

void
CollisionInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", CollisionInfo_read_param, (void *) H_STATS);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(CollisionInfo)
