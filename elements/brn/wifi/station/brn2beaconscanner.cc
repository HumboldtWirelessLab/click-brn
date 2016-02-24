/*
 * BRN2BeaconScanner.{cc,hh} -- tracks 802.11 beacon sources
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
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include <click/hashmap.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/wifi/brnavailablerates.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brn2beaconscanner.hh"

CLICK_DECLS

BRN2BeaconScanner::BRN2BeaconScanner()
  : _debug(BrnLogger::DEFAULT),
    _timer(this),
    _rtable(0),
    _winfo(0),
    _timeout(500)
{
}

BRN2BeaconScanner::~BRN2BeaconScanner()
{
}

int
BRN2BeaconScanner::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
       "WIRELESS_INFO", cpkP, cpElement, &_winfo,
       "RT", cpkP, cpElement, &_rtable,
       "TIMEOUT", cpkP, cpInteger, &_timeout,
       "DEBUG", cpkP, cpInteger, &_debug,
       cpEnd) < 0)
    return -1;

  if (_rtable && _rtable->cast("BrnAvailableRates") == 0) 
    return errh->error("BrnAvailableRates element is not a BrnAvailableRates");

  _aps.clear();

  return 0;
}

int
BRN2BeaconScanner::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_after_msec(_timeout);
	return 0;
}

void
BRN2BeaconScanner::run_timer(Timer *)
{
  chk_beacon_timeout();
  _timer.schedule_after_msec(_timeout);
}

void
BRN2BeaconScanner::chk_beacon_timeout() {
	/* Check all APs in table if they have timed out.
	 * We do this by means of the beacon interval
	 * and the time now.
	 */
	uint32_t timenow_msec = Timestamp::now().msecval();

	for (APIter iter = _aps.begin(); iter.live(); ++iter) {
		vap ap = iter.value();

		//BRN_DEBUG("INFO bi: %d last_rx: %d ts: %d", (uint32_t)ap._beacon_int, ap._last_rx.msecval(), Timestamp::now().msecval());

		if ((ap._last_rx.msecval() + _timeout) < timenow_msec) {
			_aps.remove(ap._eth);
			_paps.remove(ap._eth);
		}
	}
}

Packet *
BRN2BeaconScanner::simple_action(Packet *p)
{
  //uint8_t dir;
  uint8_t type;
  uint8_t subtype;

  if (_winfo && _winfo->_channel < 0) {
    return p;
  }

  if (p->length() < sizeof(struct click_wifi)) {
    return p;
  }
  struct click_wifi *w = (struct click_wifi *) p->data();

  //dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    return p;
  }

  if (subtype != WIFI_FC0_SUBTYPE_BEACON && subtype != WIFI_FC0_SUBTYPE_PROBE_RESP) {
    return p;
  }

  //################### READ INFO DATA FROM BEACON ##########################

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  //uint32_t timestamp = (uint32_t)*ptr;
  ptr += 8;

  uint16_t beacon_int = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint8_t *end  = (uint8_t *) p->data() + p->length();

  uint8_t *ssid_l = NULL;
  uint8_t *rates_l = NULL;
  uint8_t *xrates_l = NULL;
  uint8_t *ds_l = NULL;
  while (ptr < end) {
    switch (*ptr) {
    case WIFI_ELEMID_SSID:
      ssid_l = ptr;
      break;
    case WIFI_ELEMID_RATES:
      rates_l = ptr;
      break;
    case WIFI_ELEMID_XRATES:
      xrates_l = ptr;
      break;
    case WIFI_ELEMID_FHPARMS:
      break;
    case WIFI_ELEMID_DSPARMS:
      ds_l = ptr;
      break;
    case WIFI_ELEMID_IBSSPARMS:
      break;
    case WIFI_ELEMID_TIM:
      break;
    case WIFI_ELEMID_ERP:
      break;
    case WIFI_ELEMID_VENDOR:
      break;
    case 133: /* ??? */
      break;
    case 150: /* ??? */
      break;
    default:
      BRN_DEBUG("%{element}: ignored element id %u %u \n", this, *ptr, ptr[1]);
    }
    ptr += ptr[1] + 2;

  }

  if (_winfo && _winfo->_channel > 0 && ds_l && ds_l[2] != _winfo->_channel) {
    return p;
  }

  String ssid = "";
  if (ssid_l && ssid_l[1]) {
    //click_chatter("%d %d %d",(int)ssid_l[1],(int)ssid_l[2],(int)ssid_l[3]);
    if ( (int)ssid_l[2] != 0 )
      ssid = String((char *) ssid_l + 2, MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }

  EtherAddress bssid = EtherAddress(w->i_addr3);

  vap *ap = _aps.findp(bssid);
  if (!ap) {
    BRN_DEBUG("ALL ap: %d\n",_aps.size());

    _aps.insert(bssid, vap());

    BRN_DEBUG("add");

    ap = _aps.findp(bssid);
    ap->_ssid = "";
    ap->_ssid_empty = true;
  }

  pap *ac_pap = _paps.findp(bssid);
  if (!ac_pap) {
    _paps.insert(bssid, pap());

    BRN_DEBUG("add pap");

    ac_pap = _paps.findp(bssid);
  }

  String ac_ssid;
  if ( ssid == "" ) ac_ssid = "(none)";
  else ac_ssid = ssid;
  vap *ac_vap = ac_pap->_vaps.findp(ac_ssid);
  if (!ac_vap) {
    ac_pap->_vaps.insert(ac_ssid, vap());

    BRN_DEBUG("add vap");

    ac_vap = ac_pap->_vaps.findp(ac_ssid);
    ac_vap->_ssid = "none";
    ac_vap->_ssid_empty = true;
  }

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  //######################## REGISTER AP ###############################

  ap->_eth = bssid;
  if (ssid != "") {
    /* don't overwrite blank ssids */
    ap->_ssid = ssid;
    ap->_ssid_empty = false;
  }
  ap->_channel = (ds_l) ? ds_l[2] : -1;
  ap->_rssi = ceh->rssi;

  ap->_capability = capability;
  ap->_beacon_int = beacon_int;

  ac_vap->_eth = bssid;
  if (ssid != "") {
    /* don't overwrite blank ssids */
    ac_vap->_ssid = ssid;
    ac_vap->_ssid_empty = false;
  }
  ac_vap->_channel = (ds_l) ? ds_l[2] : -1;
  ac_vap->_rssi = ceh->rssi;

  ac_vap->_capability = capability;
  ac_vap->_beacon_int = beacon_int;

  int recv_channel = BRNPacketAnno::channel_anno(p);

  int i;
  for ( i = 0; i < ac_vap->_recv_channel.size(); i++ )
    if ( ac_vap->_recv_channel[i] == recv_channel ) break;

  if ( i == ac_vap->_recv_channel.size() ) {
    ac_vap->_recv_channel.push_back(recv_channel);
    ap->_recv_channel.push_back(recv_channel);
  }


  //########################### RATES #########################################

  ac_pap->_last_rx = Timestamp::now();

  ap->_basic_rates.clear();
  ap->_rates.clear();
  ap->_last_rx = Timestamp::now();
  ac_vap->_basic_rates.clear();
  ac_vap->_rates.clear();
  ac_vap->_last_rx = Timestamp::now();

  Vector<MCS> all_rates;

  if (rates_l) {
    for (int x = 0; x < MIN((int)rates_l[1], WIFI_RATE_SIZE); x++) {
      uint8_t rate = rates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
        ap->_basic_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
        ac_vap->_basic_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
      } else {
        ap->_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
        ac_vap->_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
      }
      all_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
    }
  }


  if (xrates_l) {
    for (int x = 0; x < MIN((int)xrates_l[1], WIFI_RATE_SIZE); x++) {
      uint8_t rate = xrates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
        ap->_basic_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
        ac_vap->_basic_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
      } else {
        ap->_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
        ac_vap->_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
      }
      all_rates.push_back(MCS((int)(rate & WIFI_RATE_VAL)));
    }
  }

  if (_rtable) {
    _rtable->insert(bssid, all_rates);
  }

  return p;
}


String
BRN2BeaconScanner::scan_string()
{
  StringAccum sa;
  Timestamp now = Timestamp::now();
  sa << "size " << _aps.size() << "\n";
  for (APIter iter = _aps.begin(); iter.live(); ++iter) {
    click_chatter("next ap");
    vap ap = iter.value();
    sa << ap._eth << " ";
    sa << "channel " << ap._channel << " ";
    sa << "rssi " << ap._rssi << " ";
    sa << "ssid ";

    if(ap._ssid == "") {
      sa << "(none) ";
    } else {
      sa << ap._ssid << " ";
    }

    sa << "beacon_interval " << ap._beacon_int << " ";
    sa << "last_rx " << now - ap._last_rx << " ";

    sa << "[ ";
    if (ap._capability & WIFI_CAPINFO_ESS) {
      sa << "ESS ";
    }
    if (ap._capability & WIFI_CAPINFO_IBSS) {
      sa << "IBSS ";
    }
    if (ap._capability & WIFI_CAPINFO_CF_POLLABLE) {
      sa << "CF_POLLABLE ";
    }
    if (ap._capability & WIFI_CAPINFO_CF_POLLREQ) {
      sa << "CF_POLLREQ ";
    }
    if (ap._capability & WIFI_CAPINFO_PRIVACY) {
      sa << "PRIVACY ";
    }
    sa << "] ";

    sa << "( { ";
    for (int x = 0; x < ap._basic_rates.size(); x++) {
      sa << ap._basic_rates[x].to_string() << " ";
    }
    sa << "} ";
    for (int x = 0; x < ap._rates.size(); x++) {
      sa << ap._rates[x].to_string() << " ";
    }

    sa << ")\n";
  }
  return sa.take_string();
}

String
BRN2BeaconScanner::scan_string2()
{
  StringAccum sa;
  Timestamp now = Timestamp::now();
  sa << "size " << _aps.size() << "\n";
  for (PAPIter iter = _paps.begin(); iter.live(); ++iter) {
    pap acpap = iter.value();

    for (VAPIter viter = acpap._vaps.begin(); viter.live(); ++viter) {
      click_chatter("next ap");
      vap ap = viter.value();
      sa << ap._eth << " ";
      sa << "channel " << ap._channel << " ";
      sa << "rssi " << ap._rssi << " ";
      sa << "ssid ";

      if(ap._ssid_empty) {
        sa << "(none) ";
      } else {
        sa << "\"" << ap._ssid << "\" ";
      }

      sa << "beacon_interval " << ap._beacon_int << " ";
      sa << "last_rx " << now - ap._last_rx << " ";

      sa << "[ ";
      if (ap._capability & WIFI_CAPINFO_ESS) {
        sa << "ESS ";
      }
      if (ap._capability & WIFI_CAPINFO_IBSS) {
        sa << "IBSS ";
      }
      if (ap._capability & WIFI_CAPINFO_CF_POLLABLE) {
        sa << "CF_POLLABLE ";
      }
      if (ap._capability & WIFI_CAPINFO_CF_POLLREQ) {
        sa << "CF_POLLREQ ";
      }
      if (ap._capability & WIFI_CAPINFO_PRIVACY) {
        sa << "PRIVACY ";
      }
      sa << "] ";

      sa << "( { ";
      for (int x = 0; x < ap._basic_rates.size(); x++) {
        sa << ap._basic_rates[x].to_string() << " ";
      }
      sa << "} ";
      for (int x = 0; x < ap._rates.size(); x++) {
        sa << ap._rates[x].to_string() << " ";
      }

      sa << ")\n";
    }
  }

  return sa.take_string();
}


void 
BRN2BeaconScanner::reset()
{
  _aps.clear();
}

enum {H_DEBUG, H_SCAN,H_SCAN2, H_RESET};

static String 
BRN2BeaconScanner_read_param(Element *e, void *thunk)
{
  BRN2BeaconScanner *td = reinterpret_cast<BRN2BeaconScanner *>(e);
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
      case H_SCAN:
        return td->scan_string();
      case H_SCAN2:
        return td->scan_string2();
      default:
      return String();
    }
}
static int 
BRN2BeaconScanner_write_param(const String &in_s, Element *e, void *vparam,
        ErrorHandler *errh)
{
  BRN2BeaconScanner *f = reinterpret_cast<BRN2BeaconScanner *>(e);
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_RESET: {    //reset
    f->reset();
  }
  }
  return 0;
}

void
BRN2BeaconScanner::add_handlers()
{
  add_read_handler("debug", BRN2BeaconScanner_read_param, (void *) H_DEBUG);
  add_read_handler("scan", BRN2BeaconScanner_read_param, (void *) H_SCAN);
  add_read_handler("scan2", BRN2BeaconScanner_read_param, (void *) H_SCAN2);

  add_write_handler("debug", BRN2BeaconScanner_write_param, (void *) H_DEBUG);
  add_write_handler("reset", BRN2BeaconScanner_write_param, (void *) H_RESET, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2BeaconScanner)
