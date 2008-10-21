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
 * brnassocresponder.{cc,hh} -- responds to stations assoc requests
 * A. Zubow
 */

#include <click/config.h>
#include "elements/brn/common.hh"

#include "brnassocresponder.hh"

#include <click/confparse.hh>
#include <click/straccum.hh>
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
#include "elements/brn/wifi/ap/iapp/brniappstationtracker.hh"
#include "elements/brn/standard/brnpacketanno.hh"
CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////
class DelayedResponse { public:
  bool reassoc;
  uint16_t status;
  uint16_t associd;
  EtherAddress src;
  EtherAddress current_ap;
  String device;
  String ssid;
};

////////////////////////////////////////////////////////////////////////////////

BRNAssocResponder::BRNAssocResponder() : 
//  _client_assoc_lst(),
  _debug(BrnLogger::DEFAULT),
  _response_delay_ms(0),
  _iapp(NULL),
  _brn_vlan(NULL),
  _response_timer(static_response_timer_hook, this)
{
}

////////////////////////////////////////////////////////////////////////////////

BRNAssocResponder::~BRNAssocResponder()
{
}

////////////////////////////////////////////////////////////////////////////////

int
BRNAssocResponder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _debug = false;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpInteger, "Debug", &_debug,
      "RESPONSE_DELAY", cpInteger, "Response delay in ms", &_response_delay_ms,
		  "WIRELESS_INFO", cpElement, "wireless_info", &_winfo,
		  "RT", cpElement, "availablerates", &_rtable,
      "IAPP", cpElement, "IAPP element", &_iapp,
		  "BRNVLAN", cpElement, "brn vlans", &_brn_vlan,
		  cpEnd) < 0)
    return -1;

  if (!_rtable || _rtable->cast("AvailableRates") == 0) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");

  if (!_winfo || _winfo->cast("WirelessInfo") == 0) 
    return errh->error("WirelessInfo element is not provided or not a WirelessInfo");

  if (_iapp && _iapp->cast("BrnIappStationTracker") == 0)
    return errh->error("IAPP element is not provided or not a BrnIappStationTracker");

  if (_brn_vlan && _brn_vlan->cast("BRNVLAN") == NULL)
    return errh->error("BRNVLAN element not a BRNVLAN");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BRNAssocResponder::initialize(ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);

  _response_timer.initialize(this);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void
BRNAssocResponder::push(int, Packet *p)
{
  BRN_CHECK_EXPR_RETURN(NULL == p || p->length() < sizeof(struct click_wifi),
    ("invalid packet"), if (p) p->kill();return;);

  struct click_wifi *w = (struct click_wifi *) p->data();
  EtherAddress addr1(w->i_addr1);  // final dst
  EtherAddress addr3(w->i_addr3);  // receiver

  // check type management
  uint8_t type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  uint8_t subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;
  BRN_CHECK_EXPR_RETURN(type != WIFI_FC0_TYPE_MGT,
    ("received non-management packet"), if (p) p->kill();return;);
  
  // Filter all packets not for us
  BRN_CHECK_EXPR_RETURN(_winfo->_bssid != addr1 || _winfo->_bssid != addr3,
    ("received wrong packet for %s with bssid %s", addr1.unparse().c_str(),
    addr3.unparse().c_str()), p->kill(); return;);

  // From here up the packet is for us, packets for others are filtered.
  // look for subtype 
  switch(subtype) {
  case WIFI_FC0_SUBTYPE_REASSOC_REQ:
  case WIFI_FC0_SUBTYPE_ASSOC_REQ:
    recv_association_request(p, subtype);
    break;
  case WIFI_FC0_SUBTYPE_ASSOC_RESP:
  case WIFI_FC0_SUBTYPE_REASSOC_RESP:
    recv_association_response(p, subtype);
    break;
  case WIFI_FC0_SUBTYPE_DISASSOC:
    recv_disassociation(p);
    break;
  default:
    BRN_ERROR("received management frame with unkown subtype %d", subtype);
    break;
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocResponder::recv_association_response(
  Packet *p, uint8_t)
{
  BRN_WARN("received own (re)assocation response, discard."); 
  p->kill();
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocResponder::recv_disassociation(
  Packet *p)
{
  struct click_wifi *w = (struct click_wifi *) p->data();
  EtherAddress addr2(w->i_addr2);  // sta

  BRN_DEBUG("received disassocation from %s", addr2.unparse().c_str());

  // Remember the disassoc
  if ( _iapp != NULL ) {
    // get station's ssid
    String ssid = _iapp->_assoc_list->get_ssid(addr2);

    _iapp->sta_disassociated(addr2);

    uint16_t vid = _brn_vlan->get_vlan(ssid);
    BRNVLAN::VLAN* vlan = _brn_vlan->get_vlan(vid);
    if (vlan != NULL)
      vlan->remove_member(addr2);
    else
      BRN_ERROR("Client %s was associated with ssid %s, but no VLAN known for this SSID.", addr2.unparse().c_str(), ssid.c_str());
  }

  p->kill();
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocResponder::recv_association_request(Packet *p, uint8_t subtype)
{
  // saves packet's device annotation
  String device = String(BRNPacketAnno::udevice_anno(p));
  bool reassoc = (subtype == WIFI_FC0_SUBTYPE_REASSOC_REQ) ? true : false;
  EtherAddress current_ap;

  struct click_wifi *w = (struct click_wifi *) p->data();
  uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  /*capabilty */
  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  /* listen interval */
  uint16_t lint = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  /* current access point */
  if (true == reassoc) {
    current_ap = EtherAddress(ptr);
    ptr += 6;
  }

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
      BRN_DEBUG(" ignored element id %u %u \n", *ptr, ptr[1]);
    }
    ptr += ptr[1] + 2;
  }

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

  String ssid;
  String my_ssid = _winfo ? _winfo->_ssid : "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, min((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  } else {
    /* there was no element or it has zero length */
    ssid = "";
  }

    /* respond to blank ssid probes also */
  //if ((ssid != "" && ssid != my_ssid) || (_brn_vlan && _brn_vlan->is_vlan(ssid))) {

  if (_brn_vlan != NULL) {
    if (ssid != "" && ssid != my_ssid && !_brn_vlan->is_vlan(ssid)) {
        BRN_WARN(" other ssid %s wanted %s", ssid.c_str(), my_ssid.c_str());
        p->kill();
        return;
    }
  }
  else {
    if (ssid != "" && ssid != my_ssid) {
        BRN_WARN(" other ssid %s wanted %s", ssid.c_str(), my_ssid.c_str());
        p->kill();
        return;
    }
  }

  StringAccum sa;

  EtherAddress dst = EtherAddress(w->i_addr1);
  EtherAddress src = EtherAddress(w->i_addr2);
  EtherAddress bssid = EtherAddress(w->i_addr3);

  sa << "src " << src;
  sa << " dst " << dst;
  sa << " bssid " << bssid;
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

  sa << " listen_int " << lint << " ";

  if (true == reassoc) {
    sa << "current AP " << current_ap << " ";
  }

  sa << "( { ";
  for (int x = 0; x < basic_rates.size(); x++) {
    sa << basic_rates[x] << " ";
  }
  sa << "} ";
  for (int x = 0; x < rates.size(); x++) {
    sa << rates[x] << " ";
  }

  sa << ")\n";

  BRN_INFO("%s request %s", reassoc ? "reassoc" : "assoc" , sa.take_string().c_str());

  uint16_t associd = 0xc000 | _associd++;

  BRN_DEBUG("association %s associd %d", src.unparse().c_str(), associd);
  p->kill();

  uint16_t status = WIFI_STATUS_SUCCESS;

  if (0 < _response_delay_ms) {
    DelayedResponse* resp = new DelayedResponse();
    resp->reassoc = reassoc;
    resp->status = status;
    resp->associd = associd;
    resp->src = src;
    resp->current_ap = current_ap;
    resp->device = device;
    resp->ssid = ssid;

    _responses.push_back(resp);
    _response_timer.schedule_after_msec(_response_delay_ms);
  }
  else
  {
    if (false == reassoc)
      send_association_response(src, status, associd);
    else
      send_reassociation_response(src, status, associd);

    if (status == WIFI_STATUS_SUCCESS) {
      BRN_DEBUG("successfully associated; %s\n", src.unparse().c_str());

      // trigger handover
      if ( _iapp != NULL )
        _iapp->sta_associated(src, _winfo->_bssid, current_ap, device, ssid);

      if ( _brn_vlan != NULL ) {
        // add station to vlan
        uint16_t vid = _brn_vlan->get_vlan(ssid);
        BRNVLAN::VLAN* vlan = _brn_vlan->get_vlan(vid);
        if (vlan != NULL)
          vlan->add_member(src);
        else
          BRN_ERROR("Client %s is associated with ssid %s, but no VLAN known for this SSID.", src.unparse().c_str(), ssid.c_str());
      }
    } else {
      BRN_DEBUG("association failed.\n");
    }
  }

  return;
}

////////////////////////////////////////////////////////////////////////////////

void
BRNAssocResponder::send_association_response(
  EtherAddress dst, uint16_t status, uint16_t associd)
{
  AssociationResponder::send_association_response(dst, status, associd);
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocResponder::send_reassociation_response(
    EtherAddress dst, uint16_t status, uint16_t associd)
{
  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();
  Vector<int> rates = _rtable->lookup(bssid);
  int max_len = sizeof (struct click_wifi) + 
    2 +                  /* cap_info */
    2 +                  /* status  */
    2 +                  /* assoc_id */
    2 + WIFI_RATES_MAXSIZE +  /* rates */
    2 + WIFI_RATES_MAXSIZE +  /* xrates */
    0;

  WritablePacket *p = Packet::make(max_len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_REASSOC_RESP;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, dst.data(), 6);
  memcpy(w->i_addr2, bssid.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);

  *(uint16_t *) w->i_dur = 0;
  *(uint16_t *) w->i_seq = 0;

  uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
  int actual_length = sizeof(struct click_wifi);

  uint16_t cap_info = 0;
  cap_info |= WIFI_CAPINFO_ESS;
  *(uint16_t *)ptr = cpu_to_le16(cap_info);
  ptr += 2;
  actual_length += 2;

  *(uint16_t *)ptr = cpu_to_le16(status);
  ptr += 2;
  actual_length += 2;

  *(uint16_t *)ptr = cpu_to_le16(associd);
  ptr += 2;
  actual_length += 2;

  /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = min(WIFI_RATE_SIZE, rates.size());
  for (int x = 0; x < min (WIFI_RATE_SIZE, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x];
    if (rates[x] == 2) {
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
    }
    ptr += 2 + num_xrates;
    actual_length += 2 + num_xrates;
  }

  p->take(max_len - actual_length);

  output(0).push(p);
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocResponder::static_response_timer_hook(Timer *, void *v)
{
  BRNAssocResponder *rt = (BRNAssocResponder*)v;
  rt->response_timer_hook();
}

////////////////////////////////////////////////////////////////////////////////

void 
BRNAssocResponder::response_timer_hook()
{
  DelayedResponse* resp = _responses.front();
  _responses.pop_front();
  
  if (false == resp->reassoc)
    send_association_response(resp->src, resp->status, resp->associd);
  else
    send_reassociation_response(resp->src, resp->status, resp->associd);

  if (resp->status == WIFI_STATUS_SUCCESS) {
    BRN_DEBUG("successfully associated; %s\n", resp->src.unparse().c_str());

    // trigger handover
    if ( _iapp != NULL )
     _iapp->sta_associated(resp->src, _winfo->_bssid, resp->current_ap, resp->device, resp->ssid);
  } else {
    BRN_DEBUG("association failed.\n");
  }
  
  delete resp;
}

////////////////////////////////////////////////////////////////////////////////

void
BRNAssocResponder::send_disassociation(EtherAddress dst, uint16_t reason)
{
  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();
  int len = sizeof (struct click_wifi) + 
    2 +                  /* reason */
    0;

  WritablePacket *p = Packet::make(len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_DISASSOC;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, dst.data(), 6);
  memcpy(w->i_addr2, bssid.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);

  *(uint16_t *) w->i_dur = 0;
  *(uint16_t *) w->i_seq = 0;

  uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  *(uint16_t *)ptr = cpu_to_le16(reason);
  ptr += 2;
  
  /*
  // TODO may be to late here, because dst has no ssid anymore; already removed from assoclistRbrnvlan
  // remove station from vlan
  String ssid = _iapp->_assoc_list->get_ssid(dst);
  uint16_t vid = _brn_vlan->get_vlan(ssid);
  BRNVLAN::VLAN* vlan = _brn_vlan->get_vlan(vid);
  if (vlan != NULL)
    vlan->remove_member(dst);
  else
    BRN_ERROR("Client %s was associated with ssid %s, but no VLAN known for this SSID.", dst.unparse().c_str(), ssid.c_str());
  */
  
  output(0).push(p);
}

enum {H_DEBUG, H_DELAY};

static String
read_debug_param(Element *e, void *vparam)
{
  BRNAssocResponder *f = (BRNAssocResponder *)e;
  switch((intptr_t)vparam) {
  case H_DEBUG:
    return String(f->_debug) + "\n";
  case H_DELAY:
    return String(f->_response_delay_ms) + "\n";
  default:
    return String("internal error") + "\n";
  }
  return String();
}

static int 
write_debug_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BRNAssocResponder *f = (BRNAssocResponder *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG:
  {
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be an integer value between 0 and 4");
    f->_debug = debug;
    break;
  }
  case H_DELAY:
  {
    int response_delay_ms;
    if (!cp_integer(s, &response_delay_ms)) 
      return errh->error("response_delay_ms parameter must be an integer");
    f->_response_delay_ms = response_delay_ms;
    break;
  }
  default:
  {
    return errh->error("internal error in BRNAssocResponder/write_debug_param");
  }
  }
  return 0;
}

void 
BRNAssocResponder::add_handlers()
{
  add_read_handler("response_delay", read_debug_param, (void*)H_DELAY);
  add_write_handler("response_delay", write_debug_param, (void*)H_DELAY);

  add_read_handler("debug", read_debug_param, (void*)H_DEBUG);
  add_write_handler("debug", write_debug_param, (void*)H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

#include <click/dequeue.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(BRNAssocResponder)

////////////////////////////////////////////////////////////////////////////////
