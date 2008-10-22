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

#include <click/config.h>

#include <click/bighashmap.hh>
#include <click/dequeue.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
#include <click/string.hh>
#include <click/confparse.hh>
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
#include <elements/wifi/wifiencap.hh>
#include "brnbeaconscanner.hh"
#include <elements/wifi/station/associationrequester.hh>
#include <elements/wifi/station/openauthrequester.hh>
#include "infrastructureclient.hh"
#include <click/timestamp.hh>

CLICK_DECLS

InfrastructureClient::InfrastructureClient()
:request_timer(this),
    _debug(BrnLogger::DEFAULT)
{
}

InfrastructureClient::~InfrastructureClient()
{
}

int
InfrastructureClient::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String probes;
  if (cp_va_kparse(conf, this, errh,
      "WIRELESS_INFO", cpkP+cpkM, cpElement, /*"Wirelessinfo",*/ &_wireless_info,
      "RT", cpkP+cpkM, cpElement, /*"AvailabeRates",*/ &_rtable,
      "BEACONSCANNER", cpkP+cpkM, cpElement, /*"Beaconscanner",*/ &_beaconscanner,
      "PROBE_REQUESTER", cpkP+cpkM, cpElement, /*"ProbeRequester",*/ &_probereq,
      "AUTH_REQUESTER", cpkP+cpkM, cpElement, /*"AuthRequester",*/ &_authreq,
      "ASSOC_REQUESTER", cpkP+cpkM, cpElement, /*"AssocRequester",*/ &_assocreq,
      "WIFIENCAP", cpkP+cpkM, cpElement, /*"Wifiencap",*/ &_wifiencap,
      "DEBUG", cpkP, cpInteger, /*"debug",*/ &_debug,
      cpEnd) < 0)

      return -1;

  if (!_wireless_info || !_wireless_info->cast("WirelessInfo")) 
    return errh->error("WirelessInfo element is not provided or not a WirelessInfo");

  if (!_rtable || !_rtable->cast("AvailableRates")) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");

  if (!_beaconscanner || !_beaconscanner->cast("BRNBeaconScanner")) 
    return errh->error("BeaconScanner element is not provided or not a BeaconScanner");

  if (!_probereq || !_probereq->cast("ProbeRequester")) 
    return errh->error("ProbeRequester element is not provided or not a ProbeRequester");

//  if (!_authreq || !_authreq->cast("OpenAuthRequester")) 
//    return errh->error("OpenAuthRequester element is not provided or not a OpenAuthRequester");

  if (!_assocreq || !_assocreq->cast("BRNAssocRequester")) 
    return errh->error("BRNAssocRequester element is not provided or not a BRNAssocRequester");

  if (!_wifiencap || !_wifiencap->cast("WifiEncap")) 
    return errh->error("WifiEncap element is not provided or not a WifiEncap");

  return 0;

}


int
InfrastructureClient::initialize(ErrorHandler *)
{
  request_timer.initialize(this);
  request_timer.schedule_after_msec(1000);
 
  _auth = false;
  _ap_available = false;
  _ad_hoc = false;

  return 0;
}

void
InfrastructureClient::run_timer(Timer* )
{
  if ( _wireless_info->_channel == 0 ) {
    BRN_ERROR("channel not specified, client disabled.");
    return;
  }

  if ( !_ap_available )
  {
    send_probe_to_ap();

    find_best_ap();
  }
  else
  {
    if ( _ad_hoc == true )
      _wifiencap->_mode = 0x00;
    else
      _wifiencap->_mode = 0x01;

    if ( !_auth )
    {
      send_auth_to_ap();
      _auth = true;
    }
    else
    {
      if ( ( _assocreq->_associated == false ) && ( _ad_hoc == false ) )
      {
        send_assoc_to_ap();
      }
    }
  }
 
  request_timer.schedule_after_msec(1000);
}


int
InfrastructureClient::find_best_ap()
{

  _ap_available = false;
  int rssi = 0;
  
//  click_chatter("try to find good ap");
  
  for (BRNBeaconScanner::APIter iter = _beaconscanner->_waps.begin(); iter.live(); iter++) {
    BRNBeaconScanner::wap ap = iter.value();
    
/*    click_chatter("Next AP");
    StringAccum sa;
 
    sa << "WirelessInfo:";

    sa << "AP-SSID: " << ap._ssid << " -> ";
    sa << "MY-SSID: " << _wireless_info->_ssid;
    sa << "AP-CHANNEL: " << ap._channel << " -> ";
    sa << "MY-CHANNEL: " << _wireless_info->_channel;
    sa << "AP-RSSI: " << ap._rssi << " -> ";
    sa << "BEST-RSSI: " << rssi;

    click_chatter("%s",sa.c_str());
*/
    // as long as we do not change the channel, we MUST check it!
    if ( ap._ssid == _wireless_info->_ssid 
      && _wireless_info->_channel == ap._channel
      && rssi <= ap._rssi )
    {
      _wireless_info->_bssid = ap._eth; // NOTE: _eth is the bssid
      _wireless_info->_wep = false;
      _ap_available = true;
      rssi = ap._rssi;
      _ad_hoc = (ap._capability & WIFI_CAPINFO_IBSS) ? true : false;
    }
  }

  return 0;
}



int
InfrastructureClient::send_probe_to_ap()
{
  BRN_DEBUG("Send Probe");

  _probereq->send_probe_request();
  
  return(0);
}

int
InfrastructureClient::send_auth_to_ap()
{
  BRN_DEBUG("Send Auth");

  find_best_ap();

  if ( _ap_available == true && _authreq)
  {
    _authreq->send_auth_request();
  }

  return(0);
}

int
InfrastructureClient::send_assoc_to_ap()
{
  BRN_DEBUG("Send Assoc");

  find_best_ap();

  if ( _ap_available == true )
  {
    _assocreq->send_assoc_req();
  }

  return(0);
}


String
InfrastructureClient::wireless_info()
{
  StringAccum sa;
 
  sa << "WirelessInfo [";

  sa << "ssid: " << _wireless_info->_ssid << ", ";
  sa << "bssid: " << _wireless_info->_bssid << ", ";
  sa << "channel: " << _wireless_info->_channel << ", ";
  sa << "wep: " << _wireless_info->_wep << ", ";
  sa << "ad-hoc: " << _ad_hoc << "]";


  return sa.take_string();
}

String
InfrastructureClient::print_assoc()
{
  StringAccum sa;
 
  sa << "AssocInfo: is Assoc: " << _assocreq->_associated << " ";

  return sa.take_string();
}

enum { H_WIRELESS_INFO,
       H_AUTH,
       H_ASSOC };

static String
InfrastructureClient_read_param(Element *e, void *thunk)
{
  InfrastructureClient *infstr_client = (InfrastructureClient *)e;
  
  switch ((uintptr_t) thunk)
  {
    case H_WIRELESS_INFO : return ( infstr_client->wireless_info( ) );
    case H_ASSOC : return ( infstr_client->print_assoc( ) );
    default: return String();
  }
  return String();
}

static int 
send_probe_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  InfrastructureClient *infstr_client = (InfrastructureClient *)e;

  String s = cp_uncomment(in_s);

  bool probe;
  if (!cp_bool(s, &probe)) 
    return errh->error("debug parameter must be boolean");

  if ( probe == true ) infstr_client->send_probe_to_ap();

  return 0;
}

static int 
send_auth_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  InfrastructureClient *infstr_client = (InfrastructureClient *)e;

  String s = cp_uncomment(in_s);

  bool auth;
  if (!cp_bool(s, &auth)) 
    return errh->error("debug parameter must be boolean");

  if ( auth == true ) infstr_client->send_auth_to_ap();

  return 0;
}

static int 
send_assoc_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  InfrastructureClient *infstr_client = (InfrastructureClient *)e;

  String s = cp_uncomment(in_s);

  bool assoc;
  if (!cp_bool(s, &assoc)) 
    return errh->error("debug parameter must be boolean");

  if ( assoc == true ) infstr_client->send_assoc_to_ap();

  return 0;
}

static String
read_debug_param(Element *e, void *)
{
  InfrastructureClient *de = (InfrastructureClient *)e;
  return String(de->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  InfrastructureClient *de = (InfrastructureClient *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be int");
  de->_debug = debug;
  return 0;
}

void
InfrastructureClient::add_handlers()
{
  add_read_handler("wireless_info", InfrastructureClient_read_param , (void *)H_WIRELESS_INFO);
  add_read_handler("assoc", InfrastructureClient_read_param , (void *)H_ASSOC);
  add_read_handler("debug", read_debug_param, 0);

  add_write_handler("send_probe", send_probe_handler, 0);
  add_write_handler("send_auth", send_auth_handler, 0);
  add_write_handler("send_assoc", send_assoc_handler, 0);
  add_write_handler("debug", write_debug_param, 0);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(InfrastructureClient)
