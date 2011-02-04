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
 * LPRLinkProbeHandler.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn2/standard/compression/lzw.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "lprlinkprobehandler.hh"
#include "lprprotocol.hh"


CLICK_DECLS

LPRLinkProbeHandler::LPRLinkProbeHandler()
  : _etx_metric(0),
    _debug(BrnLogger::DEFAULT),
    _active(false)
{
}

LPRLinkProbeHandler::~LPRLinkProbeHandler()
{
}

int
LPRLinkProbeHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
      "ETXMETRIC", cpkP , cpElement, &_etx_metric,
      "ACTIVE", cpkP , cpBool, &_active,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
handler(void *element, EtherAddress */*ea*/, char *buffer, int size, bool direction)
{
  LPRLinkProbeHandler *lph = (LPRLinkProbeHandler*)element;

  if ( direction )
    return lph->lpSendHandler(buffer, size);
  else
    return lph->lpReceiveHandler(buffer, size);
}

int
LPRLinkProbeHandler::initialize(ErrorHandler *)
{
  _linkstat->registerHandler(this,BRN2_LINKSTAT_MINOR_TYPE_LPR,&handler);

  max_hosts = 128;
  known_links = new unsigned char[max_hosts * max_hosts];
  memset(known_links, 0, max_hosts * max_hosts);
  known_timestamps = new unsigned char[max_hosts];
  memset(known_timestamps, 0, max_hosts);

  known_hosts.push_back(EtherAddress(_linkstat->_dev->getEtherAddress()->data()));

  _seq = 0;

  return 0;
}

int
LPRLinkProbeHandler::lpSendHandler(char *buffer, int size)
{
  struct packed_link_header lprh;
  struct packed_link_info  lpri;

  if ( ! _active ) return 0;

  updateKnownHosts();
  updateLinksToMe();

  unsigned char *addr = new unsigned char[known_hosts.size() * 6];
  unsigned char *links = new unsigned char[known_hosts.size() * known_hosts.size()];
  unsigned char *timestamp = new unsigned char[known_hosts.size()];
  memset(links, 0, known_hosts.size() * known_hosts.size());

//  click_chatter("Nodes: %d",known_hosts.size());

  lprh._version = VERSION_BASE_MAC_LZW;
  lprh._no_nodes = known_hosts.size();
  lpri._header = &lprh;
  lpri._macs = (struct etheraddr *)addr;
  lpri._timestamp = timestamp;
  lpri._links = links;

  for ( int h = 0; h < known_hosts.size(); h++ ) {
    memcpy((char*)(&addr[h*6]), known_hosts[h].data(), 6);
//    click_chatter("Add host: %s",known_hosts[h].s().c_str());

    for ( int h2 = 0; h2 < known_hosts.size(); h2++ )
      links[h*known_hosts.size() + h2] = known_links[h*max_hosts + h2];  //cop links to linksfield

    timestamp[h] = known_timestamps[h];
  }

  int len = LPRProtocol::pack2(&lpri, (unsigned char*)buffer, size);

  if ( len == -1 ) {
    len = 0;
    BRN_WARN("To heavy");
  }

  delete[] addr;
  delete[] links;
  delete[] timestamp;

  BRN_DEBUG("Call send: %d space: %d",len,size);
  return len;
}

void
LPRLinkProbeHandler::updateKnownHosts()
{
  Vector<EtherAddress> hosts;
  int kh;

  _linkstat->get_neighbors(&hosts);

  for ( int nh = 0; nh < hosts.size(); nh++ ) {
    for ( kh = 0; kh < known_hosts.size(); kh++ )
      if ( known_hosts[kh] == hosts[nh] ) break;

    if ( kh == known_hosts.size() ) known_hosts.push_back(EtherAddress(hosts[nh].data()));
  }

  hosts.clear();
}

void
LPRLinkProbeHandler::updateLinksToMe()
{
  int kh;
  bool linkChanges;

  linkChanges = false;

  for ( kh = 1; kh < known_hosts.size(); kh++ ) {
    if ( known_links[kh] != ( _linkstat->get_rev_rate(&known_hosts[kh]) / 10 ) ) {
      known_links[kh] = _linkstat->get_rev_rate(&known_hosts[kh]) / 10;

      linkChanges = true;
    }
  }

  if ( linkChanges ) known_timestamps[0]++;//no need for modulo; char will switch from 255 to 0

}

int
LPRLinkProbeHandler::lpReceiveHandler(char *buffer, int size)
{
  int kh;
  EtherAddress ea;
  struct packed_link_info  *lpri;
  uint8_t *macs;

  if ( ! _active ) return size;

  BRN_DEBUG("Call receive %d",size);

  lpri = LPRProtocol::unpack2((unsigned char *)buffer, size);
  macs = (uint8_t*)lpri->_macs;

  /* Add the new hosts (got from received linkprobe) */
  for ( int h = 0; h < lpri->_header->_no_nodes; h++ ) {
    ea = EtherAddress(&(macs[h*6]));

    for ( kh = 0; kh < known_hosts.size(); kh++ )
      if ( known_hosts[kh] == ea ) break;

    if ( kh == known_hosts.size() ) known_hosts.push_back(EtherAddress(&(macs[6*h])));
  }

  int kh1, kh2;

  bool changes = false;
  Vector<BrnRateSize> brs;
  brs.push_back(BrnRateSize(2,1500)); //TODO: got real use value (linkstat)
  Vector<uint8_t> fwd;
  Vector<uint8_t> rev;

  for ( int nh1 = 0; nh1 < lpri->_header->_no_nodes; nh1++ ) {

    for ( kh1 = 0; kh1 < known_hosts.size(); kh1++ )
      if ( memcmp(&(macs[6*nh1]), known_hosts[kh1].data(), 6) == 0 ) break;

    if ( kh1 != known_hosts.size() ) {
      if ( (lpri->_timestamp[nh1] > known_timestamps[kh1]) || (known_timestamps[kh1]-lpri->_timestamp[nh1] >100) ) {  //TODO: handle overrun ( now: diff > 100)

        known_timestamps[kh1] = lpri->_timestamp[nh1];

        for ( int nh2 = 0; nh2 < lpri->_header->_no_nodes; nh2++ ) {

          for ( kh2 = 0; kh2 < known_hosts.size(); kh2++ )
            if ( memcmp(&(macs[6*nh2]), known_hosts[kh2].data(), 6) == 0 ) break;

          if ( kh2 != known_hosts.size() ) {

            known_links[kh1 * max_hosts + kh2] = lpri->_links[nh1*lpri->_header->_no_nodes + nh2];

            if ( ( memcmp(known_hosts[kh1].data(), _linkstat->_dev->getEtherAddress()->data(), 6) != 0 ) &&
                   ( memcmp(known_hosts[kh2].data(), _linkstat->_dev->getEtherAddress()->data(), 6) != 0 ) ) {

              if ( _etx_metric && ( known_links[kh1 * max_hosts + kh2] != 0 ) && ( known_links[kh2 * max_hosts + kh1] != 0 ) ) {
                fwd.push_back(10 * known_links[kh1 * max_hosts + kh2]);
                rev.push_back(10 * known_links[kh2 * max_hosts + kh1]);

                _etx_metric->update_link(known_hosts[kh1], known_hosts[kh2], brs, fwd, rev, _seq);

                fwd.clear();
                rev.clear();
              }
              changes = true;
            }
          }
        }
      }
    }
  }

  if ( changes ) _seq++;
  /* free the stuff */
  delete lpri->_header;
  delete[] lpri->_timestamp;
  delete[] lpri->_macs;
  delete[] lpri->_links;
  delete lpri;

  return size;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
LPRLinkProbeHandler::get_info()
{
  StringAccum sa;
  int kh;

  sa << "Nodes: " << known_hosts.size() << "\n";

  for( kh = 0; kh < known_hosts.size(); kh++ )
    sa << known_hosts[kh].unparse() << "\t" << (int)known_timestamps[kh] << "\n";

  sa << "Links: \n";

  for ( kh = 0; kh < known_hosts.size(); kh++ ) {
    for ( int kh2 = 0; kh2 < known_hosts.size(); kh2++ )
      sa << (int)known_links[kh * max_hosts + kh2] << " ";

    sa << "\n";
  }

  return sa.take_string();
}

static String
read_table_param(Element *e, void *)
{
  LPRLinkProbeHandler *fl = (LPRLinkProbeHandler *)e;

  return fl->get_info();
}

static String
read_active_param(Element *e, void *)
{
  LPRLinkProbeHandler *fl = (LPRLinkProbeHandler *)e;
  return String(fl->_active) + "\n";
}

static int 
write_active_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  LPRLinkProbeHandler *fl = (LPRLinkProbeHandler *)e;
  String s = cp_uncomment(in_s);
  bool active;
  if (!cp_bool(s, &active))
    return errh->error("active parameter must be an bool value (true/false)");
  fl->_active = active;
  return 0;
}

static String
read_debug_param(Element *e, void *)
{
  LPRLinkProbeHandler *fl = (LPRLinkProbeHandler *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  LPRLinkProbeHandler *fl = (LPRLinkProbeHandler *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
LPRLinkProbeHandler::add_handlers()
{
  add_read_handler("table", read_table_param, 0);
  add_read_handler("active", read_active_param, 0);
  add_read_handler("debug", read_debug_param, 0);

  add_write_handler("active", write_active_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LPRLinkProbeHandler)
