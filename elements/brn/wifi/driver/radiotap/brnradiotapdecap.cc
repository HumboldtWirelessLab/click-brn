/*
 * radiotapdecap.{cc,hh} -- decapsultates 802.11 packets
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
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>

#include <elements/brn/brnprotocol/brnpacketanno.hh>
#include <elements/brn/wifi/brnwifi.hh>

#include "brnradiotap.h"

#include "brnradiotapdecap.hh"

CLICK_DECLS

#define NUM_RADIOTAP_ELEMENTS 26

static int32_t frequ_array[] =   {2412,2417,2422,2427,2432,2437,2442,2447,2452,2457,2462,2467,2472,2484,
                                  5180,5200,5220,5240,5260,5280,5300,5320,
                                  5500,5520,5540,5560,5580,5600,5620,5640,5660,5680,5700,
                                  5745,5765,5785,5805,5825};

static int32_t channel_array[] = {1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14, //14
                                  36,  40,  44,  48,  52,  56,  60,  64,                               //8
                                  100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140,               //11
                                  149, 153, 157, 161, 165};                                            //5

static const int radiotap_elem_to_bytes[NUM_RADIOTAP_ELEMENTS] =
	{8, /* IEEE80211_RADIOTAP_TSFT */
	 1, /* IEEE80211_RADIOTAP_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RATE */
	 4, /* IEEE80211_RADIOTAP_CHANNEL */
	 2, /* IEEE80211_RADIOTAP_FHSS */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_LOCK_QUALITY */
	 2, /* IEEE80211_RADIOTAP_TX_ATTENUATION */
	 2, /* IEEE80211_RADIOTAP_DB_TX_ATTENUATION */
	 1, /* IEEE80211_RADIOTAP_DBM_TX_POWER */
	 1, /* IEEE80211_RADIOTAP_ANTENNA */
	 1, /* IEEE80211_RADIOTAP_DB_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DB_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_RX_FLAGS */
	 2, /* IEEE80211_RADIOTAP_TX_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RTS_RETRIES */
	 1, /* IEEE80211_RADIOTAP_DATA_RETRIES */
	 0, /* IEEE80211_RADIOTAP_UNUSED */
	 3, /* IEEE80211_RADIOTAP_MCS */
	 8, /* IEEE80211_RADIOTAP_AMPDU_STATUS */
	 12,/* IEEE80211_RADIOTAP_VHT */
	 8, /* IEEE80211_RADIOTAP_MULTIRATE */
	 4, /* IEEE80211_RADIOTAP_DATA_MULTIRETRIES */
	 1, /* IEEE80211_RADIOTAP_QUEUE */
	 12 /* IEEE80211_RADIOTAP_MULTI_RSSI */
  };


static const char *radiotap_elem_to_char[32 /*NUM_RADIOTAP_ELEMENTS*/] =
  {
    "IEEE80211_RADIOTAP_TSFT",             /*  0 */
    "IEEE80211_RADIOTAP_FLAGS",
    "IEEE80211_RADIOTAP_RATE",
    "IEEE80211_RADIOTAP_CHANNEL",
    "IEEE80211_RADIOTAP_FHSS",
    "IEEE80211_RADIOTAP_DBM_ANTSIGNAL",
    "IEEE80211_RADIOTAP_DBM_ANTNOISE",
    "IEEE80211_RADIOTAP_LOCK_QUALITY",
    "IEEE80211_RADIOTAP_TX_ATTENUATION",
    "IEEE80211_RADIOTAP_DB_TX_ATTENUATION",
    "IEEE80211_RADIOTAP_DBM_TX_POWER",      /* 10 */
    "IEEE80211_RADIOTAP_ANTENNA",
    "IEEE80211_RADIOTAP_DB_ANTSIGNAL",
    "IEEE80211_RADIOTAP_DB_ANTNOISE",
    "IEEE80211_RADIOTAP_RX_FLAGS",
    "IEEE80211_RADIOTAP_TX_FLAGS",          /* 15 */
    "IEEE80211_RADIOTAP_RTS_RETRIES",
    "IEEE80211_RADIOTAP_DATA_RETRIES",
    "IEEE80211_RADIOTAP_UNUSED",
    "IEEE80211_RADIOTAP_MCS",
    "IEEE80211_RADIOTAP_AMPDU_STATUS",      /* 20 */
    "IEEE80211_RADIOTAP_VHT",
    "IEEE80211_RADIOTAP_MULTIRATE",
    "IEEE80211_RADIOTAP_DATA_MULTIRETRIES",
    "IEEE80211_RADIOTAP_QUEUE",
    "IEEE80211_RADIOTAP_MULTI_RSSI",        /* 25 */
    "IEEE80211_RADIOTAP_UNUSED",
    "IEEE80211_RADIOTAP_UNUSED",
    "IEEE80211_RADIOTAP_UNUSED",
    "IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE",/* 29 */
    "IEEE80211_RADIOTAP_VENDOR_NAMESPACE",  /* 30 */
    "IEEE80211_RADIOTAP_EXT",                /* 31 */
  };

static int rt_el_present(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	if (element > NUM_RADIOTAP_ELEMENTS)
		return 0;
	return le32_to_cpu(th->it_present) & (1 << element);
}

static int rt_check_header(struct ieee80211_radiotap_header *th, int len)
{
	int bytes = 0;
	int x = 0;
	if (th->it_version != 0) {
		return 0;
	}

	if (le16_to_cpu(th->it_len) < sizeof(struct ieee80211_radiotap_header)) {
		return 0;
	}

	for (x = 0; x < NUM_RADIOTAP_ELEMENTS; x++) {
		if (rt_el_present(th, x))
		    bytes += radiotap_elem_to_bytes[x];
        click_chatter("%d %s: %d",x, radiotap_elem_to_char[x],((le32_to_cpu(th->it_present) & (1 << x)) != 0)?1:0);
	}

	if (le16_to_cpu(th->it_len) < sizeof(struct ieee80211_radiotap_header) + bytes) {
        click_chatter("Wrong size: %d vs %d", le16_to_cpu(th->it_len), sizeof(struct ieee80211_radiotap_header) + bytes );
		return 0;
	}

	if (le16_to_cpu(th->it_len) > (uint32_t)len) {
        click_chatter("Wrong len: %d vs %d",le16_to_cpu(th->it_len), (uint32_t)len);
		return 0;
	}

	click_chatter("rt is ok");
	return 1;
}

static u_int8_t *rt_el_offset(struct ieee80211_radiotap_header *th, u_int32_t element) {
  unsigned int x = 0;
  u_int8_t *offset = ((u_int8_t *) th) + sizeof(ieee80211_radiotap_header);
  uint8_t pos = 0;
  for (x = 0; x < NUM_RADIOTAP_ELEMENTS && x < element; x++) {
    if (rt_el_present(th, x)) {
      /*final alignment fix*/
      if (( pos & 1) && (( x == IEEE80211_RADIOTAP_CHANNEL ) || ( x == IEEE80211_RADIOTAP_RX_FLAGS ) || ( x == IEEE80211_RADIOTAP_MULTI_RSSI ))) {
        offset++;
        pos++;
      }

      offset += radiotap_elem_to_bytes[x];
      pos += radiotap_elem_to_bytes[x];
    }
  }

  /*final alignment fix*/
  if ( (pos & 1) && (( x == IEEE80211_RADIOTAP_CHANNEL ) || ( x == IEEE80211_RADIOTAP_RX_FLAGS ) || ( x == IEEE80211_RADIOTAP_MULTI_RSSI ))) {
      offset++;
  }

  return offset;
}

static uint8_t
freq2channel(int freq, bool debug)
{
  if ( debug ) click_chatter("Freq: %d",freq);

  int32_t low_index = 0, next_index = 0;
  int32_t high_index = 37;

  while ( low_index < high_index ) {
    next_index = (high_index + low_index) >> 1;
    if ( freq == frequ_array[next_index] ) return channel_array[next_index];
    if ( freq > frequ_array[next_index] ) low_index = next_index + 1;
    else high_index = next_index - 1;
  }

  if ( freq == frequ_array[low_index] ) return channel_array[low_index];
  if ( high_index < 0 ) return 0;
  if ( freq == frequ_array[high_index] ) return channel_array[high_index];

  return 0;
}

static
const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}

BrnRadiotapDecap::BrnRadiotapDecap()
{
}

BrnRadiotapDecap::~BrnRadiotapDecap()
{
}

int
BrnRadiotapDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
		   "DEBUG", 0, cpBool, &_debug,
		   cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
BrnRadiotapDecap::simple_action(Packet *p)
{
  struct ieee80211_radiotap_header *th = (struct ieee80211_radiotap_header *) p->data();
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  struct brn_click_wifi_extra_extention *wee = BrnWifi::get_brn_click_wifi_extra_extention(p);

  //click_chatter("Present: %u start: %u",le32_to_cpu(th->it_present),(void*)&(th[1]));

  if (rt_check_header(th, p->length())) {
		memset((void*)ceh, 0, sizeof(struct click_wifi_extra));
		ceh->magic = WIFI_EXTRA_MAGIC;

		if (rt_el_present(th, IEEE80211_RADIOTAP_FLAGS)) {
			u_int8_t flags = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_FLAGS));
            click_chatter("Flags: %s",byte_to_binary(flags));
			if (flags & IEEE80211_RADIOTAP_F_DATAPAD) {
				ceh->pad = 1;
			}
			if (flags & IEEE80211_RADIOTAP_F_FCS) {
				p->take(4);
			}
//TODO Check type of Error. Now all errors are CRC"
      if (flags & IEEE80211_RADIOTAP_F_BADFCS) {
        ceh->flags |= WIFI_EXTRA_RX_ERR;
        ceh->flags |= WIFI_EXTRA_RX_CRC_ERR;
      }
    }

    if (rt_el_present(th, IEEE80211_RADIOTAP_RATE)) {
      ceh->rate = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_RATE));
      click_chatter("Rate: %d",ceh->rate);
    }

    if (rt_el_present(th, IEEE80211_RADIOTAP_CHANNEL)) {
      uint16_t freq = le16_to_cpu(*((u_int16_t *) rt_el_offset(th, IEEE80211_RADIOTAP_CHANNEL)));
      uint8_t channel = freq2channel(freq, _debug);
      BRNPacketAnno::set_channel_anno(p, channel);
    }

		if (rt_el_present(th, IEEE80211_RADIOTAP_DBM_ANTSIGNAL))
			ceh->rssi = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_DBM_ANTSIGNAL));

		if (rt_el_present(th, IEEE80211_RADIOTAP_DBM_ANTNOISE))
			ceh->silence = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_DBM_ANTNOISE));

		if (rt_el_present(th, IEEE80211_RADIOTAP_DB_ANTSIGNAL))
			ceh->rssi = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_DB_ANTSIGNAL));

		if (rt_el_present(th, IEEE80211_RADIOTAP_DB_ANTNOISE))
			ceh->silence = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_DB_ANTNOISE));

		if (rt_el_present(th, IEEE80211_RADIOTAP_RX_FLAGS)) {
			u_int16_t flags = le16_to_cpu(*((u_int16_t *) rt_el_offset(th, IEEE80211_RADIOTAP_RX_FLAGS)));

      if ((flags & IEEE80211_RADIOTAP_F_RX_BADFCS) || (flags & IEEE80211_RADIOTAP_F_RX_BADPLCP) ) {
				ceh->flags |= WIFI_EXTRA_RX_ERR;
//TODO Check type of Error. Now all errors are CRC"
        ceh->flags |= WIFI_EXTRA_RX_CRC_ERR;
      }
		}

		if (rt_el_present(th, IEEE80211_RADIOTAP_TX_FLAGS)) {
			u_int16_t flags = le16_to_cpu(*((u_int16_t *) rt_el_offset(th, IEEE80211_RADIOTAP_TX_FLAGS)));
			ceh->flags |= WIFI_EXTRA_TX;
			if (flags & IEEE80211_RADIOTAP_F_TX_FAIL)
				ceh->flags |= WIFI_EXTRA_TX_FAIL;
		}

    if (rt_el_present(th, IEEE80211_RADIOTAP_DATA_RETRIES)) {
      ceh->retries = *((u_int8_t *) rt_el_offset(th, IEEE80211_RADIOTAP_DATA_RETRIES));
    }
    if (rt_el_present(th, IEEE80211_RADIOTAP_MCS)) {
      //uint8_t known = 0;
      uint8_t flags = 0;
      uint8_t index = 0;
      uint8_t *rt_el_offset_p = (uint8_t *)rt_el_offset(th, IEEE80211_RADIOTAP_MCS);

      click_chatter("p: %d mcs p: %d",p->data(),rt_el_offset_p);

      //known = rt_el_offset_p[0];
      flags = rt_el_offset_p[1];
      index = rt_el_offset_p[2];

      if ( index != RADIOTAP_RATE_MCS_INVALID ) {
        //click_chatter("known: %d flags: %d index: %d bw: %d gi: %d fec: %d",
        //(int)known,(int)flags,(int)index, (int)(flags & 3), (int)((flags >> 2) & 1), (int)((flags >> 4) & 1));

        BrnWifi::fromMCS(index, (flags & 3), (flags >> 2) & 1, &(ceh->rate));
        BrnWifi::setHTMode(wee, 0, (flags >> 3) & 1);
        BrnWifi::setFEC(wee, 0, (flags >> 4) & 1);
        BrnWifi::setPreambleLength(wee, 0, (flags >> 5) & 1);
        BrnWifi::setSTBC(wee, 0, (flags >> 6) & 1);

        ceh->flags |= WIFI_EXTRA_MCS_RATE0;
      }
    }

    if (rt_el_present(th, IEEE80211_RADIOTAP_MULTI_RSSI)) {
      //click_chatter("Offset: %u (%u)",(void*)rt_el_offset(th, IEEE80211_RADIOTAP_MULTI_RSSI),
      //              ((uint32_t)rt_el_offset(th, IEEE80211_RADIOTAP_MULTI_RSSI)-(uint32_t)&(th[1])));

      struct brn_click_wifi_extra_rx_status *ext_status =
          (struct brn_click_wifi_extra_rx_status *)BRNPacketAnno::get_brn_wifi_extra_rx_status_anno(p);

      memcpy((void*)ext_status, rt_el_offset(th, IEEE80211_RADIOTAP_MULTI_RSSI ), sizeof(struct brn_click_wifi_extra_rx_status));

      BrnWifi::setExtRxStatus(ceh,1);

      //click_chatter("RSSI: %d %d %d %d %d %d %d %d %d %d %d",
      //    ext_status->rssi_ctl[0],ext_status->rssi_ctl[1],ext_status->rssi_ctl[2],
      //    ext_status->rssi_ext[0],ext_status->rssi_ext[1],ext_status->rssi_ext[2],
      //    ext_status->evm[0],ext_status->evm[1],ext_status->evm[2],ext_status->evm[3],ext_status->evm[4]);
    }

    //click_chatter("Len; %d",th->it_len);

    p->pull(le16_to_cpu(th->it_len));
    p->set_mac_header(p->data());  // reset mac-header pointer
  } else {
	  click_chatter("RT-Header contains errors");
  }

  if ( _debug ) click_chatter("Noise: %d",ceh->silence);

  if (ceh->silence == 0) {
    ceh->silence = -95;
    //click_chatter("Silence is 0. Set to -95");
  }

  ceh->rssi = ceh->rssi - ceh->silence;

  return p;
}


enum {H_DEBUG};

static String
BrnRadiotapDecap_read_param(Element *e, void *thunk)
{
  BrnRadiotapDecap *td = (BrnRadiotapDecap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
BrnRadiotapDecap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BrnRadiotapDecap *f = (BrnRadiotapDecap *)e;
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
BrnRadiotapDecap::add_handlers()
{
  add_read_handler("debug", BrnRadiotapDecap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", BrnRadiotapDecap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
/*ELEMENT_REQUIRES(userlevel linux)*/
EXPORT_ELEMENT(BrnRadiotapDecap)
