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
#include <iostream>
#include <sstream>

#include "brn_annrate_net.hh"
#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"

#define OUTPUT_FACTOR 54.0

CLICK_DECLS


BrnAnnRateNet::BrnAnnRateNet(String& net)
{
  std::string netWithNewLines = replaceNewLineSequenes(net);
  std::istringstream netStream(netWithNewLines);
  ann.load(netStream);
}

std::string BrnAnnRateNet::replaceNewLineSequenes(String s)
{
  std::string with_newlines(s.c_str());
  
  bool done = false;
	do
	{
		size_t pos = with_newlines.find("\\n");
		if(pos == std::string::npos)
			done = true;
		else
			with_newlines = with_newlines.replace(pos, 2, "\n");
	}while(!done);
  
  return(with_newlines);
}


uint8_t BrnAnnRateNet::classify(double input)
{
  const int num_rates = 8;
  const int rates[]   = {6, 9, 12, 18, 24, 36, 48, 54};

	double min_dst = 99.; 
  uint8_t result = rates[0];
	for (int i = 0; i < num_rates; i++)
	{
		float dst = abs(((float)rates[i]) - input * OUTPUT_FACTOR);
		if (dst <= min_dst) 
    {
      min_dst = dst; 
      result = rates[i];
    }
	}
	return result;
}

uint8_t BrnAnnRateNet::getRate(uint32_t num_neighbors, uint32_t num_hidden_nodes, uint8_t rssi)
{
  Eigen::VectorXd input(3);
  input << num_neighbors, num_hidden_nodes, rssi; // keep the order!
  
  Eigen::VectorXd output = ann(input);
  
  return(classify(output[0]));
}

String BrnAnnRateNet::test()
{
  StringAccum sa;
  sa << "<BrnAnnRateNetTest>\n";
  for(uint32_t neighbors = 3; neighbors < 9; neighbors++)
  {
    for(uint32_t hidden_nodes = 0; hidden_nodes < 4; hidden_nodes++)
    {
      for(uint8_t rssi = 6; rssi < 20; rssi = rssi + 3)
      {
        uint8_t rate = getRate(neighbors, hidden_nodes, rssi);
        sa << "  <test ";
        sa << "neighbors='" << neighbors << "'";
        sa << "hidden_nodes='" << hidden_nodes << "'";
        sa << "rssi='" << (int)rssi << "'";
        sa << "result_rate='" << (int)rate << "'";
        sa << " />\n";
      }
    }
  }
  sa << "</BrnAnnRateNetTest>\n";

  return(sa.take_string());
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(BrnAnnRateNet)
