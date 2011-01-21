/*
 * radiotapencap.{cc,hh} -- encapsultates 802.11 packets
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include "brnradiotapencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
//#include <clicknet/radiotap.h>
#include "brnradiotap.h"
#include "elements/brn2/wifi/brnwifi.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

CLICK_DECLS



#define CLICK_RADIOTAP_PRESENT (		\
	(1 << IEEE80211_RADIOTAP_RATE)		| \
  (1 << IEEE80211_RADIOTAP_CHANNEL)    | \
	(1 << IEEE80211_RADIOTAP_DBM_TX_POWER)	| \
	(1 << IEEE80211_RADIOTAP_RTS_RETRIES)	| \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)	| \
  (1 << IEEE80211_RADIOTAP_RATE_1)  | \
  (1 << IEEE80211_RADIOTAP_RATE_2)  | \
  (1 << IEEE80211_RADIOTAP_RATE_3)  | \
  (1 << IEEE80211_RADIOTAP_DATA_RETRIES_1)  | \
  (1 << IEEE80211_RADIOTAP_DATA_RETRIES_2)  | \
  (1 << IEEE80211_RADIOTAP_DATA_RETRIES_3)  | \
  (1 << IEEE80211_RADIOTAP_QUEUE)  | \
  0)

struct click_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int8_t	wt_rate;
  u_int8_t  wt_channel;
  u_int8_t	wt_txpower;
	u_int8_t  wt_rts_retries;
	u_int8_t  wt_data_retries;
  u_int8_t  wt_rate1;
  u_int8_t  wt_rate2;
  u_int8_t  wt_rate3;
  u_int8_t  wt_data_retries1;
  u_int8_t  wt_data_retries2;
  u_int8_t  wt_data_retries3;
  u_int8_t  wt_queue;
};






BrnRadiotapEncap::BrnRadiotapEncap()
{
}

BrnRadiotapEncap::~BrnRadiotapEncap()
{
}

int
BrnRadiotapEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
		   "DEBUG", 0, cpBool, &_debug,
		   cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
BrnRadiotapEncap::simple_action(Packet *p)
{

  WritablePacket *p_out = p->uniqueify();
  if (!p_out) {
    p->kill();
    return 0;
  }

  p_out = p_out->push(sizeof(struct click_radiotap_header));

  if (p_out) {
	  struct click_radiotap_header *crh  = (struct click_radiotap_header *) p_out->data();
	  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	  memset(crh, 0, sizeof(struct click_radiotap_header));

	  crh->wt_ihdr.it_version = 0;
	  crh->wt_ihdr.it_len = cpu_to_le16(sizeof(struct click_radiotap_header));
	  crh->wt_ihdr.it_present = cpu_to_le32(CLICK_RADIOTAP_PRESENT);

	  crh->wt_rate = ceh->rate;
	  crh->wt_txpower = ceh->power;
	  crh->wt_rts_retries = 0;
	  if (ceh->max_tries > 0) {
		  crh->wt_data_retries = ceh->max_tries - 1;
	  } else {
		  crh->wt_data_retries = WIFI_MAX_RETRIES + 1;
	  }

    crh->wt_channel = BRNPacketAnno::channel_anno(p);
    crh->wt_rate1 = ceh->rate1;
    crh->wt_rate2 = ceh->rate2;
    crh->wt_rate3 = ceh->rate3;
    crh->wt_data_retries1 = ceh->max_tries1;
    crh->wt_data_retries2 = ceh->max_tries2;
    crh->wt_data_retries3 = ceh->max_tries3;
    crh->wt_queue = BRNPacketAnno::tos_anno(p);
  }

  return p_out;
}


enum {H_DEBUG};

static String
BrnRadiotapEncap_read_param(Element *e, void *thunk)
{
  BrnRadiotapEncap *td = (BrnRadiotapEncap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
BrnRadiotapEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BrnRadiotapEncap *f = (BrnRadiotapEncap *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
BrnRadiotapEncap::add_handlers()
{
  add_read_handler("debug", BrnRadiotapEncap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", BrnRadiotapEncap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapEncap)
