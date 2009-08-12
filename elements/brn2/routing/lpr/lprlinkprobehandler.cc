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
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"
#include "lprlinkprobehandler.hh"
#include "lprprotocol.hh"



CLICK_DECLS

LPRLinkProbeHandler::LPRLinkProbeHandler()
  :  _debug(BrnLogger::DEFAULT)
{
}

LPRLinkProbeHandler::~LPRLinkProbeHandler()
{
}

int
LPRLinkProbeHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LINKTABLE", cpkP+cpkM , cpElement, &_lt,
      "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
handler(void *element, char *buffer, int size, bool direction)
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
  _linkstat->registerHandler(this,0,&handler);
  return 0;
}

int
LPRLinkProbeHandler::lpSendHandler(char *buffer, int size)
{
  Vector<EtherAddress> hosts;
  struct packed_link_header lprh;
  struct packed_link_info  lpri;
  _linkstat->get_neighbors(&hosts);

  unsigned char *addr = new unsigned char[(1 + hosts.size()) * 6];
  unsigned char *links = new unsigned char[(1 + hosts.size()) * (1 + hosts.size())];
  unsigned char *timestamp = new unsigned char[(1 + hosts.size())];
  memset(links,0,hosts.size() * hosts.size());

  click_chatter("Nodes: %d",hosts.size());

  lprh._version = 0;
  lprh._no_nodes = hosts.size() + 1;
  lpri._header = &lprh;
  lpri._macs = (struct etheraddr *)addr;
  lpri._timestamp = timestamp;
  lpri._links = links;

  memcpy((char*)(&addr[0]),_linkstat->_me->getEtherAddress()->data(),6);

  click_chatter("A: %s",_linkstat->_me->getEtherAddress()->s().c_str());
  for ( int h = 0; h < hosts.size(); h++ ) {
    memcpy((char*)(&addr[(h + 1)*6]),hosts[h].data(),6);
    click_chatter("A: %s",hosts[h].s().c_str());
    //get_host_metric_to_me(EtherAddress s)
    //get_host_metric_from_me(EtherAddress s)
  }

  int len = LPRProtocol::pack(&lpri, (unsigned char*)buffer, size);

  delete addr;
  delete links;
  delete timestamp;

  click_chatter("Call send: %d",len);
  return len;
}

int
LPRLinkProbeHandler::lpReceiveHandler(char *buffer, int size)
{
  click_chatter("Call receive %d",size);
  return size;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

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
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LPRLinkProbeHandler)
