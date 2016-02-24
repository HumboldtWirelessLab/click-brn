#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/vector.hh>
#include "dhcpsubnetlist.hh"
#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

BRN2DHCPSubnetList::BRN2DHCPSubnetList()
 : _debug(0)
{
}

BRN2DHCPSubnetList::~BRN2DHCPSubnetList()
{
}

int
BRN2DHCPSubnetList::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _debug = false;

  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

BRN2DHCPSubnetList::DHCPSubnet *
BRN2DHCPSubnetList::getSubnet(int index)
{
  if ( index < _subnet_list.size() )
    return &(_subnet_list[index]);

  return NULL;
}

BRN2DHCPSubnetList::DHCPSubnet *
BRN2DHCPSubnetList::getSubnetByVlanID(int id)
{
  BRN_DEBUG("Size of SubnetList: %d",_subnet_list.size());

  for ( int i = 0; i < _subnet_list.size(); i++ ) {
    BRN2DHCPSubnetList::DHCPSubnet *sn = &(_subnet_list[i]);

    BRN_DEBUG("ID: %d",sn->_vlan);

    if ( sn->_vlan == id ) return sn;
  }

  return NULL;
}


enum {
  H_DEBUG,
  H_READ,
  H_INSERT,
  H_REMOVE };

static String
BRN2DHCPSubnetList_read_param(Element *e, void *thunk)
{
  BRN2DHCPSubnetList *snl = reinterpret_cast<BRN2DHCPSubnetList *>(e);
  switch ((uintptr_t) thunk) {
    case H_DEBUG: {
      return String(snl->_debug) + "\n";
    }
    case H_READ: {
      StringAccum sa;
      sa << "<dhcp_subnetlist>\n";
      for ( int i = 0; i < snl->_subnet_list.size(); i++ ) {
        BRN2DHCPSubnetList::DHCPSubnet sn = snl->_subnet_list[i];
        sa << "<name=\"" << sn._me.unparse() << "\" ";
        sa << "ipnet=\"" << sn._net_address.unparse() << "\" ";
        sa << "domainname=\"" << sn._domain_name << "\" ";
        sa << "vlan=\"" << (uint32_t)sn._vlan << "\" ";
        sa << ">\n";
      }
      sa << "</dhcp_subnetlist>\n";

      return sa.take_string();
   }
   default:
    return String();
  }
}

static int
BRN2DHCPSubnetList_write_param(const String &in_s, Element *e, void *vparam,
                                 ErrorHandler *errh)
{
  BRN2DHCPSubnetList *f = reinterpret_cast<BRN2DHCPSubnetList *>(e);
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {
      bool debug;
      if (!cp_bool(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
    case H_INSERT: {
      Vector<String> args;
      cp_spacevec(s, args);

      EtherAddress _me;

      IPAddress _net_address;
      IPAddress _subnet_mask;
      IPAddress _broadcast_address;

      IPAddress _router;             //IP
      IPAddress _server_ident;       //IP DHCP-Server
      IPAddress _name_server;        //IP

      String _domain_name;

      String _sname;                 //servername
      String _bootfilename;          //name des Bootfiles

      String _time_offset;
      uint32_t _default_lease;

      uint32_t _vlan;

      cp_ethernet_address(args[0], &_me);
      cp_ip_address(args[1],&_net_address);
      cp_ip_address(args[2],&_subnet_mask);
      cp_ip_address(args[3],&_broadcast_address);
      cp_ip_address(args[4],&_router);
      cp_ip_address(args[5],&_server_ident);
      cp_ip_address(args[6],&_name_server);
      _domain_name = args[7];
      _sname = args[8];
      _bootfilename = args[9];
      _time_offset = args[10];
      cp_integer(args[11], &_default_lease);
      cp_integer(args[12], &_vlan);

      f->_subnet_list.push_back(BRN2DHCPSubnetList::DHCPSubnet(_me, _net_address, _subnet_mask, _broadcast_address,
                                                                    _router, _server_ident, _name_server,
                                                                    _domain_name, _sname, _bootfilename,
                                                                    _time_offset, _default_lease, _vlan ));
      break;
    }
  }

  return 0;
}

void
BRN2DHCPSubnetList::add_handlers()
{
  add_read_handler("debug", BRN2DHCPSubnetList_read_param, (void *) H_DEBUG);
  add_read_handler("info", BRN2DHCPSubnetList_read_param, (void *) H_READ);

  add_write_handler("debug", BRN2DHCPSubnetList_write_param, (void *) H_DEBUG);
  add_write_handler("insert", BRN2DHCPSubnetList_write_param, (void *) H_INSERT);
}

#include <click/vector.cc>
template class Vector<BRN2DHCPSubnetList::DHCPSubnet>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DHCPSubnetList)

