/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * brnassocrequester.{cc,hh} -- extends the click req by reassocs
 * M. Kurth
 */

#include <click/config.h>
#include <elements/brn/common.hh>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
#include "brnassocrequester.hh"
CLICK_DECLS


////////////////////////////////////////////////////////////////////////////////

BRNAssocRequester::BRNAssocRequester() :
  _debug(BrnLogger::DEFAULT), _bssid()
{
}

////////////////////////////////////////////////////////////////////////////////

BRNAssocRequester::~BRNAssocRequester()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
BRNAssocRequester::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _debug = false;
  _associated = false;
  if (cp_va_kparse(conf, this, errh,
      /* not required */
      //cpKeywords,
      "DEBUG", cpkP+cpkM, cpInteger, /*"Debug",*/ &_debug,
      "ETH", cpkP+cpkM, cpEthernetAddress, /*"eth",*/ &_eth,
      "WIRELESS_INFO", cpkP+cpkM, cpElement, /*"wirleess_info",*/ &_winfo,
      "RT", cpkP+cpkM, cpElement, /*"availablerates",*/ &_rtable,
      cpEnd) < 0)
    return -1;

  if (_debug >= BrnLogger::INFO)
    AssociationRequester::_debug=true;

  if (!_rtable || _rtable->cast("AvailableRates") == 0) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");

  if (!_winfo || _winfo->cast("WirelessInfo") == 0) 
    return errh->error("WirelessInfo element is not provided or not a WirelessInfo");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocRequester::push(int, Packet *p)
{
  uint8_t dir;
  uint8_t type;
  uint8_t subtype;

  if (p->length() < sizeof(struct click_wifi)) {
    BRN_ERROR("packet too small: %d vs %d\n",
      p->length(), sizeof(struct click_wifi));

    p->kill();
    return ;
  }

  struct click_wifi *w = (struct click_wifi *) p->data();

  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    BRN_WARN("received non-management packet.");
    p->kill();
    return ;
  }

  if (subtype == WIFI_FC0_SUBTYPE_ASSOC_RESP) {
    process_response(p);
    p->kill();
    return;
  }

  if (subtype == WIFI_FC0_SUBTYPE_REASSOC_RESP) {
    process_reassoc_resp(p);
    p->kill();
    return;
  }

  if (subtype == WIFI_FC0_SUBTYPE_DISASSOC) {
    process_disassociation(p);
    p->kill();
    return;
  }

  BRN_DEBUG("received non-assoc response packet.");
  p->kill();
  return ;
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocRequester::send_reassoc_req()
{
  if (!_bssid) {
    BRN_ERROR("unknown bssid, no reassoc possible.");
    return;
  }
  if (!_associated) {
    BRN_WARN("not associated, sending assoc instead of reassoc");
    send_assoc_req();
  }

  /*
   * Format of the reassociation request body (802.11 spec 7.2.3.6)
   * 1 Capability information
   * 2 Listen interval
   * 3 Current AP address
   * 4 SSID
   * 5 Supported rates
  */

  _associated = false;
  EtherAddress bssid = _bssid;
  EtherAddress newAp = _winfo ? _winfo->_bssid : EtherAddress();
  String ssid = _winfo ? _winfo->_ssid : "";
  int linterval = _winfo ? _winfo->_interval : 1;
  Vector<int> rates = _rtable->lookup(newAp);
  int max_len = sizeof (struct click_wifi) + 
    2 + /* cap_info */
    2 + /* listen_int */
    2 + ssid.length() +
    2 + WIFI_RATES_MAXSIZE +  /* rates */
    2 + WIFI_RATES_MAXSIZE +  /* xrates */
    0;

  WritablePacket *p = Packet::make(max_len);
  if(p == 0) {
    BRN_FATAL("out of memory");
    return;
  }
  
  if (!rates.size()) {
    BRN_WARN(" couldn't lookup rates for %s\n", newAp.unparse().c_str());
  }

  struct click_wifi *w = (struct click_wifi *) p->data();
  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_REASSOC_REQ;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  w->i_dur[0] = 0;
  w->i_dur[1] = 0;

  w->i_seq[0] = 0;
  w->i_seq[1] = 0;

  // Use mac address and bssid of the new ap like madwifi does
  // see madwifi:ieee80211_output.c ieee80211_mgmt_output()
  memcpy(w->i_addr1, newAp.data(), 6);
  memcpy(w->i_addr2, _eth.data(), 6);
  memcpy(w->i_addr3, newAp.data(), 6);

  uint8_t *ptr = (uint8_t *)  p->data() + sizeof(click_wifi);
  int actual_length = sizeof (struct click_wifi);

  uint16_t capability = 0;
  capability |= WIFI_CAPINFO_ESS;
  if (_winfo && _winfo->_wep) {
    capability |= WIFI_CAPINFO_PRIVACY;
  }

  /* capability */
  *(uint16_t *) ptr = cpu_to_le16(capability);
  ptr += 2;
  actual_length += 2;

  /* listen_int */
  *(uint16_t *) ptr = cpu_to_le16(linterval);
  ptr += 2;
  actual_length += 2;

  /* Former AP */
  memcpy(ptr, bssid.data(), 6);
  ptr += 6;

  /* SSID */
  ptr[0] = WIFI_ELEMID_SSID;
  ptr[1] = ssid.length();
  ptr += 2;
  actual_length += 2;

  memcpy(ptr, ssid.c_str(), ssid.length());
  ptr += ssid.length();
  actual_length += ssid.length();

  /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = min(WIFI_RATE_SIZE, rates.size());
  for (int x = 0; x < min (WIFI_RATE_SIZE, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x];

    if (rates[x] == 2) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }

    if (_winfo && _winfo->_channel > 15 && rates[x] == 12) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }
  }
  ptr += 2 + min(WIFI_RATE_SIZE, rates.size());
  actual_length += 2 + min(WIFI_RATE_SIZE, rates.size());

  int num_xrates = rates.size() - WIFI_RATE_SIZE;
  if (num_xrates > 0) {
    /* rates */
    ptr[0] = WIFI_ELEMID_XRATES;
    ptr[1] = num_xrates;
    for (int x = 0; x < num_xrates; x++) {
      ptr[2 + x] = (uint8_t) rates[x + WIFI_RATE_SIZE];
      if (rates[x + WIFI_RATE_SIZE] == 2) {
        ptr [2 + x] |= WIFI_RATE_BASIC;
      }
      if (_winfo && _winfo->_channel > 15 && rates[x] == 12) {
        ptr [2 + x] |= WIFI_RATE_BASIC;
      }
    }
    ptr += 2 + num_xrates;
    actual_length += 2 + num_xrates;
  }

  // each probe packet is annotated with a timestamp
  struct timeval now;
  now = Timestamp::now().timeval();
  p->set_timestamp_anno(now);

  p->take(max_len - actual_length);
  output(0).push(p);
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocRequester::process_response(Packet *p) 
{
  struct click_wifi *w = (struct click_wifi *) p->data();
  EtherAddress bssid = EtherAddress(w->i_addr3);
  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t status = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t associd = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint8_t *rates_l = ptr;

  Vector<int> basic_rates;
  Vector<int> rates;
  Vector<int> all_rates;
  if (rates_l) {
    for (int x = 0; x < min((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
      uint8_t rate = rates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
        basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
        rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
      all_rates.push_back((int)(rate & WIFI_RATE_VAL));
    }
  }

  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->anno();//p->all_user_anno();
  StringAccum sa;
  sa << bssid << " ";
  int rssi = ceh->rssi;
  sa << "+" << rssi << " ";

  sa << "[ ";
  if (capability & WIFI_CAPINFO_ESS) {
    sa << "ESS ";
  }
  if (capability & WIFI_CAPINFO_IBSS) {
    sa << "IBSS ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLABLE) {
    sa << "CF_POLLABLE ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLREQ) {
    sa << "CF_POLLREQ ";
  }
  if (capability & WIFI_CAPINFO_PRIVACY) {
    sa << "PRIVACY ";
  }
  sa << "] ";

  sa << "status " << status << " ";
  sa << "associd " << associd << " ";

  sa << "( { ";
  for (int x = 0; x < basic_rates.size(); x++) {
    sa << basic_rates[x] << " ";
  }
  sa << "} ";
  for (int x = 0; x < rates.size(); x++) {
    sa << rates[x] << " ";
  }

  sa << ")";
  BRN_DEBUG("* response %s", sa.take_string().c_str());

  if (_rtable) {
    _rtable->insert(bssid, all_rates);
  }
  if (status == 0) {
    _associated = true;
    _bssid = bssid;
    BRN_INFO("associated with bssid %s", bssid.unparse().c_str());
  }
  return;
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocRequester::process_reassoc_resp(Packet *p)
{
  struct click_wifi *w = (struct click_wifi *) p->data();
  EtherAddress bssid = EtherAddress(w->i_addr3);
  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t status = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t associd = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint8_t *rates_l = ptr;

  Vector<int> basic_rates;
  Vector<int> rates;
  Vector<int> all_rates;
  if (rates_l) {
    for (int x = 0; x < min((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
      uint8_t rate = rates_l[x + 2];
      if (rate & WIFI_RATE_BASIC) {
        basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
        rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
      all_rates.push_back((int)(rate & WIFI_RATE_VAL));
    }
  }

  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->anno(); //p->all_user_anno();
  StringAccum sa;
  sa << bssid << " ";
  int rssi = ceh->rssi;
  sa << "+" << rssi << " ";

  sa << "[ ";
  if (capability & WIFI_CAPINFO_ESS) {
    sa << "ESS ";
  }
  if (capability & WIFI_CAPINFO_IBSS) {
    sa << "IBSS ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLABLE) {
    sa << "CF_POLLABLE ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLREQ) {
    sa << "CF_POLLREQ ";
  }
  if (capability & WIFI_CAPINFO_PRIVACY) {
    sa << "PRIVACY ";
  }
  sa << "] ";

  sa << "status " << status << " ";
  sa << "associd " << associd << " ";

  sa << "( { ";
  for (int x = 0; x < basic_rates.size(); x++) {
    sa << basic_rates[x] << " ";
  }
  sa << "} ";
  for (int x = 0; x < rates.size(); x++) {
    sa << rates[x] << " ";
  }

  sa << ")";

  BRN_DEBUG("* response %s", sa.take_string().c_str());

  if (_rtable) {
    _rtable->insert(bssid, all_rates);
  }

  if (status == 0) {
    _associated = true;
    _bssid = bssid;
    BRN_INFO("reassociated with bssid %s", bssid.unparse().c_str());
  }
  return;
}

////////////////////////////////////////////////////////////////////////////////

enum {H_SEND_REASSOC_REQ};

static int 
BRNAssocRequester_write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);

  BRNAssocRequester *f = (BRNAssocRequester *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_SEND_REASSOC_REQ: {
    f->send_reassoc_req();
  }
  }
  return 0;
}

static String
read_debug_param(Element *e, void *)
{
  BRNAssocRequester *f = (BRNAssocRequester *)e;
  return String(f->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRNAssocRequester *f = (BRNAssocRequester *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  f->_debug = debug;
  if (f->_debug >= BrnLogger::INFO)
    ((AssociationRequester*)f)->_debug=true;
  else
    ((AssociationRequester*)f)->_debug=false;
  return 0;
}

void 
BRNAssocRequester::add_handlers()
{

  AssociationRequester::add_handlers();

  add_read_handler("debug", read_debug_param, 0);

  add_write_handler("debug", write_debug_param, 0);
  add_write_handler("send_reassoc_req", 
    BRNAssocRequester_write_param, (void *) H_SEND_REASSOC_REQ);
}

////////////////////////////////////////////////////////////////////////////////

#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(BRNAssocRequester)

////////////////////////////////////////////////////////////////////////////////
