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

// ALWAYS INCLUDE <click/config.h> FIRST
#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include "brn2_dsrclassifier.hh"
#include "brn2_dsrprotocol.hh"
#include "elements/brn2/brnprotocol/brn2_logger.hh"


CLICK_DECLS

BRN2DSRClassifier::BRN2DSRClassifier()
  : _debug(/*BrnLogger::DEFAULT*/0)
{
}

BRN2DSRClassifier::~BRN2DSRClassifier()
{
}

int
BRN2DSRClassifier::configure(Vector<String> &conf, ErrorHandler *)
{
  //set_noutputs(conf.size());
  for( int argno = 0 ; argno < conf.size(); argno++ )
  {
    String s = getNextArg(conf[argno]);
    if( s == "RAW" )
    {
      BRN_DEBUG("setting up RAW port");
      _msg_to_outport_map.insert(BRN_DSR_PAYLOADTYPE_RAW, argno);
    }
    else if ( s == "UDT" )
    {
      BRN_DEBUG("setting up UDT port");
      _msg_to_outport_map.insert(BRN_DSR_PAYLOADTYPE_UDT, argno);
    }
    else if ( s == "DHT" )
    {
      BRN_DEBUG("setting up DHT port");
      _msg_to_outport_map.insert(BRN_DSR_PAYLOADTYPE_DHT, argno);
    }
    else if ( s == "-" )
    {
      BRN_DEBUG("setting up - port");
      _msg_to_outport_map.insert(BRN_DSR_PAYLOADTYPE_REST, argno);
    }
  }// for
  
  return 0;
}

int
BRN2DSRClassifier::initialize(ErrorHandler *)
{
  return 0;
}


void 
BRN2DSRClassifier::push(int, Packet *p)
{

  uint8_t dsr_payloadtype = p->anno_u8(BRN_DSR_PAYLOADTYPE_KEY); //p->user_anno_c(BRN_DSR_PAYLOADTYPE_KEY);

  int portNum;
  if( _msg_to_outport_map.find_pair(dsr_payloadtype) )
  {
    portNum =
      _msg_to_outport_map.find(dsr_payloadtype);
  }
  else
  {
    portNum =
      _msg_to_outport_map.find(BRN_DSR_PAYLOADTYPE_REST);
  }

  switch(dsr_payloadtype)
  {
  case BRN_DSR_PAYLOADTYPE_RAW:
    BRN_DEBUG("[class] BRN_DSR_PAYLOADTYPE_RAW\n");
    break;
  case BRN_DSR_PAYLOADTYPE_UDT:
    BRN_DEBUG("[class] BRN_DSR_PAYLOADTYPE_UDT\n");
    break;
  case BRN_DSR_PAYLOADTYPE_DHT:
    BRN_DEBUG("[class] BRN_DSR_PAYLOADTYPE_DHT\n");
    break;
  default:
    BRN_DEBUG("[class] UNKNWON TYPE\n");
  }

  BRN_DEBUG("[class] portNum : %d\n", portNum);
  checked_output_push(portNum, p);
}

String 
BRN2DSRClassifier::getNextArg(const String &s)
{
  char buf[256];
  const char * arg = s.data();
  int currIndex = 0;
  
  while( *arg != ',' && *arg != ')' )
  {
    if( *arg == ' ')
    {
      arg++;
      continue;
    }
    buf[currIndex++] = *arg;
    arg++;
  }
  buf[currIndex] = '\0';
  
  return String(buf);
}

EXPORT_ELEMENT(BRN2DSRClassifier)
#include <click/bighashmap.cc>
CLICK_ENDDECLS 
