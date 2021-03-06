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
#include <click/etheraddress.hh>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/type_traits.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "cycle.hh"
#include "elements/grid/ackresponder.hh"
#include "config.hh"

CLICK_DECLS

DibadawnConfig::DibadawnConfig()
:   debugLevel(0), 
    votingRule(0),
    numOfVotingSets(5),
    useLinkStatistic(false),
    trustThreshold(0.90),
    probabilityForBridgeAtNet(0.5),
    probabilityForAAPAtNet(0.5)
{
  useOriginForwardDelay = true;
  maxHops = 14;
  maxTraversalTimeMs = 52, 
  maxJitter = maxTraversalTimeMs / 4;
  isPrintResults = false;
  ;
}

const char* DibadawnConfig::thisNodeAsCstr()
{
  return(thisNode.unparse().c_str());
}

String DibadawnConfig::asString()
{
  StringAccum sa;
  sa << "<dibadawn_config time='" << Timestamp::now() << "' >\n";
  
  sa << "\t<NODEIDENTITY value='" << thisNode.unparse() << "' />\n";
  sa << "\t<DEBUG value='" << debugLevel << "' />\n";
  sa << "\t<ORIGINDELAY value='" << useOriginForwardDelay << "' />\n";
  sa << "\t<MAXTRAVERSALTIMEMS value='" << maxTraversalTimeMs << "' />\n";
  sa << "\t<MAXHOPS value='" << maxHops << "' />\n";
  sa << "\t<MAXJITTERMS value='" << maxJitter << "' />\n";
  sa << "\t<VOTINGRULE value='" << votingRule << "' />\n";
  sa << "\t<votingRules>\n";
  sa << "\t\t<rule number='0' name='last run' />\n";
  sa << "\t\t<rule number='1' name='Unanimous' />\n";
  sa << "\t\t<rule number='2' name='Plain majority' />\n";
  sa << "\t\t<rule number='3' name='Single-for' />\n";
  sa << "\t\t<rule number='4' name='Intelligent majority:' />\n";
  sa << "\t\t<rule number='5' name='Trusted non-bridge rule' />\n";
  sa << "\t\t<rule number='6' name='Weighted rule' />\n";
  sa << "\t</votingRules>\n";
  
  sa << "</dibadawn_config>\n";

  return sa.take_string();
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnConfig)
