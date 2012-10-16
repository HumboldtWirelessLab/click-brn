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

#ifndef CLICK_HIDDENNODEDETECTION_HH
#define CLICK_HIDDENNODEDETECTION_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include <elements/brn2/brn2.h>
#include "elements/brn2/wifi/brnwifi.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"

CLICK_DECLS

/*
=c
HiddenNodeDetection()

=d

=a
*/

class HiddenNodeDetection : public BRNElement {
    
    public:
        
        class NodeInfo;
        typedef HashMap<EtherAddress, NodeInfo*> NodeInfoTable;
        typedef NodeInfoTable::const_iterator NodeInfoTableIter;
        
        class NodeInfo {
        public:
        
            Timestamp _last_notice;
            bool _neighbour;

            NodeInfoTable _links_to;
            HashMap<EtherAddress, Timestamp> _links_usage;

            NodeInfo(): _neighbour(false) {
                
                _last_notice = Timestamp::now();
            }

            inline void add_link(EtherAddress ea, NodeInfo *ni) {
                
                if ( ! _links_to.findp(ea) ) _links_to.insert(ea,ni);
                
                _links_usage.insert(ea,Timestamp::now());
            }

            inline void update() {
                
                _last_notice = Timestamp::now();
            }
            
            inline HashMap<EtherAddress, NodeInfo*> get_links_to() {
                
                return _links_to;
            }
            
            inline HashMap<EtherAddress, Timestamp> get_links_usage() {
                
                return _links_usage;
            }
        };

  

    HiddenNodeDetection();
    ~HiddenNodeDetection();

    const char *class_name() const  { return "HiddenNodeDetection"; }
    const char *processing() const  { return PUSH; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);
    int initialize(ErrorHandler *);

    void add_handlers();

    void push(int, Packet *p);

    String stats_handler(int mode);
    
    inline HashMap<EtherAddress, NodeInfo*> get_nodeinfo_table() {
        
        return _nodeinfo_tab;
    }
    
    inline bool has_neighbours(EtherAddress address) {
        
        if (_nodeinfo_tab.find(address) != NULL) {
            return _nodeinfo_tab.find(address)->_neighbour;
        } else {
            return false;   
        }
    }
    
  private:

    BRN2Device *_device;

    NodeInfoTable _nodeinfo_tab;

    EtherAddress _last_data_dst;
    uint16_t _last_data_seq;

};

CLICK_ENDDECLS
#endif
