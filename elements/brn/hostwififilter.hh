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

#ifndef HOSTWIFIFILTER_HH_
#define HOSTWIFIFILTER_HH_

#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

class WirelessInfo;

#define MESSAGE_INTERVAL 100

/*
=c

HostWifiFilter

=s 

Filter packets which are not designated for this host according to modes sta and ap.
Note: kills all incoming control messages. 

available modes are STRICT and PROMISC with no and recv/bssid checks, respectively.
Furthermore there is a mode NOBSSID, i.e. only the recv addr is considered. 
The mode NOADHOCBSSID is special: Full checks are performed for all frames except
ad-hoc because the bssid in brn ad-hoc frames is bogus.
STRICT_INIT is the same as STRICT except that the bssid is not checked as long
as the internal bssid is null (should only happen during bootstrap of a client).
=d 

*/
class HostWifiFilter : public Element
{
public:
  enum {
    STRICT       = 0,  ///< check receiver address and bssid
    STRICT_INIT  = 1,  ///< check receiver address and bssid, but allow initialization 
    NOADHOCBSSID = 2,  ///< check only receiver address for ad-hoc frames
    NOBSSID      = 3,  ///< check only receiver address
    PROMISC      = 4   ///< do not filter anything
  };
public:
	HostWifiFilter();
	virtual ~HostWifiFilter();

  const char *class_name() const  { return "HostWifiFilter"; }
  const char *port_count() const  { return "1/1-2"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();

  void push(int port, Packet *p);

  int _debug;
  int _mode;
  int _not_init; ///< count packets arrived and not initialized
  WirelessInfo* _winfo;
  EtherAddress  _addr;
  
};

CLICK_ENDDECLS
#endif /*HOSTWIFIFILTER_HH_*/
