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

#ifndef BRN2INFRASTRUCTURECLIENTELEMENT_HH
#define BRN2INFRASTRUCTURECLIENTELEMENT_HH

#include <click/bighashmap.hh>
#include <click/deque.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
#include <click/string.hh>
#include <click/timestamp.hh>

#include <elements/wifi/wirelessinfo.hh>
#include <elements/wifi/wifiencap.hh>
#include <elements/wifi/station/openauthrequester.hh>

#include "elements/brn/wifi/brnavailablerates.hh"
#include "elements/brn/routing/identity/brn2_device.hh"
#include "elements/brn/wifi/station/brn2assocrequester.hh"
#include "elements/brn/wifi/station/brn2proberequester.hh"
#include "elements/brn/wifi/availablechannels.hh"

#include "brn2beaconscanner.hh"

CLICK_DECLS

/**
 * Status: Not Finished
 * TODO: remove "nodeidentity.hh"-include
 *       replace ath2operation using writehandler
 * Resp: robert
*/

class BRN2InfrastructureClient : public Element {

public:
  BRN2InfrastructureClient();
  ~BRN2InfrastructureClient();

  const char *class_name() const    { return "BRN2InfrastructureClient"; }
  const char *port_count() const    { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void run_timer(Timer* );

  void add_handlers();

  int find_best_ap();

  int send_probe_to_ap();
  int send_auth_to_ap();
  int send_assoc_to_ap();
  int send_disassoc_to_ap();
  int start_assoc();

  String wireless_info();
  String print_assoc();
  
  BRN2Device *_device;
  
  WirelessInfo *_wireless_info;
  BrnAvailableRates *_rtable;
  BRN2BeaconScanner *_beaconscanner;

  BrnProbeRequester *_probereq;
  OpenAuthRequester *_authreq;
  BRN2AssocRequester *_assocreq;
  WifiEncap *_wifiencap;

  Timer request_timer;

  String _ssid;

  bool _auth;
  bool _ap_available;

  bool _ad_hoc;

  bool _active_scan_mode;

  bool _scan_all_channels;
  int _channel_index;
  AvailableChannels *_channellist;
  int _channel_is_set;

  int _minChannelScanTime;
  int _maxChannelScanTime;

  int _debug;
};

CLICK_ENDDECLS
#endif
