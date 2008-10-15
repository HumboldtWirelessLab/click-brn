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
 * hostwififilter.{cc,hh}
 *
 */

#include <click/config.h>
#include "common.hh"

#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>

#include "hostwififilter.hh"
#include <elements/wifi/wirelessinfo.hh>


CLICK_DECLS

HostWifiFilter::HostWifiFilter() :
  _debug(BrnLogger::DEFAULT),
  _mode(STRICT),
  _not_init(0),
  _winfo(NULL)
{
}

HostWifiFilter::~HostWifiFilter()
{
}

int
HostWifiFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
        cpEthernetAddress, "Ethernet address", &_addr,
        cpElement, "wirleess_info", &_winfo,
      /* not required */
      cpKeywords,
        "MODE", cpInteger, "Mode", &_mode,
        "DEBUG", cpInteger, "Debug", &_debug,
      cpEnd) < 0)
    return -1;

  if (!_winfo && _winfo->cast("WirelessInfo") == 0)
    return errh->error("WIRELESS_INFO element is not a WirelessInfo");

  if (!_addr || _addr.is_broadcast() || _addr.is_group())
    return errh->error("Invalid ethernet address given (%s).", _addr.s().c_str());

  if (STRICT != _mode
    && NOADHOCBSSID != _mode
    && STRICT_INIT != _mode
    && NOBSSID != _mode
    && PROMISC != _mode)
    return errh->error("invalid mode.");
    
  return 0;
}

void 
HostWifiFilter::push(int, Packet *p)
{
  BRN_CHECK_EXPR_RETURN(NULL == p,
    ("invalid argument"), return;);
  
  struct click_wifi *w = (struct click_wifi *) p->data();

  // check version 
  if(WIFI_FC0_VERSION_0 != (w->i_fc[0] & WIFI_FC0_VERSION_MASK)) {
    BRN_ERROR("cannot handle version %d", w->i_fc[0] & WIFI_FC0_VERSION_MASK);
    p->kill();
    return;
  }
  
  // Kill all control messages
  uint8_t type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  uint8_t dir  = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  if (WIFI_FC0_TYPE_CTL == type) {
    BRN_DEBUG("killing control message.");
    p->kill();
    return;
  }

  // check length
  BRN_CHECK_EXPR_RETURN(p->length() < sizeof(click_wifi),
    ("got too small packet ... discard."), p->kill();return;);

  // If monitor, simply return
  if (PROMISC == _mode) {
    output(0).push(p);
    return;
  }

  // check receiver address
  EtherAddress addr_receiver(w->i_addr1);
  if (_addr != addr_receiver
    && !addr_receiver.is_broadcast()
    && !addr_receiver.is_group()) {
    BRN_DEBUG("packet with receiver address %s send to output 1", 
      addr_receiver.s().c_str());
    checked_output_push(1, p);
    return;
  }
  
  // if in mode NOADHOCBSSID, do not check bssid (broadcasts are sent out on both ports)
  if (NOADHOCBSSID == _mode) {
    if (addr_receiver.is_broadcast())
      checked_output_push(1, p->clone());
    output(0).push(p);
    return; 
  }

  // check bssid
  EtherAddress bssid;
  switch (dir) {
  case WIFI_FC1_DIR_NODS:
    bssid = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(w->i_addr1);
    break;
  case WIFI_FC1_DIR_FROMDS:
    bssid = EtherAddress(w->i_addr2);
    break;
  case WIFI_FC1_DIR_DSTODS:
    bssid = EtherAddress(w->i_addr3);
    break;
  }
  
  if (!bssid) {
    BRN_WARN("unable to determine bssid (a1=%s, a2=%s, a3=%s).", 
      EtherAddress(w->i_addr1).s().c_str(),
      EtherAddress(w->i_addr2).s().c_str(), 
      EtherAddress(w->i_addr3).s().c_str()), 

    p->kill();
    return;
  }

  if (_winfo->_bssid == bssid) {
    output(0).push(p);
    return; 
  }

  // if bssid is broadcast, duplicate and send out to both ports
  // if in full mode and not initialized, send packet to both outputs...
  if (bssid.is_broadcast() 
    ||!_winfo->_bssid && STRICT_INIT == _mode) {
    if (!_winfo->_bssid && MESSAGE_INTERVAL <= ++_not_init) {
      _not_init = 0;
      BRN_WARN("Got another %d packets but missing my bssid.", MESSAGE_INTERVAL);
    }

    output(0).push(p->clone());
    checked_output_push(1, p);
    return; 
  }
  
  // If not active, simply print out a debug message
  if (NOBSSID == _mode) {
    BRN_INFO("packet with wrong bssid %s", bssid.s().c_str());

    output(0).push(p);
    return; 
  }
  
  BRN_DEBUG("packet with bssid %s send to output 1", bssid.s().c_str());
  checked_output_push(1, p);
}


enum {H_DEBUG,H_MODE};

static String 
read_param(Element *e, void *thunk)
{
  HostWifiFilter *td = (HostWifiFilter *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_MODE:
    return String(td->_mode) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  HostWifiFilter *f = (HostWifiFilter *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be an integer value between 0 and 4");
    f->_debug = debug;
    break;
  }
  case H_MODE: {    //debug
    int mode;
    if (!cp_integer(s, &mode)) 
      return errh->error("mode parameter must be boolean");
    if (HostWifiFilter::STRICT != mode
      && HostWifiFilter::STRICT_INIT != mode
      && HostWifiFilter::NOADHOCBSSID != mode
      && HostWifiFilter::NOBSSID != mode
      && HostWifiFilter::PROMISC != mode)
      return errh->error("invalid mode.");
    f->_mode = mode;
    break;
  }
  }
  return 0;
}
 
void
HostWifiFilter::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
  
  add_read_handler("mode", read_param, (void *) H_MODE);
  add_write_handler("mode", write_param, (void *) H_MODE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(HostWifiFilter)
