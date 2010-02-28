#ifndef CLICK_BRN2VLAN_HH
#define CLICK_BRN2VLAN_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <click/vector.hh>
#include "../wifi/brn2_wirelessinfolist.hh"
#include "../services/dhcp/dhcpsubnetlist.hh"
#include "elements/brn2/vlan/brn2vlantable.hh"

CLICK_DECLS

//RESPONSIBLE: Robert

/*TODO:
*/

class BRN2VLAN : public Element {

 public:
  class VlanInfo {

   public:

    VlanInfo() {
    }

  };

  typedef Vector<VlanInfo> VlanInfoList;

  BRN2VLAN();
  ~BRN2VLAN();

  const char *class_name() const		{ return "BRN2VLAN"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();

  BRN2WirelessInfoList *_wifiinfolist;
  BRN2DHCPSubnetList *_dhcpsubnetlist;
  BRN2VLANTable *_vlantable;
  int _debug;
};

CLICK_ENDDECLS
#endif
