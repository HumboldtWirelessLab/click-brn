/*
 * brn2_beaconsource.{cc,hh} -- sends 802.11 beacon packets
 * John Bicket
 * extended by Robert Sombrutzki
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
#include <click/hashmap.hh>
#include <click/packet_anno.hh>
#include <click/timer.hh>
#include <click/error.hh>
#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/brn2.h"

#include "elements/brn/wifi/brnavailablerates.hh"
#include "brn2_beaconsource.hh"
#include "../brn2_wirelessinfolist.hh"
#include "../../brnprotocol/brnpacketanno.hh"

CLICK_DECLS

BRN2BeaconSource::BRN2BeaconSource()
  : _timer(this),
    _debug(false),
    _active(true),
    _switch_channel_countdown(0),
    _rtable(0),
    _headroom(32)
{
  BRNElement::init();
  _bcast = EtherAddress();
  memset(_bcast.data(), 0xff, 6);
}

BRN2BeaconSource::~BRN2BeaconSource()
{
}

int
BRN2BeaconSource::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _active = true;
  if (cp_va_kparse(conf, this, errh,
      "WIRELESS_INFO", cpkP, cpElement, &_winfo,
      "WIRELESSINFOLIST", cpkP, cpElement, &_winfolist,
      "RT", cpkP, cpElement, &_rtable,
      "ACTIVE", cpkP, cpBool, &_active,
      "HEADROOM", cpkP, cpInteger, &_headroom,
      "DEBUG", cpkP, cpBool, &_debug,
    cpEnd) < 0)
    return -1;


  if (!_rtable || _rtable->cast("BrnAvailableRates") == 0) 
    return errh->error("BrnAvailableRates element is not provided or not a BrnAvailableRates");

  if ( (!_winfo || _winfo->cast("WirelessInfo") == 0) &&
    (!_winfolist || _winfolist->cast("BRN2WirelessInfoList") == 0 ) )
    return errh->error("Nether WirelessInfo nor WirelessInfoList are given");

  if (_winfo->_interval <= 0) {
    return errh->error("INTERVAL must be >0\n");
  }

  return 0;
}

int
BRN2BeaconSource::initialize (ErrorHandler *)
{
  unsigned int _min_jitter  = 0 /* ms */;
  unsigned int _jitter      = _winfo->_interval;

  click_brn_srandom();

  unsigned int j = (unsigned int) ( _min_jitter +( click_random() % ( _jitter ) ) );

  _timer.initialize(this);
  _timer.schedule_after_msec(j);

  return 0;
}

void
BRN2BeaconSource::run_timer(Timer *)
{
  if (_active)
    send_probe_beacon(_bcast, false, "");

  _timer.schedule_after_msec(_winfo->_interval);
}

void
BRN2BeaconSource::send_probe_beacon(EtherAddress dst, bool probe, String ssid) {
  BRN2WirelessInfoList::WifiInfo *wi;

  if ( ssid != "" )
    send_beacon(dst, probe, ssid);
  else {  //Just send the first available SSID, since client element is confused by multiple ssid having the same bssid
    if ( _winfo ) {
      send_beacon(dst, probe, _winfo->_ssid);
    } else if ( _winfolist ) {
      for ( int i = 0; i < _winfolist->countWifiInfo(); i++ ) {
        wi = _winfolist->getWifiInfo(i);
        if ( (wi->_ssid != NULL) && (wi->_ssid != "" ) ) {
          send_beacon(dst, probe, wi->_ssid);
          break;
        }
      }
    }
  }
}

void
BRN2BeaconSource::send_beacon(EtherAddress dst, bool probe, String ssid)
{
  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();  //TODO:
  String my_ssid = ssid;

  Vector<MCS> rates = _rtable->lookup(bssid);
  int max_len = sizeof (struct click_wifi) + 
    8 +                  /* timestamp */
    2 +                  /* beacon interval */
    2 +                  /* cap_info */
    2 + my_ssid.length() + /* ssid */
    2 + WIFI_RATES_MAXSIZE +  /* rates */
    2 + WIFI_RATES_MAXSIZE +  /* xrates */
    2 + 1 +              /* ds parms */
    2 + 4 +              /* tim */
    0;

  if ( _switch_channel_countdown > 0 ) {
    max_len += 2 + 3;                   //Channel Switch Announcement
  }

  WritablePacket *p = WritablePacket::make(_headroom, NULL, max_len, 32);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT;
  if (probe) {
    w->i_fc[0] |= WIFI_FC0_SUBTYPE_PROBE_RESP;
  } else {
    w->i_fc[0] |=  WIFI_FC0_SUBTYPE_BEACON;
  }

  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, dst.data(), 6);
  memcpy(w->i_addr2, bssid.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);

  w->i_dur = 0;
  w->i_seq = 0;

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
  int actual_length = sizeof (struct click_wifi);


  /* timestamp is set in the hal. ??? */
  memset(ptr, 0, 8);
  ptr += 8;
  actual_length += 8;

  uint16_t beacon_int = (uint16_t) _winfo->_interval;
  *(uint16_t *)ptr = cpu_to_le16(beacon_int);
  ptr += 2;
  actual_length += 2;

  uint16_t cap_info = 0;
  cap_info |= WIFI_CAPINFO_ESS;

  if (probe && is_protected_ssid(ssid)) {
    click_chatter("SSID %s is protected.", ssid.c_str());
    cap_info |= WIFI_CAPINFO_PRIVACY;
  }

  *(uint16_t *)ptr = cpu_to_le16(cap_info);
  ptr += 2;
  actual_length += 2;

  /* ssid */
  ptr[0] = WIFI_ELEMID_SSID;
  ptr[1] = my_ssid.length();
  memcpy(ptr + 2, my_ssid.data(), my_ssid.length());
  ptr += 2 + my_ssid.length();
  actual_length += 2 + my_ssid.length();

  /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = MIN(WIFI_RATE_SIZE, rates.size());
  for (int x = 0; x < MIN(WIFI_RATE_SIZE, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x].get_packed_8();

    if (rates[x].get_packed_8() == 2) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }

  }
  ptr += 2 + MIN(WIFI_RATE_SIZE, rates.size());
  actual_length += 2 + MIN(WIFI_RATE_SIZE, rates.size());

  int num_xrates = rates.size() - WIFI_RATE_SIZE;
  if (num_xrates > 0) {
    /* rates */
    ptr[0] = WIFI_ELEMID_XRATES;
    ptr[1] = num_xrates;
    for (int x = 0; x < num_xrates; x++) {
      ptr[2 + x] = (uint8_t) rates[x + WIFI_RATE_SIZE].get_packed_8();

      if (rates[x + WIFI_RATE_SIZE].get_packed_8() == 2) {
        ptr [2 + x] |= WIFI_RATE_BASIC;
      }

    }
    ptr += 2 + num_xrates;
    actual_length += 2 + num_xrates;
  }


  /* channel */
  ptr[0] = WIFI_ELEMID_DSPARMS;
  ptr[1] = 1;
  ptr[2] = (uint8_t) _winfo->_channel;
  ptr += 2 + 1;
  actual_length += 2 + 1;

  if ( _switch_channel_countdown > 0 ) {
    /* channel switch */
    _switch_channel_countdown--;

    ptr[0] = 37; //WIFI_ELEMID_CSA;
    ptr[1] = 3;

    ptr[2] = 0;                                                //Transsmission district
    ptr[3] = _target_channel;                                  // new channel
    ptr[4] = (uint8_t)_switch_channel_countdown;               //bitmap control

    ptr += 2 + 3;
    actual_length += 2 + 3;

    if ( _switch_channel_countdown == 0 )
    {
      _winfo->_channel = _target_channel;
      BRNPacketAnno::set_channel_anno(p, _winfo->_channel, OPERATION_SET_CHANNEL_AFTER_PACKET);
    }
  }

  /* tim */

  ptr[0] = WIFI_ELEMID_TIM;
  ptr[1] = 4;

  ptr[2] = 0; //count
  ptr[3] = 1; //period
  ptr[4] = 0; //bitmap control
  ptr[5] = 0; //paritial virtual bitmap
  ptr += 2 + 4;
  actual_length += 2 + 4;

  p->take(max_len - actual_length);

  output(0).push(p);
}

void
BRN2BeaconSource::push(int, Packet *p)
{

  uint8_t type;
  uint8_t subtype;

  if (p->length() < sizeof(struct click_wifi)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_wifi));

    p->kill();
    return;

  }
  struct click_wifi *w = (struct click_wifi *) p->data();

  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%{element}: received non-management packet\n",
     this);
    p->kill();
    return;
  }

  if (subtype != WIFI_FC0_SUBTYPE_PROBE_REQ) {
    click_chatter("%{element}: received non-probe-req packet\n",
		  this);
    p->kill();
    return;
  }

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  uint8_t *end  = (uint8_t *) p->data() + p->length();

  uint8_t *ssid_l = NULL;
  uint8_t *rates_l = NULL;

  while (ptr < end) {
    switch (*ptr) {
    case WIFI_ELEMID_SSID:
      ssid_l = ptr;
      break;
    case WIFI_ELEMID_RATES:
      rates_l = ptr;
      break;
    default:
      if (_debug) {
	click_chatter("%{element}: ignored element id %u %u \n",
		      this,
		      *ptr,
		      ptr[1]);
      }
    }
    ptr += ptr[1] + 2;

  }

  StringAccum sa;
  String ssid = "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }

  if (ssid != "" && ssid != _winfo->_ssid) {
    if ( ( _winfolist == NULL ) || ( _winfolist->getWifiInfoForBSSID(ssid) == NULL) ) {
      if (_debug) {
        click_chatter("%{element} other ssid %s wanted %s\n",
                      this,
                      ssid.c_str(),
                      _winfo->_ssid.c_str());
      }
      p->kill();
      return;
    }
  }

  EtherAddress src = EtherAddress(w->i_addr2);

  sa << "ProbeReq: " << src << " ssid " << ssid << " ";

  sa << "rates {";
  if (rates_l) {
    for (int x = 0; x < MIN((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
      uint8_t rate = rates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
	sa << " * " << (int) (rate ^ WIFI_RATE_BASIC);
      } else {
	sa << " " << (int) rate;
      }
    }
  }
  sa << " }";

  if (_debug) {
    click_chatter("%{element}: %s\n",
		  this,
		  sa.take_string().c_str());
  }
  send_probe_beacon(src, true, ssid);

  p->kill();
  return;
}

bool
BRN2BeaconSource::is_protected_ssid(String ssid)
{
  BRN2WirelessInfoList::WifiInfo *wi;

  if ( ssid == "" ) return false;
  if ( _winfo && ( _winfo->_ssid == ssid ) ) return false;
  if ( _winfolist ) {
    for ( int i = 0; i < _winfolist->countWifiInfo(); i++ ) {
       wi = _winfolist->getWifiInfo(i);
       if (wi->_ssid == ssid ) return wi->_protected;
    }
  }

  return false;
}
enum {H_DEBUG, H_ACTIVE, H_CHANNEL};

static String 
BRN2BeaconSource_read_param(Element *e, void *thunk)
{
  BRN2BeaconSource *td = (BRN2BeaconSource *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_ACTIVE:
    return String(td->_active) + "\n";
  case H_CHANNEL:
    return String(td->_winfo->_channel) + "\n";
  default:
    return String();
  }
}
static int 
BRN2BeaconSource_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BRN2BeaconSource *f = (BRN2BeaconSource *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_ACTIVE:
    bool active;
    if (!cp_bool(s, &active)) 
      return errh->error("active parameter must be boolean");
    f->_active = active;
    break;
  case H_CHANNEL:
    unsigned new_channel;
    unsigned countdown;
    Vector<String> args;

    cp_spacevec(s, args);

    if (!cp_unsigned(args[0], &new_channel))
      return errh->error("channel parameter must be unsigned integer");
    else
      f->_target_channel = new_channel;

    if (args.size() == 2 ) {
      if (!cp_unsigned(args[1], &countdown))
        return errh->error("countdown parameter must be unsigned integer");
    } else
      countdown = 2;

    f->_switch_channel_countdown = countdown;

    break;
  }
  return 0;
}

void
BRN2BeaconSource::add_handlers()
{
  add_read_handler("debug", BRN2BeaconSource_read_param, (void *) H_DEBUG);
  add_read_handler("active", BRN2BeaconSource_read_param, (void *) H_ACTIVE);
  add_read_handler("channel", BRN2BeaconSource_read_param, (void *) H_CHANNEL);

  add_write_handler("debug", BRN2BeaconSource_write_param, (void *) H_DEBUG);
  add_write_handler("active", BRN2BeaconSource_write_param, (void *) H_ACTIVE);
  add_write_handler("channel", BRN2BeaconSource_write_param, (void *) H_CHANNEL);

}

#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2BeaconSource)

