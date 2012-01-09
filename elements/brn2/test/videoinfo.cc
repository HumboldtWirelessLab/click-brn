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
 * videoinfo.{cc,hh}
 */

#include <click/config.h>
#include <click/confparse.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "videoinfo.hh"

CLICK_DECLS

VideoInfo::VideoInfo()
{
  BRNElement::init();
}

VideoInfo::~VideoInfo()
{
  _vil.clear();
}

int
VideoInfo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

void
VideoInfo::push(int port, Packet *p)
{

  String videoinfo = String(p->data());

  _vil.push_back(VideoInfoData(videoinfo));

  if ( _vil.size() > 1000 ) _vil.erase(_vil.begin());

  p->kill();
}

/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

String
VideoInfo::read_info()
{
  Timestamp now = Timestamp::now();

  StringAccum sa;

  sa << "<videoinfo id='" << BRN_NODE_NAME << "'>\n";
  for ( int i = 0; i < _vil.size(); i++) {
    sa << "\t<info time='" << _vil[i]._time.unparse() << "' value='" <<  _vil[i]._data << "' />\n";
  }
  sa << "</videoinfo>\n";

  _vil.clear();
  return sa.take_string();

}

static String
read_handler(Element *e, void */*thunk*/)
{
  VideoInfo *vi = (VideoInfo*)e;

  return vi->read_info();
}

void
VideoInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("info", read_handler);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(VideoInfo)
