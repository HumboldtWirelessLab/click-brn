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
#include "radiotapencap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include <clicknet/radiotap.h>

#include <elements/brn/brnprotocol/brnpacketanno.hh>
#include <elements/brn/wifi/brnwifi.hh>

CLICK_DECLS



#define CLICK_RADIOTAP_PRESENT (		\
	(1 << IEEE80211_RADIOTAP_RATE)		| \
	(1 << IEEE80211_RADIOTAP_CHANNEL)	| \
	(1 << IEEE80211_RADIOTAP_DBM_TX_POWER)	| \
	(1 << IEEE80211_RADIOTAP_TX_FLAGS)	| \
	(1 << IEEE80211_RADIOTAP_RTS_RETRIES)	| \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)	| \
	(1 << IEEE80211_RADIOTAP_MCS)	| \
	(1 << IEEE80211_RADIOTAP_MULTIRATE)	| \
	(1 << IEEE80211_RADIOTAP_DATA_MULTIRETRIES)	| \
	(1 << IEEE80211_RADIOTAP_QUEUE)	| \
	0)

struct click_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int8_t	wt_rate;
	u_int8_t	wt_align_for_channel;
	u_int16_t	wt_channel_frequence;
	u_int16_t	wt_channel_flags;
	u_int8_t	wt_txpower;
	u_int16_t	wt_tx_flags;
	u_int8_t	wt_rts_retries;
	u_int8_t	wt_data_retries;
	/* MCS: see http://www.radiotap.org/defined-fields/MCS */
	u_int8_t	wt_known;            // field indicates which information is known
	u_int8_t	wt_flags;            // HT,FEC,...
	u_int8_t	wt_mcs;              // field indicates the MCS rate index as in IEEE_802.11n-2009
	/* BRN Extention */
	u_int8_t	wt_rates[4];
	u_int8_t	wt_multi_mcs[4];

	u_int8_t	wt_multi_retries[4];
	u_int8_t	wt_queue;
};// __attribute__((__packed__));






RadiotapEncap::RadiotapEncap()
{
}

RadiotapEncap::~RadiotapEncap()
{
}

int
RadiotapEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _debug = false;
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
RadiotapEncap::simple_action(Packet *p)
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
	  struct brn_click_wifi_extra_extention *wee = BrnWifi::get_brn_click_wifi_extra_extention(p_out);

	  memset(crh, 0, sizeof(struct click_radiotap_header));

	  crh->wt_ihdr.it_version = 0;
	  crh->wt_ihdr.it_len = cpu_to_le16(sizeof(struct click_radiotap_header));
	  crh->wt_ihdr.it_present = cpu_to_le32(CLICK_RADIOTAP_PRESENT);

	  if ( BrnWifi::getMCS(ceh, 0) == 1 ) {                                         //Is MCS-rate ?
			crh->wt_rate = RADIOTAP_RATE_MCS_INVALID;                                   //yes, so set normal rate to invalid

			uint8_t mcs_index, mcs_bandwidth, mcs_guard_interval;

			BrnWifi::toMCS(&mcs_index, &mcs_bandwidth, &mcs_guard_interval, ceh->rate); //get mcs data

			crh->wt_known = (uint8_t)_mcs_known;
			crh->wt_flags = mcs_bandwidth | (mcs_guard_interval << 2) | (BrnWifi::getHTMode(wee, 0) << 3) |
                                                                        (BrnWifi::getFEC(wee,0) << 4);
			crh->wt_mcs = mcs_index;

	  } else {
			crh->wt_rate = ceh->rate;
			crh->wt_known = RADIOTAP_RATE_MCS_INVALID;
			crh->wt_flags = RADIOTAP_RATE_MCS_INVALID;
			crh->wt_mcs = RADIOTAP_RATE_MCS_INVALID;
      }

	  crh->wt_rts_retries = 0;
	  crh->wt_txpower = ceh->power;
          if (ceh->flags & WIFI_EXTRA_TX_NOACK) {
                  crh->wt_tx_flags |= IEEE80211_RADIOTAP_F_TX_NOACK;
          }
	  if (ceh->max_tries > 0) {
		  crh->wt_data_retries = ceh->max_tries - 1;
	  } else {
		  crh->wt_data_retries = WIFI_MAX_RETRIES + 1;
	  }

	  crh->wt_channel_frequence = 0; //channel2frequ(BRNPacketAnno::channel_anno(p));
	  crh->wt_channel_flags = 0;

	  crh->wt_rates[0] = ceh->rate;
	  crh->wt_rates[1] = ceh->rate1;
	  crh->wt_rates[2] = ceh->rate2;
	  crh->wt_rates[3] = ceh->rate3;

	  for ( int i = 0; i < 4; i++ ) {
		if ( BrnWifi::getMCS(ceh, i) == 1 ) {
			crh->wt_known = (uint8_t)_mcs_known;
			//set mcs,fec and gf for index
			crh->wt_multi_mcs[i] |= RADIOTAP_RATE_IS_MCS | (wee->mcs_flags[i] << 1);
		}
	  }

	  crh->wt_multi_retries[0] = ceh->max_tries;
	  crh->wt_multi_retries[1] = ceh->max_tries1;
	  crh->wt_multi_retries[2] = ceh->max_tries2;
	  crh->wt_multi_retries[3] = ceh->max_tries3;

      crh->wt_queue = BrnWifi::getTxQueue(ceh);
  }

  return p_out;
}


enum {H_DEBUG};

static String
RadiotapEncap_read_param(Element *e, void *thunk)
{
  RadiotapEncap *td = (RadiotapEncap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
RadiotapEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  RadiotapEncap *f = (RadiotapEncap *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
RadiotapEncap::add_handlers()
{
  add_read_handler("debug", RadiotapEncap_read_param, H_DEBUG);

  add_write_handler("debug", RadiotapEncap_write_param, H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapEncap)
