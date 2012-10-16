#ifndef CLICK_DHCPSUBNETLIST_HH
#define CLICK_DHCPSUBNETLIST_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <click/vector.hh>

CLICK_DECLS

//RESPONSIBLE: Robert

/*TODO:
  - handle to remove WifiInfos
  - check whether there is another WifiInfo  using the vlan-id
  - what can be private
  - rename to SubnetInfo or SubnetList
*/

class BRN2DHCPSubnetList : public Element {

 public:
  class DHCPSubnet {

   public:

    EtherAddress _me;
//    IPAddress _start_ip_range;  //TODO: was just for DHT

    IPAddress _net_address;
    IPAddress _subnet_mask;
    IPAddress _broadcast_address;

    IPAddress _router;             //IP
    IPAddress _server_ident;       //IP DHCP-Server
    IPAddress _name_server;        //IP

    String _domain_name;

    String _sname;           //servername
    String _bootfilename;    //name des Bootfiles

    String _time_offset;
    uint32_t _default_lease;

    uint16_t _vlan;

    DHCPSubnet() {
    }

    DHCPSubnet( EtherAddress me, IPAddress net_address, IPAddress subnet_mask, IPAddress broadcast_address,
                IPAddress router, IPAddress server_ident, IPAddress name_server, String domain_name, String sname,
                String bootfilename, String time_offset, uint32_t default_lease, uint16_t vlan ) {
       _me = me;
       _net_address = net_address;
       _subnet_mask = subnet_mask;
       _broadcast_address = broadcast_address;
       _router = router;                  //IP
       _server_ident = server_ident;      //IP DHCP-Server
       _name_server = name_server;        //IP
       _domain_name = domain_name;
       _sname = sname;                    //servername
       _bootfilename = bootfilename;      //name des Bootfiles
       _time_offset = time_offset;
       _default_lease = default_lease;
       _vlan = vlan;
     }
  };

  typedef Vector<DHCPSubnet> SubnetList;

  BRN2DHCPSubnetList();
  ~BRN2DHCPSubnetList();

  const char *class_name() const		{ return "BRN2DHCPSubnetList"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();

  int countSubnets() { return _subnet_list.size(); }
  DHCPSubnet *getSubnet(int index);
  DHCPSubnet *getSubnetByVlanID(int id);

  SubnetList _subnet_list;

  int _debug;
};

CLICK_ENDDECLS
#endif
