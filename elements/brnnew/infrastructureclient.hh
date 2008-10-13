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

#ifndef INFRASTRUCTURECLIENTELEMENT_HH
#define INFRASTRUCTURECLIENTELEMENT_HH

#include <click/bighashmap.hh>
#include <click/dequeue.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <click/timer.hh>
#include <click/string.hh>
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
#include <elements/wifi/wifiencap.hh>
#include "brnbeaconscanner.hh"
#include <elements/wifi/station/proberequester.hh>
#include <elements/wifi/station/openauthrequester.hh>
#include <elements/wifi/station/associationrequester.hh>
#include "nodeidentity.hh"
#include "brnettmetric.hh"
#include "brnetxmetric.hh"
#include <click/timestamp.hh>

CLICK_DECLS

class InfrastructureClient : public Element {

public:
  InfrastructureClient();
  ~InfrastructureClient();

  const char *class_name() const    { return "InfrastructureClient"; }
  const char *port_count() const    { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void run_timer(Timer* );

  void add_handlers();

  int find_best_ap();

  int send_probe_to_ap();
  int send_auth_to_ap();
  int send_assoc_to_ap();

  String wireless_info();
  String print_assoc();

  WirelessInfo *_wireless_info;
  AvailableRates *_rtable;
  BRNBeaconScanner *_beaconscanner;

  ProbeRequester *_probereq;
  OpenAuthRequester *_authreq;
  AssociationRequester *_assocreq;
  WifiEncap *_wifiencap;

  Timer request_timer;

  String _ssid;

  bool _auth;
  bool _ap_available;

  int _debug;

  bool _ad_hoc;

};

CLICK_ENDDECLS
#endif
