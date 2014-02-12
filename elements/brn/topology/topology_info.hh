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
#ifndef TOPOLOGY_INFO_HH
#define TOPOLOGY_INFO_HH

#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class TopologyInfo : public BRNElement
{
public:
    class Bridge
    {
    public:
        EtherAddress node_a;
        EtherAddress node_b;
        uint32_t countDetection;

        Bridge(EtherAddress *a, EtherAddress *b)
        {
            node_a = EtherAddress(a->data());
            node_b = EtherAddress(b->data());
            countDetection = 1;
        }

        void incDetection()
        {
            countDetection++;
        }

        bool equals(EtherAddress *a, EtherAddress *b)
        {
            return (((memcmp(node_a.data(), a->data(), 6) == 0) && (memcmp(node_b.data(), b->data(), 6) == 0)) ||
                    ((memcmp(node_a.data(), b->data(), 6) == 0) && (memcmp(node_b.data(), a->data(), 6) == 0)));
        }
    };

    class ArticulationPoint
    {
    public:
        EtherAddress node;
        uint32_t countDetection;

        ArticulationPoint(EtherAddress *n)
        {
            node = EtherAddress(n->data());
            countDetection = 1;
        }

        void incDetection()
        {
            countDetection++;
        }

        bool equals(EtherAddress *a)
        {
            return memcmp(node.data(), a->data(), 6);
        }
    };
    
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();
    
    TopologyInfo();
    ~TopologyInfo();

    const char *class_name() const { return "TopologyInfo";}
    const char *processing() const { return AGNOSTIC;}

    const char *port_count() const
    {
        return "0/0";
    }

    void incNoDetection()
    {
        number_of_detections++;
    }
    
    void addBridge(EtherAddress *a, EtherAddress *b);
    void removeBridge(EtherAddress *a, EtherAddress *b);
    void addArticulationPoint(EtherAddress *a);
    void removeArticulationPoint(EtherAddress *a);
    Bridge* getBridge(EtherAddress *a, EtherAddress *b);
    ArticulationPoint *getArticulationPoint(EtherAddress *a);

    String topology_info(void);

private:
    Vector<Bridge*> _bridges;
    Vector<ArticulationPoint*> _artpoints;
    int number_of_detections;
};

CLICK_ENDDECLS
#endif
