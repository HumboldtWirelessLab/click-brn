#include <click/config.h>

#include "nodeidentity.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include <click/standard/addressinfo.hh>


CLICK_DECLS

BRN2NodeIdentity::BRN2NodeIdentity()
  : _debug(BrnLogger::DEFAULT)
{
}

BRN2NodeIdentity::~BRN2NodeIdentity()
{
}

int
BRN2NodeIdentity::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (conf.size() != noutputs())
    return errh->error("need %d arguments, one per output port", noutputs());

  int before = errh->nerrors();

  for (int slot = 0; slot < conf.size(); slot++) {
    int i = 0;
    int len = conf[slot].length();
    const char *s = conf[slot].data();

    int slot_branch = _exprs.size();

  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpString, "#1 device (wlan)", &_dev_wlan_name,
      cpString, "#2 device (vlan0)", &_dev_vlan0_name,
      cpString, "#3 device (vlan1)", &_dev_vlan1_name,
      cpElement, "link table", &_link_table,
      cpKeywords,
      "ADDR_WLAN", cpEtherAddress, "#1 device address (wlan)", &me_wlan,
      "ADDR_VLAN0", cpEtherAddress, "#2 device address (vlan0)", &me_vlan0,
      "ADDR_VLAN1", cpEtherAddress, "#3 device address (vlan1)", &me_vlan1,
                cpEnd) < 0)
    return -1;


  return 0;
}

int
NodeIdentity::initialize(ErrorHandler *)
{
  if (_link_table) {
    //vlan0 and vlan1 have the same ether address
    _link_table->update_both_links(*_me_wlan, *_me_vlan0, 0, 0, 1, true);
  }

  return 0;
}

/* returns true if the given ethernet address belongs to this node (e.g. wlan-dev)*/
bool
NodeIdentity::isIdentical(EtherAddress *e)
{
  if (!_me_wlan || !_me_vlan0)
    return false;

  if ( (*e == *_me_wlan) || (*e == *_me_vlan0) ) {
    return true;
  } else {
    return false;
  }
}

EtherAddress *
NodeIdentity::getMyWirelessAddress()
{
  return _me_wlan;
}

EtherAddress *
NodeIdentity::getMyVlan0Address()
{
  return _me_vlan0;
}

EtherAddress *
NodeIdentity::getMyVlan1Address()
{
  return _me_vlan1;
}

BrnLinkTable* 
NodeIdentity::get_link_table()
{
  return (_link_table);
}

//BRNNEW
/*int
NodeIdentity::findOwnIdentity(const RouteQuerierRoute &r)
{
  for (int i=0; i<r.size(); i++) {
    if ( (r[i].ether() == *_me_wlan) || (r[i].ether() == *_me_vlan0) )
      return i;
  }
  return -1;
}
*/
EtherAddress *
NodeIdentity::getEthernetAddress(String dev_name)
{
  if (dev_name == _dev_wlan_name) {
    return _me_wlan;
  } else if (dev_name == _dev_vlan0_name) {
    return _me_vlan0;
  } else if (dev_name == _dev_vlan1_name) {
    return _me_vlan1; //vlan0 and vlan1 have the same ethernet address
  }

  return (NULL);
}

Packet *
NodeIdentity::skipInMemoryHops(Packet *p_in)
{
  BRN_DEBUG(" * calling NodeIdentity::skipInMemoryHops().");

  click_brn_dsr *brn_dsr =
        (click_brn_dsr *)(p_in->data() + sizeof(click_brn));

  int index = brn_dsr->dsr_hop_count - brn_dsr->dsr_segsleft;

  BRN_DEBUG(" * index = %d brn_dsr->dsr_hop_count = %d", index, brn_dsr->dsr_hop_count);

  if ( (brn_dsr->dsr_hop_count == 0) || (brn_dsr->dsr_segsleft == 0) )
    return p_in;

  assert(index >= 0 && index < BRN_DSR_MAX_HOP_COUNT);
  assert(index <= brn_dsr->dsr_hop_count);

  EtherAddress dest(brn_dsr->addr[index].hw.data);

  BRN_DEBUG(" * test next hop: %s", dest.s().c_str());
  BRN_DEBUG(" * HC, Index, SL. %d %d %d", brn_dsr->dsr_hop_count, index, brn_dsr->dsr_segsleft);

  while (isIdentical(&dest) && (brn_dsr->dsr_segsleft > 0)) {
    BRN_DEBUG(" * skip next hop: %s", dest.s().c_str());
    brn_dsr->dsr_segsleft--;
    index = brn_dsr->dsr_hop_count - brn_dsr->dsr_segsleft;

    dest = EtherAddress(brn_dsr->addr[index].hw.data);
    BRN_DEBUG(" * check next hop (maybe skip required): %s", dest.s().c_str());
  }

  if (index == brn_dsr->dsr_hop_count) {// no hops left; use final dst
    BRN_DEBUG(" * using final dst. %d %d", brn_dsr->dsr_hop_count, index);
//BRNNEW    p_in->set_dst_ether_anno(EtherAddress(brn_dsr->dsr_dst.data));
  } else {
//BRNNEW    p_in->set_dst_ether_anno(EtherAddress(brn_dsr->addr[index].hw.data));
  }

  return p_in;
}

const String &
NodeIdentity::getWlan0DeviceName()
{
  return _dev_wlan_name;
}

const String &
NodeIdentity::getVlan0DeviceName()
{
  return _dev_vlan0_name;
}

const String &
NodeIdentity::getVlan1DeviceName()
{
  return _dev_vlan1_name;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  NodeIdentity *id = (NodeIdentity *)e;
  return String(id->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  NodeIdentity *id = (NodeIdentity *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  id->_debug = debug;
  return 0;
}

void
NodeIdentity::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/vector.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(NodeIdentity)
